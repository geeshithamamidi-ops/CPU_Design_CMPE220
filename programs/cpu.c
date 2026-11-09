/* ============================================================================
 *  cpu.c  —  Team 02 CPU simulator core: fetch -> decode -> execute
 *  Owner: Anupama Singh
 * ----------------------------------------------------------------------------
 *  Follows docs/isa/FINALIZED_ISA.md exactly:
 *
 *  Flags (column "Flags set" in the opcode table)
 *    ALU, ADDI           -> Z N C V
 *    LDI, LD, LDR, MOV   -> Z N        (C and V keep their old values)
 *    all others          -> unchanged
 *
 *  Stack: full-descending. SP starts at 0xFF and points at the item most
 *  recently pushed (SP == 0xFF means empty).
 *    PUSH / CALL :  SP <- SP - 1 ;  RAM[SP] <- value
 *    POP  / RET  :  value <- RAM[SP] ;  SP <- SP + 1
 *
 *  Immediates: imm8 is unsigned (0..255) except for ADDI, where it is
 *  signed two's complement (-128..127).
 * ==========================================================================*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "assembler.h"
#include "cpu.h"

/* Record an error message in cpu->error; always returns -1. */
static int fail(CPU *cpu, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cpu->error, CPU_ERRLEN, fmt, ap);
    va_end(ap);
    return -1;
}

void cpu_reset(CPU *cpu) {
    memset(cpu, 0, sizeof *cpu);
    cpu->pc = TEXT_BASE;
    cpu->sp = SP_INIT;
}

int cpu_load(CPU *cpu, const char *path) {
    cpu_reset(cpu);
    int n = assemble(path, cpu->ram, RAM_WORDS);  /* Saima's assembler */
    if (n < 0) return fail(cpu, "assembly of '%s' failed", path);
    if (n > TEXT_END + 1) return fail(cpu, "program has %d words; text segment holds 64", n);
    cpu->text_words = n;

    /* Watch every data address the program loads from or stores to, so the
     * memory dump shows those cells from the very first step. */
    for (int a = 0; a < n; a++) {
        unsigned op = cpu->ram[a] >> 12;
        if (op == OP_LD || op == OP_ST) cpu->watch[cpu->ram[a] & 0xFF] = 1;
    }
    return n;
}

/* ---------------------------------------------------------------------------
 *  Memory and stack helpers — every access is bounds-checked
 * -------------------------------------------------------------------------*/
static int mem_read(CPU *cpu, int addr, uint16_t *out) {
    if (addr < 0 || addr >= RAM_WORDS) return fail(cpu, "read from invalid address 0x%X", addr);
    *out = cpu->ram[addr];
    return 0;
}

static int mem_write(CPU *cpu, int addr, uint16_t value, StepInfo *info) {
    if (addr < 0 || addr >= RAM_WORDS) return fail(cpu, "write to invalid address 0x%X", addr);
    if (cpu->ram[addr] != value) info->changed[addr] = 1;
    cpu->ram[addr] = value;
    cpu->watch[addr] = 1;
    return 0;
}

static int push(CPU *cpu, uint16_t value, StepInfo *info) {
    if (cpu->sp - 1 < STACK_BASE) return fail(cpu, "stack overflow (SP would leave 0xC0-0xFF)");
    cpu->sp--;
    return mem_write(cpu, cpu->sp, value, info);
}

static int pop(CPU *cpu, uint16_t *out) {
    if (cpu->sp >= SP_INIT) return fail(cpu, "stack underflow (POP/RET on an empty stack)");
    if (mem_read(cpu, cpu->sp, out)) return -1;
    cpu->sp++;
    return 0;
}

/* ---------------------------------------------------------------------------
 *  Flags
 * -------------------------------------------------------------------------*/
static int to_signed16(uint16_t v) { return (v & 0x8000) ? (int)v - 0x10000 : (int)v; }

/* Set Z and N from a 16-bit result; leave C and V alone. */
static void set_zn(CPU *cpu, uint16_t v) {
    cpu->flags &= (uint8_t)~(F_Z | F_N);
    if (v == 0) cpu->flags |= F_Z;
    if (v & 0x8000) cpu->flags |= F_N;
}

/* Set all four flags. */
static void set_zncv(CPU *cpu, uint16_t v, int carry, int overflow) {
    cpu->flags = 0;
    set_zn(cpu, v);
    if (carry) cpu->flags |= F_C;
    if (overflow) cpu->flags |= F_V;
}

/* ---------------------------------------------------------------------------
 *  ALU: compute a <fn> b, set Z N C V, return the 16-bit result in *out
 * -------------------------------------------------------------------------*/
static int alu(CPU *cpu, unsigned fn, uint16_t a, uint16_t b, uint16_t *out) {
    long sa = to_signed16(a), sb = to_signed16(b);   /* signed views, for V */
    unsigned long full = 0;                            /* unsigned, for C     */
    int carry = 0, overflow = 0;

    switch (fn) {
    case FN_ADD:
        full = (unsigned long)a + b;
        carry = full > 0xFFFF;
        overflow = sa + sb > 32767 || sa + sb < -32768;
        break;
    case FN_SUB:
    case FN_CMP:                                       /* CMP = SUB, no write */
        full = (uint16_t)(a - b);
        carry = a < b;                                 /* borrow              */
        overflow = sa - sb > 32767 || sa - sb < -32768;
        break;
    case FN_MUL:
        full = (unsigned long)a * b;
        carry = full > 0xFFFF;                         /* needs > 16 bits     */
        overflow = sa * sb > 32767 || sa * sb < -32768;
        break;
    case FN_DIV:
        if (b == 0) return fail(cpu, "division by zero");
        full = a / b;                                  /* unsigned division   */
        break;
    case FN_AND: full = a & b;             break;
    case FN_OR:  full = a | b;             break;
    case FN_NOT: full = (uint16_t)~b;      break;     /* rd = ~rs            */
    case FN_SHL:
        full = (unsigned long)a << (b & 0xF);
        carry = full > 0xFFFF;                         /* a 1 bit shifted out */
        break;
    case FN_SHR: full = a >> (b & 0xF);    break;
    default:
        return fail(cpu, "unknown ALU function code 0x%X", fn);
    }

    *out = (uint16_t)(full & 0xFFFF);
    set_zncv(cpu, *out, carry, overflow);
    return 0;
}

