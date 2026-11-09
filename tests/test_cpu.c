/* ============================================================================
 *  test_cpu.c  —  unit tests for the CPU simulator core
 *  Owner: Anupama Singh
 * ----------------------------------------------------------------------------
 *  1. The two assignment programs reproduce docs/EXPECTED_RESULTS.md.
 *  2. Every instruction behaves as in docs/isa/FINALIZED_ISA.md
 *     (programs are written as raw 16-bit words, so these tests check the
 *     CPU on its own, independent of the assembler).
 *  3. Recursion through Saima's assembler (CALL / RET / PUSH / POP).
 *
 *  Run from the repository root:   make test
 * ==========================================================================*/
#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "dump.h"

static int passed = 0, failed = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) passed++;                                                  \
        else { failed++; printf("  FAIL line %d: %s\n", __LINE__, #cond); }  \
    } while (0)

static CPU cpu;

/* ---- build instruction words directly (formats from FINALIZED_ISA.md) ---- */
static uint16_t R(unsigned op, unsigned rd, unsigned rs, unsigned fn) {
    return (uint16_t)((op << 12) | (rd << 8) | (rs << 4) | (fn & 0xF));
}
static uint16_t I(unsigned op, unsigned rd, int imm8) {
    return (uint16_t)((op << 12) | (rd << 8) | ((unsigned)imm8 & 0xFF));
}

/* Load raw words at 0x00 and run to HALT. Returns 0 on success. */
static int run_words(const uint16_t *words, int n, long max_steps) {
    cpu_reset(&cpu);
    memcpy(cpu.ram, words, (size_t)n * sizeof words[0]);
    cpu.text_words = n;
    return cpu_run(&cpu, max_steps, NULL);
}
#define RUN(...) run_words((const uint16_t[]){__VA_ARGS__}, \
                           (int)(sizeof((const uint16_t[]){__VA_ARGS__}) / 2), 1000)

static int z(void) { return cpu_flag(&cpu, F_Z); }
static int n(void) { return cpu_flag(&cpu, F_N); }
static int c(void) { return cpu_flag(&cpu, F_C); }
static int v(void) { return cpu_flag(&cpu, F_V); }

/* No RAM cell outside the program text and `data_addr` was modified. */
static int only_touched(int data_addr) {
    for (int a = cpu.text_words; a < RAM_WORDS; a++)
        if (a != data_addr && cpu.ram[a] != 0) return 0;
    return 1;
}

/* ===== 1. EXPECTED_RESULTS.md ============================================*/
static void test_exp(void) {
    const uint16_t text[] = {0x2000, 0x2108, 0x2208, 0x1010, 0x82FF, 0xB003, 0x4040, 0x0000};
    CHECK(cpu_load(&cpu, "programs/exp.asm") == 8);
    for (int a = 0; a < 8; a++) CHECK(cpu.ram[a] == text[a]);
    CHECK(cpu_run(&cpu, 1000, NULL) == 0);
    CHECK(cpu.r[0] == 0x0040 && cpu.r[1] == 0x0008 && cpu.r[2] == 0x0000);
    CHECK(cpu.ram[0x40] == 64);
    CHECK(cpu.pc == 0x08 && cpu.sp == 0xFF && z());
    CHECK(cpu.steps == 29);
    CHECK(only_touched(0x40));
}

static void test_fact(void) {
    const uint16_t text[] = {0x2001, 0x2108, 0x2301, 0x1012, 0x81FF,
                             0x1139, 0xB003, 0x4040, 0x0000};
    CHECK(cpu_load(&cpu, "programs/fact.asm") == 9);
    for (int a = 0; a < 9; a++) CHECK(cpu.ram[a] == text[a]);
    CHECK(cpu_run(&cpu, 1000, NULL) == 0);
    CHECK(cpu.r[0] == 0x9D80 && cpu.r[1] == 0x0001 && cpu.r[3] == 0x0001);
    CHECK(cpu.ram[0x40] == 40320);
    CHECK(cpu.pc == 0x09 && cpu.sp == 0xFF && z());
    CHECK(cpu.steps == 33);
    CHECK(only_touched(0x40));
}

