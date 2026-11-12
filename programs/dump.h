/* ============================================================================
 *  dump.h  —  memory / register printouts for the CPU simulator
 * ----------------------------------------------------------------------------
 *  Baseline trace format; Geeshitha owns the final report formatting.
 *
 *  The RAM block shown is: the program's text words, the data cells it
 *  reads or writes, and the stack items currently pushed.
 *    PC>  next instruction to fetch      SP>  top of stack
 *    *    RAM cell, register, SP or flag changed by the instruction just
 *         executed (registers are compared with the previous printout)
 * ==========================================================================*/
#ifndef DUMP_H
#define DUMP_H

#include <stddef.h>
#include <stdint.h>
#include "cpu.h"

/* Write the assembly text for one instruction word, in the syntax of
 * docs/isa/FINALIZED_ISA.md (e.g. "ALU R0, R1, MUL", "ST 0x40, R0"). */
void disassemble(uint16_t word, char *out, size_t size);

void dump_loaded(const CPU *cpu, const char *program_name);
void dump_step(const CPU *cpu, const StepInfo *info);
void dump_final(const CPU *cpu);

#endif /* DUMP_H */