/* ---------------------------------------------------------------------------
 *  One instruction cycle
 * -------------------------------------------------------------------------*/
int cpu_step(CPU *cpu, StepInfo *info) {
    memset(info, 0, sizeof *info);
    if (cpu->halted) return fail(cpu, "CPU is halted");

    /* 1. FETCH ------------------------------------------------------------*/
    int addr = cpu->pc;
    if (addr < TEXT_BASE || addr > TEXT_END)
        return fail(cpu, "PC 0x%02X is outside the text segment", addr);
    cpu->ir = cpu->ram[addr];
    cpu->pc = addr + 1;

    /* 2. DECODE -----------------------------------------------------------*/
    unsigned op   = (cpu->ir >> 12) & 0xF;
    unsigned rd   = (cpu->ir >> 8) & 0xF;
    unsigned rs   = (cpu->ir >> 4) & 0xF;
    unsigned fn   = cpu->ir & 0xF;            /* fn, or off4 for LDR/STR    */
    unsigned imm8 = cpu->ir & 0xFF;           /* immediate or address       */
    uint16_t tmp  = 0;

    /* 3. EXECUTE ----------------------------------------------------------*/
    switch (op) {
    case OP_HALT:
        cpu->halted = 1;
        break;

    case OP_ALU:                              /* rd = rd <fn> rs            */
        if (alu(cpu, fn, cpu->r[rd], cpu->r[rs], &tmp)) return -1;
        if (fn != FN_CMP) cpu->r[rd] = tmp;   /* CMP only sets the flags    */
        break;

    case OP_LDI:                              /* rd = imm8 (unsigned)       */
        cpu->r[rd] = (uint16_t)imm8;
        set_zn(cpu, cpu->r[rd]);
        break;

    case OP_LD:                               /* rd = RAM[addr8]            */
        if (mem_read(cpu, (int)imm8, &cpu->r[rd])) return -1;
        set_zn(cpu, cpu->r[rd]);
        break;

    case OP_ST:                               /* RAM[addr8] = register      */
        if (mem_write(cpu, (int)imm8, cpu->r[rd], info)) return -1;
        break;

    case OP_LDR:                              /* rd = RAM[rs + off4]        */
        if (mem_read(cpu, cpu->r[rs] + (int)fn, &cpu->r[rd])) return -1;
        set_zn(cpu, cpu->r[rd]);
        break;

    case OP_STR:                              /* RAM[rs + off4] = rd        */
        if (mem_write(cpu, cpu->r[rs] + (int)fn, cpu->r[rd], info)) return -1;
        break;

    case OP_MOV:                              /* rd = rs                    */
        cpu->r[rd] = cpu->r[rs];
        set_zn(cpu, cpu->r[rd]);
        break;

    case OP_ADDI: {                           /* rd = rd + imm8 (signed)    */
        int simm = (imm8 & 0x80) ? (int)imm8 - 0x100 : (int)imm8;
        if (alu(cpu, FN_ADD, cpu->r[rd], (uint16_t)simm, &cpu->r[rd])) return -1;
        break;
    }

    case OP_JMP:  cpu->pc = (int)imm8;                                  break;
    case OP_JZ:   if (cpu_flag(cpu, F_Z))  cpu->pc = (int)imm8;         break;
    case OP_JNZ:  if (!cpu_flag(cpu, F_Z)) cpu->pc = (int)imm8;         break;
    case OP_JN:   if (cpu_flag(cpu, F_N))  cpu->pc = (int)imm8;         break;

    case OP_CALL:                             /* push return address, jump  */
        if (push(cpu, (uint16_t)cpu->pc, info)) return -1;
        cpu->pc = (int)imm8;
        break;

    case OP_RET:                              /* pop return address         */
        if (pop(cpu, &tmp)) return -1;
        cpu->pc = tmp;
        break;

    case OP_STK:                              /* fn 0 = PUSH, fn 1 = POP    */
        if (fn == STK_PUSH) {
            if (push(cpu, cpu->r[rd], info)) return -1;
        } else if (fn == STK_POP) {
            if (pop(cpu, &cpu->r[rd])) return -1;
        } else {
            return fail(cpu, "bad PUSH/POP selector %u", fn);
        }
        break;
    }

    cpu->steps++;
    info->step = cpu->steps;
    info->addr = addr;
    info->word = cpu->ir;
    return 0;
}

int cpu_run(CPU *cpu, long max_steps, StepHook hook) {
    StepInfo info;                            /* filled in by every step    */
    while (!cpu->halted) {
        if (cpu->steps >= max_steps)
            return fail(cpu, "stopped after %ld steps (infinite loop?)", max_steps);
        if (cpu_step(cpu, &info)) return -1;
        if (hook) hook(cpu, &info);
    }
    return 0;
}
