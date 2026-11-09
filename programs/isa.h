/* ============================================================================
 *  isa.h  —  Team 02 CPU, finalized ISA v1.0 (shared by assembler.c and cpu.c)
 * ----------------------------------------------------------------------------
 *  Single header of ISA constants so the assembler and the simulator can never
 *  drift apart. If the ISA changes, change it HERE first (owner: Saima).
 *
 *  16-bit word · 16 registers R0..R15 · 256-word RAM · one word per instruction
 *  Formats:  R [op4|rd4|rs4|fn4]   I [op4|rd4|imm8]   M [op4|rd4|rs4|off4]
 * ==========================================================================*/
#ifndef ISA_H
#define ISA_H

#define WORD_BITS   16
#define RAM_WORDS   256     /* addresses 0x00 .. 0xFF                        */
#define NREG        16      /* R0 .. R15                                     */
#define SP_INIT     0xFF    /* stack top; full-descending (grows down)       */

/* ---- memory map (word addresses) ----------------------------------------*/
#define TEXT_BASE   0x00    /* 0x00..0x3F  program instructions              */
#define DATA_BASE   0x40    /* 0x40..0x7F  globals / results                 */
#define HEAP_BASE   0x80    /* 0x80..0xBF  heap                              */
#define STACK_BASE  0xC0    /* 0xC0..0xFF  stack                             */

/* ---- opcodes (high nibble of the instruction word) ----------------------*/
enum {
    OP_HALT = 0x0, OP_ALU = 0x1, OP_LDI = 0x2, OP_LD  = 0x3,
    OP_ST   = 0x4, OP_LDR = 0x5, OP_STR = 0x6, OP_MOV = 0x7,
    OP_ADDI = 0x8, OP_JMP = 0x9, OP_JZ  = 0xA, OP_JNZ = 0xB,
    OP_JN   = 0xC, OP_CALL= 0xD, OP_RET = 0xE, OP_STK = 0xF   /* STK=PUSH/POP */
};

/* ---- ALU function codes (fn nibble when opcode == OP_ALU) ----------------*/
enum {
    FN_ADD = 0x0, FN_SUB = 0x1, FN_MUL = 0x2, FN_DIV = 0x3, FN_AND = 0x4,
    FN_OR  = 0x5, FN_NOT = 0x6, FN_SHL = 0x7, FN_SHR = 0x8, FN_CMP = 0x9
};

/* ---- PUSH/POP discriminator (fn nibble when opcode == OP_STK) ------------*/
enum { STK_PUSH = 0x0, STK_POP = 0x1 };

/* ---- FLAGS bit positions -------------------------------------------------*/
enum { F_Z = 1, F_N = 2, F_C = 4, F_V = 8 };

#endif /* ISA_H */