/* R0 after each MUL in fact.asm: 8, 56, 336, 1680, 6720, 20160, 40320 */
static void test_fact_checkpoints(void) {
    const uint16_t want[] = {0x0008, 0x0038, 0x0150, 0x0690, 0x1A40, 0x4EC0, 0x9D80};
    StepInfo info;
    int k = 0;
    CHECK(cpu_load(&cpu, "programs/fact.asm") == 9);
    while (!cpu.halted && cpu_step(&cpu, &info) == 0)
        if (info.addr == 0x03 && k < 7) CHECK(cpu.r[0] == want[k++]);
    CHECK(k == 7);
}

/* ===== 2. Instructions =================================================== */
static void test_load_store(void) {
    CHECK(RUN(I(OP_LDI, 1, 0x50), I(OP_LDI, 2, 7),
              R(OP_STR, 2, 1, 3),        /* RAM[R1+3] = R2  -> RAM[0x53] = 7 */
              R(OP_LDR, 3, 1, 3),        /* R3 = RAM[R1+3]                    */
              I(OP_ST, 3, 0x60),         /* RAM[0x60] = R3                    */
              I(OP_LD, 4, 0x60),         /* R4 = RAM[0x60]                    */
              R(OP_MOV, 5, 4, 0),        /* R5 = R4                           */
              0x0000) == 0);
    CHECK(cpu.ram[0x53] == 7 && cpu.ram[0x60] == 7);
    CHECK(cpu.r[3] == 7 && cpu.r[4] == 7 && cpu.r[5] == 7);
}

static void test_alu(void) {
    CHECK(RUN(I(OP_LDI, 1, 12), I(OP_LDI, 2, 10), I(OP_LDI, 6, 2),
              R(OP_MOV, 3, 1, 0), R(OP_ALU, 3, 2, FN_AND),   /* 1100 & 1010 = 1000 */
              R(OP_MOV, 4, 1, 0), R(OP_ALU, 4, 2, FN_OR),    /* 1100 | 1010 = 1110 */
              R(OP_ALU, 5, 1, FN_NOT),                       /* ~12 = 0xFFF3       */
              R(OP_MOV, 7, 1, 0), R(OP_ALU, 7, 6, FN_SHL),   /* 12 << 2 = 48       */
              R(OP_MOV, 8, 1, 0), R(OP_ALU, 8, 6, FN_SHR),   /* 12 >> 2 = 3        */
              R(OP_MOV, 9, 1, 0), R(OP_ALU, 9, 2, FN_SUB),   /* 12 - 10 = 2        */
              R(OP_MOV, 10, 1, 0), R(OP_ALU, 10, 6, FN_DIV), /* 12 / 2 = 6         */
              R(OP_MOV, 11, 1, 0), R(OP_ALU, 11, 2, FN_ADD), /* 12 + 10 = 22       */
              0x0000) == 0);
    CHECK(cpu.r[3] == 8 && cpu.r[4] == 14 && cpu.r[5] == 0xFFF3);
    CHECK(cpu.r[7] == 48 && cpu.r[8] == 3 && cpu.r[9] == 2);
    CHECK(cpu.r[10] == 6 && cpu.r[11] == 22);
}

static void test_jumps(void) {
    /* JZ / JNZ / JN taken and not taken; JMP */
    CHECK(RUN(I(OP_LDI, 1, 0),        /* 0: Z=1                    */
              I(OP_JZ, 0, 3),         /* 1: taken -> 3             */
              I(OP_LDI, 2, 99),       /* 2: skipped                */
              I(OP_JNZ, 0, 6),        /* 3: Z still 1, not taken   */
              I(OP_LDI, 3, 1),        /* 4: runs                   */
              I(OP_JMP, 0, 7),        /* 5: -> 7                   */
              I(OP_LDI, 4, 99),       /* 6: skipped                */
              I(OP_ADDI, 5, -1),      /* 7: R5 = 0xFFFF, N=1       */
              I(OP_JN, 0, 10),        /* 8: taken -> 10            */
              I(OP_LDI, 6, 99),       /* 9: skipped                */
              0x0000) == 0);          /* 10                        */
    CHECK(cpu.r[2] == 0 && cpu.r[3] == 1 && cpu.r[4] == 0 && cpu.r[6] == 0);
    CHECK(cpu.r[5] == 0xFFFF);
}

