/* ============================================================================
 *  assembler.h  —  Team 02 two-pass assembler (public interface)
 * ----------------------------------------------------------------------------
 *  cpu.c includes this and calls assemble() to load a .asm program directly
 *  into its RAM array. No file format in between — the assembler writes the
 *  16-bit instruction words straight into ram[], starting at address 0x00.
 * ==========================================================================*/
#ifndef ASSEMBLER_H
#define ASSEMBLER_H
#include <stdint.h>

/* Assemble the assembly-source file at `path` into ram[] (word-addressed,
 * first instruction at index 0). `ram_words` is the size of ram[] (use
 * RAM_WORDS). Returns the number of instruction words emitted, or -1 on a
 * syntax/IO error (a message is printed to stderr).                         */
int assemble(const char *path, uint16_t *ram, int ram_words);

#endif /* ASSEMBLER_H */
