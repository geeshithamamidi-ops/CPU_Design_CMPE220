/* ============================================================================
 *  dump.c  —  memory / register printouts for the CPU simulator (see dump.h)
 * ==========================================================================*/
#include <stdio.h>

#include "dump.h"

#define LINE "=============================================================================="
#define THIN "------------------------------------------------------------------------"

static const char *ALU_NAME[] = {"ADD", "SUB", "MUL", "DIV", "AND",
                                 "OR",  "NOT", "SHL", "SHR", "CMP"};

void disassemble(uint16_t w, char *out, size_t size) {
    unsigned op = (w >> 12) & 0xF, rd = (w >> 8) & 0xF, rs = (w >> 4) & 0xF;
    unsigned fn = w & 0xF, imm = w & 0xFF;
    int simm = (imm & 0x80) ? (int)imm - 0x100 : (int)imm;

    switch (op) {
    case OP_HALT: snprintf(out, size, "HALT"); break;
    case OP_ALU:
        if (fn <= FN_CMP) snprintf(out, size, "ALU R%u, R%u, %s", rd, rs, ALU_NAME[fn]);
        else              snprintf(out, size, "ALU ??? (fn %u)", fn);
        break;
    case OP_LDI:  snprintf(out, size, "LDI R%u, %u", rd, imm); break;
    case OP_LD:   snprintf(out, size, "LD R%u, [0x%02X]", rd, imm); break;
    case OP_ST:   snprintf(out, size, "ST 0x%02X, R%u", imm, rd); break;
    case OP_LDR:  snprintf(out, size, "LDR R%u, [R%u+%u]", rd, rs, fn); break;
    case OP_STR:  snprintf(out, size, "STR [R%u+%u], R%u", rs, fn, rd); break;
    case OP_MOV:  snprintf(out, size, "MOV R%u, R%u", rd, rs); break;
    case OP_ADDI: snprintf(out, size, "ADDI R%u, %d", rd, simm); break;
    case OP_JMP:  snprintf(out, size, "JMP 0x%02X", imm); break;
    case OP_JZ:   snprintf(out, size, "JZ 0x%02X", imm); break;
    case OP_JNZ:  snprintf(out, size, "JNZ 0x%02X", imm); break;
    case OP_JN:   snprintf(out, size, "JN 0x%02X", imm); break;
    case OP_CALL: snprintf(out, size, "CALL 0x%02X", imm); break;
    case OP_RET:  snprintf(out, size, "RET"); break;
    default:      snprintf(out, size, fn == STK_PUSH ? "PUSH R%u" : "POP R%u", rd); break;
    }
}

static const char *segment(int a) {
    if (a < DATA_BASE)  return "text";
    if (a < HEAP_BASE)  return "data";
    if (a < STACK_BASE) return "heap";
    return "stack";
}

/* "1001 1101 1000 0000" */
static void binary16(uint16_t w, char *out) {
    int k = 0;
    for (int bit = 15; bit >= 0; bit--) {
        out[k++] = (char)('0' + ((w >> bit) & 1));
        if (bit % 4 == 0 && bit) out[k++] = ' ';
    }
    out[k] = '\0';
}

/* Print the RAM block. `changed` may be NULL. */
static void print_memory(const CPU *cpu, const uint8_t *changed) {
    char bin[24], text[40];
    printf("     %-5s %-6s %-7s %-20s %s\n", "Addr", "Seg", "Hex", "Binary", "Contents");
    printf(" %s\n", THIN);
    for (int a = 0; a < RAM_WORDS; a++) {
        int is_text  = a < cpu->text_words;
        int is_data  = cpu->watch[a] && a >= DATA_BASE && a < STACK_BASE;
        int is_stack = a >= cpu->sp && a < SP_INIT;
        if (!is_text && !is_data && !is_stack) continue;

        uint16_t w = cpu->ram[a];
        const char *mark = "";
        if (a == cpu->pc && !cpu->halted)        mark = "PC>";
        else if (a == cpu->sp && cpu->sp < SP_INIT) mark = "SP>";

        if (is_text)
            disassemble(w, text, sizeof text);                 /* instruction */
        else if (w & 0x8000)
            snprintf(text, sizeof text, "%u  (signed %d)", w, (int)w - 0x10000);
        else
            snprintf(text, sizeof text, "%u", w);              /* value       */

        binary16(w, bin);
        printf(" %-4s0x%02X  %-6s 0x%04X  %s  %s%s\n", mark, a, segment(a), w, bin, text,
               (changed && changed[a]) ? "  *" : "");
    }
}

/* Register values as of the previous printout, so dump_step() can mark
 * the registers that the instruction just executed changed.            */
static uint16_t prev_r[16];
static uint16_t prev_sp;
static uint8_t  prev_flags;

static void remember_registers(const CPU *cpu) {
    for (int i = 0; i < 16; i++) prev_r[i] = cpu->r[i];
    prev_sp    = cpu->sp;
    prev_flags = cpu->flags;
}

/* "*" if a value changed since the previous printout (only when marking). */
static char mark_if(int mark, int changed) { return (mark && changed) ? '*' : ' '; }

/* R0..R15 in two rows, then PC, IR, SP and the flags.
 * With mark = 1, a "*" follows every register, SP or flag that changed. */
static void print_registers(const CPU *cpu, int mark) {
    for (int row = 0; row < 2; row++) {
        for (int i = row * 8; i < row * 8 + 8; i++)
            printf(" R%-2d=%04X%c", i, cpu->r[i], mark_if(mark, cpu->r[i] != prev_r[i]));
        printf("\n");
    }
    printf(" PC=0x%02X  IR=0x%04X  SP=0x%02X%c FLAGS:", cpu->pc, cpu->ir, cpu->sp,
           mark_if(mark, cpu->sp != prev_sp));
    static const struct { int bit; char name; } FLAG[] = {{F_Z, 'Z'}, {F_N, 'N'}, {F_C, 'C'}, {F_V, 'V'}};
    for (int k = 0; k < 4; k++)
        printf(" %c=%d%c", FLAG[k].name, cpu_flag(cpu, FLAG[k].bit),
               mark_if(mark, (cpu->flags ^ prev_flags) & FLAG[k].bit));
    printf("\n");
}

/* One-time key printed at the top of every trace. */
static void print_legend(void) {
    printf(" Legend:  PC> next instruction   SP> top of stack   "
           "* changed by the last instruction\n");
    printf("          Contents = decoded instruction (text) or value (data/stack)\n");
    printf("          Registers in hex; FLAGS Z=zero N=negative C=carry V=overflow\n");
}

void dump_loaded(const CPU *cpu, const char *program_name) {
    printf("%s\n Program: %s  (loaded, before execution)\n%s\n", LINE, program_name, LINE);
    print_legend();
    printf("\n");
    print_memory(cpu, NULL);
    printf("\n");
    print_registers(cpu, 0);
    remember_registers(cpu);
    printf("\n");
}

void dump_step(const CPU *cpu, const StepInfo *info) {
    char text[40];
    disassemble(info->word, text, sizeof text);
    printf("%s\n Step %3ld | executed 0x%02X: %-20s (0x%04X)\n%s\n", LINE, info->step,
           info->addr, text, info->word, LINE);
    print_memory(cpu, info->changed);
    printf("\n");
    print_registers(cpu, 1);
    remember_registers(cpu);
    printf("\n");
}

void dump_final(const CPU *cpu) {
    printf("%s\n HALT after %ld instructions - final state\n%s\n", LINE, cpu->steps, LINE);
    print_memory(cpu, NULL);
    printf("\n");
    print_registers(cpu, 0);
}
