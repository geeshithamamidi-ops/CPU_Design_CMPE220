/* ============================================================================
 *  cpu.h  —  Team 02 CPU simulator core (public interface)
 *  Owner: Anupama Singh
 * ----------------------------------------------------------------------------
 *  Implements the finalized ISA in isa.h / docs/isa/FINALIZED_ISA.md.
 *  Programs are loaded with Saima's assembler: assemble(path, ram, RAM_WORDS).
 *
 *  One instruction cycle (cpu_step):
 *    1. FETCH    IR <- RAM[PC],  PC <- PC + 1
 *    2. DECODE   split IR into opcode | rd | rs | fn  (or imm8 / off4)
 *    3. EXECUTE  ALU, load/store, jump, call/return, push/pop; update FLAGS
 * ==========================================================================*/
#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "isa.h"

#define TEXT_END   (DATA_BASE - 1)     /* 0x3F: last text-segment address   */
#define CPU_ERRLEN 128

typedef struct {
    uint16_t ram[RAM_WORDS];   /* 256 words x 16 bits                       */
    uint16_t r[NREG];          /* R0..R15                                   */
    int      pc;               /* program counter: next instruction         */
    uint16_t ir;               /* instruction register: current instruction */
    int      sp;               /* stack pointer: top item (0xFF = empty)    */
    uint8_t  flags;            /* F_Z | F_N | F_C | F_V  (bits from isa.h)  */
    int      halted;           /* 1 after HALT                              */
    long     steps;            /* instructions executed so far              */
    int      text_words;       /* words the assembler placed at 0x00..      */
    uint8_t  watch[RAM_WORDS]; /* data cells to show in dumps: LD/ST targets
                                  plus every address ever written           */
    char     error[CPU_ERRLEN];/* message when a step fails                 */
} CPU;

/* What one step did, so the dump can show it. */
typedef struct {
    long     step;              /* step number (1, 2, ...)                  */
    int      addr;              /* address the instruction was fetched from */
    uint16_t word;              /* the instruction word                     */
    uint8_t  changed[RAM_WORDS];/* 1 for each RAM cell this step modified   */
} StepInfo;

/* Clear RAM and registers; PC = 0x00, SP = 0xFF, flags = 0. */
void cpu_reset(CPU *cpu);

/* Reset, then assemble the .asm file at `path` straight into RAM.
 * Returns the number of instruction words, or -1 on an assembler error. */
int cpu_load(CPU *cpu, const char *path);

/* Execute one instruction. Returns 0, or -1 with cpu->error set. */
int cpu_step(CPU *cpu, StepInfo *info);

/* Optional callback, run after every instruction (used to print the trace). */
typedef void (*StepHook)(const CPU *cpu, const StepInfo *info);

/* Run until HALT. Stops with an error after max_steps (runaway loop guard).
 * Returns 0, or -1 with cpu->error set. */
int cpu_run(CPU *cpu, long max_steps, StepHook hook);

/* Flag helper: 1 if flag bit `f` (F_Z, F_N, F_C, F_V) is set. */
static inline int cpu_flag(const CPU *cpu, int f) { return (cpu->flags & f) != 0; }

#endif /* CPU_H */
