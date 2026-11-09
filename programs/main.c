/* ============================================================================
 *  main.c  —  command-line entry point for the Team 02 CPU simulator
 *  Owner: Anupama Singh
 * ----------------------------------------------------------------------------
 *  ./cpu programs/exp.asm                 full trace: RAM after every instruction
 *  ./cpu programs/fact.asm --quiet        only the loaded and final RAM
 *  ./cpu programs/fact.asm --max-steps 500
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "dump.h"

static void usage(const char *self) {
    fprintf(stderr,
            "usage: %s <program.asm> [--quiet] [--max-steps N]\n"
            "  --quiet        print only the loaded and final memory\n"
            "  --max-steps N  stop after N instructions (default 10000)\n", self);
}

int main(int argc, char **argv) {
    const char *path = NULL;
    int quiet = 0;
    long max_steps = 10000;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--quiet"))                         quiet = 1;
        else if (!strcmp(argv[i], "--max-steps") && i + 1 < argc) max_steps = strtol(argv[++i], NULL, 10);
        else if (argv[i][0] == '-' || path) { usage(argv[0]); return 2; }
        else                                                     path = argv[i];
    }
    if (!path) { usage(argv[0]); return 2; }

    static CPU cpu;                    /* static: keeps the 256-word RAM off the stack */
    if (cpu_load(&cpu, path) < 0) {
        fprintf(stderr, "Error: %s\n", cpu.error);
        return 1;
    }

    dump_loaded(&cpu, path);
    if (cpu_run(&cpu, max_steps, quiet ? NULL : dump_step) < 0) {
        fprintf(stderr, "CPU error at step %ld: %s\n", cpu.steps + 1, cpu.error);
        return 1;
    }
    dump_final(&cpu);
    return 0;
}