/* Stack is full-descending: SP-- then write; read then SP++ */
static void test_stack(void) {
    CHECK(RUN(I(OP_LDI, 1, 1), I(OP_LDI, 2, 2),
              R(OP_STK, 1, 0, STK_PUSH), R(OP_STK, 2, 0, STK_PUSH), 0x0000) == 0);
    CHECK(cpu.sp == 0xFD);
    CHECK(cpu.ram[0xFE] == 1 && cpu.ram[0xFD] == 2 && cpu.ram[0xFF] == 0);

    CHECK(RUN(I(OP_LDI, 1, 1), I(OP_LDI, 2, 2),
              R(OP_STK, 1, 0, STK_PUSH), R(OP_STK, 2, 0, STK_PUSH),
              R(OP_STK, 3, 0, STK_POP), R(OP_STK, 4, 0, STK_POP), 0x0000) == 0);
    CHECK(cpu.r[3] == 2 && cpu.r[4] == 1 && cpu.sp == 0xFF);   /* last in, first out */

    CHECK(RUN(I(OP_CALL, 0, 2), 0x0000, R(OP_RET, 0, 0, 0)) == 0);
    CHECK(cpu.ram[0xFE] == 1);                                 /* return address     */
    CHECK(cpu.pc == 0x02 && cpu.sp == 0xFF && cpu.steps == 3);
}

/* Flags column of the opcode table */
static void test_flags(void) {
    CHECK(RUN(I(OP_LDI, 1, 1), I(OP_ADDI, 1, -1), 0x0000) == 0);
    CHECK(z() && !n());                                        /* ADDI -> Z          */

    CHECK(RUN(I(OP_LDI, 1, 2), I(OP_LDI, 2, 5), R(OP_ALU, 1, 2, FN_SUB), 0x0000) == 0);
    CHECK(n() && c() && !z());                                 /* 2-5: N, borrow     */

    CHECK(RUN(I(OP_LDI, 1, 5), I(OP_LDI, 2, 5), R(OP_ALU, 1, 2, FN_CMP), 0x0000) == 0);
    CHECK(z() && cpu.r[1] == 5);                               /* CMP: flags only    */

    CHECK(RUN(I(OP_LDI, 1, 255), I(OP_LDI, 2, 255),
              R(OP_ALU, 1, 2, FN_MUL), R(OP_ALU, 1, 2, FN_MUL), 0x0000) == 0);
    CHECK(v());                                                /* signed overflow    */

    CHECK(RUN(I(OP_LDI, 1, 1), R(OP_ALU, 2, 0, FN_NOT), R(OP_ALU, 2, 1, FN_ADD), 0x0000) == 0);
    CHECK(c() && z());                                         /* 0xFFFF + 1 carries */

    /* LDI, LD, LDR, MOV set Z and N but keep C and V */
    CHECK(RUN(I(OP_LDI, 1, 1), R(OP_ALU, 2, 0, FN_NOT), R(OP_ALU, 2, 1, FN_ADD), /* C=1 */
              I(OP_LDI, 3, 0), 0x0000) == 0);
    CHECK(z() && c());
    CHECK(RUN(R(OP_ALU, 2, 0, FN_NOT), I(OP_LDI, 3, 1), R(OP_MOV, 4, 2, 0), 0x0000) == 0);
    CHECK(n() && !z());                                        /* MOV of 0xFFFF      */
    CHECK(RUN(I(OP_LDI, 1, 1), I(OP_LD, 2, 0x50), 0x0000) == 0);
    CHECK(z());                                                /* LD of 0            */
    CHECK(RUN(I(OP_LDI, 1, 0x50), R(OP_LDR, 2, 1, 1), 0x0000) == 0);
    CHECK(z());                                                /* LDR of 0           */

    /* ST, PUSH, POP, JMP leave the flags alone */
    CHECK(RUN(I(OP_LDI, 1, 1), I(OP_ST, 0, 0x40), R(OP_STK, 0, 0, STK_PUSH),
              R(OP_STK, 2, 0, STK_POP), I(OP_JMP, 0, 5), 0x0000) == 0);
    CHECK(!z());                                               /* still from LDI R1,1 */
}

/* Errors stop the CPU with a message instead of crashing */
static void test_errors(void) {
    CHECK(RUN(I(OP_LDI, 1, 4), R(OP_ALU, 1, 0, FN_DIV), 0x0000) != 0);
    CHECK(strstr(cpu.error, "division") != NULL);
    CHECK(RUN(R(OP_RET, 0, 0, 0)) != 0);                       /* empty stack        */
    CHECK(RUN(R(OP_STK, 1, 0, STK_POP), 0x0000) != 0);
    CHECK(RUN(I(OP_JMP, 0, 0)) != 0);                          /* runaway loop guard */
    CHECK(RUN(I(OP_JMP, 0, 0x40)) != 0);                       /* PC leaves text     */
    CHECK(RUN(R(OP_ALU, 1, 2, 0xC), 0x0000) != 0);             /* unknown ALU fn     */
    CHECK(cpu_load(&cpu, "programs/does_not_exist.asm") < 0);
}

/* ===== 3. Recursion through Saima's assembler ============================ */
static void test_recursive_factorial(void) {
    const char *path = "tests/recursive_fact.tmp.asm";
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (!f) return;
    fputs("main:   LDI  R1, 5\n"
          "        CALL fact\n"
          "        ST   0x40, R0\n"
          "        HALT\n"
          "fact:   LDI  R2, 1\n"
          "        ALU  R1, R2, CMP\n"
          "        JZ   base\n"
          "        JN   base\n"
          "        PUSH R1\n"
          "        ADDI R1, -1\n"
          "        CALL fact\n"
          "        POP  R1\n"
          "        ALU  R0, R1, MUL\n"
          "        RET\n"
          "base:   LDI  R0, 1\n"
          "        RET\n", f);
    fclose(f);
    CHECK(cpu_load(&cpu, path) == 16);
    remove(path);
    CHECK(cpu_run(&cpu, 1000, NULL) == 0);
    CHECK(cpu.ram[0x40] == 120);
    CHECK(cpu.sp == SP_INIT);                                  /* fully unwound      */
}

/* LDR / STR with [base+offset] operands through Saima's assembler */
static void test_indexed_via_assembler(void) {
    const char *path = "tests/indexed.tmp.asm";
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (!f) return;
    fputs("        LDI  R5, 0x50\n"
          "        LDI  R2, 7\n"
          "        STR  [R5+3], R2\n"      /* RAM[0x53] = 7 */
          "        LDR  R3, [R5+3]\n"      /* R3 = 7        */
          "        STR  [R5], R3\n"        /* RAM[0x50] = 7 */
          "        HALT\n", f);
    fclose(f);
    CHECK(cpu_load(&cpu, path) == 6);
    remove(path);
    CHECK(cpu.ram[2] == 0x6253 && cpu.ram[3] == 0x5353);
    CHECK(cpu_run(&cpu, 100, NULL) == 0);
    CHECK(cpu.ram[0x53] == 7 && cpu.ram[0x50] == 7 && cpu.r[3] == 7);
}

/* Disassembler output matches the spec's syntax */
static void test_disassemble(void) {
    char t[40];
    disassemble(0x1012, t, sizeof t); CHECK(!strcmp(t, "ALU R0, R1, MUL"));
    disassemble(0x81FF, t, sizeof t); CHECK(!strcmp(t, "ADDI R1, -1"));
    disassemble(0x4040, t, sizeof t); CHECK(!strcmp(t, "ST 0x40, R0"));
    disassemble(0x51E2, t, sizeof t); CHECK(!strcmp(t, "LDR R1, [R14+2]"));
    disassemble(0x61E2, t, sizeof t); CHECK(!strcmp(t, "STR [R14+2], R1"));
    disassemble(0xF100, t, sizeof t); CHECK(!strcmp(t, "PUSH R1"));
    disassemble(0xF101, t, sizeof t); CHECK(!strcmp(t, "POP R1"));
    disassemble(0xB003, t, sizeof t); CHECK(!strcmp(t, "JNZ 0x03"));
    disassemble(0x0000, t, sizeof t); CHECK(!strcmp(t, "HALT"));
}

int main(void) {
    test_exp();
    test_fact();
    test_fact_checkpoints();
    test_load_store();
    test_alu();
    test_jumps();
    test_stack();
    test_flags();
    test_errors();
    test_recursive_factorial();
    test_indexed_via_assembler();
    test_disassemble();
    printf("%d checks passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
