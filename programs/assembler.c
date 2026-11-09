/* ============================================================================
 *  assembler.c  —  Team 02 two-pass assembler for our 16-bit ISA
 *  Owner: Saima Ashraf  (implements the finalized encoding table in isa.h)
 * ----------------------------------------------------------------------------
 *  Turns assembly source into 16-bit machine words.
 *
 *  Pass 1 : scan every line, record each label -> address (one word/instr).
 *  Pass 2 : encode each instruction to one 16-bit word per isa.h.
 *
 *  Syntax it accepts
 *    - one instruction per line                 e.g.  ALU R0, R1, MUL
 *    - labels end with ':'                      e.g.  loop:  ADDI R1, -1
 *    - comments start with ';'                        LDI R0, 1   ; result
 *    - immediates: decimal, -decimal, or 0xHEX
 *    - jump/CALL target may be a label or a number
 *
 *  Two ways to use it
 *    1. As a library:  #include "assembler.h";  assemble(path, ram, RAM_WORDS);
 *    2. Standalone   :  gcc -DASM_STANDALONE -o asm assembler.c
 *                       ./asm program.asm           (prints the hex words)
 *                       ./asm program.asm out.hex   (also writes a hex image)
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "isa.h"
#include "assembler.h"

/* ---- limits --------------------------------------------------------------*/
#define MAXLINES 512
#define MAXLABEL 32

/* ---- symbol table (label -> address) and kept source lines ---------------*/
typedef struct { char label[MAXLABEL]; int addr; } Sym;
static Sym  g_syms[MAXLINES];  static int g_nsyms;
static char g_line[MAXLINES][96]; static int g_nlines;

/* ---------- small helpers -------------------------------------------------*/

/* "R5" -> 5 ; anything else -> -1 */
static int reg_num(const char *t){
    if ((t[0] == 'R' || t[0] == 'r') && isdigit((unsigned char)t[1]))
        return atoi(t + 1);
    return -1;
}

/* look up a previously defined label; -1 if not found */
static int find_sym(const char *name){
    for (int i = 0; i < g_nsyms; i++)
        if (!strcmp(g_syms[i].label, name)) return g_syms[i].addr;
    return -1;
}

/* mnemonic -> ALU fn code ; -1 if not an ALU function */
static int alu_fn(const char *m){
    if (!strcmp(m, "ADD")) return FN_ADD;
    if (!strcmp(m, "SUB")) return FN_SUB;
    if (!strcmp(m, "MUL")) return FN_MUL;
    if (!strcmp(m, "DIV")) return FN_DIV;
    if (!strcmp(m, "AND")) return FN_AND;
    if (!strcmp(m, "OR"))  return FN_OR;
    if (!strcmp(m, "NOT")) return FN_NOT;
    if (!strcmp(m, "SHL")) return FN_SHL;
    if (!strcmp(m, "SHR")) return FN_SHR;
    if (!strcmp(m, "CMP")) return FN_CMP;
    return -1;
}

/* parse an immediate: 0xHEX, -decimal, or decimal */
static int imm_val(const char *t){
    if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) return (int)strtol(t, NULL, 16);
    return (int)strtol(t, NULL, 10);
}

/* strip ';' comment and surrounding whitespace; return 1 if text remains */
static int clean(char *s){
    char *c = strchr(s, ';'); if (c) *c = '\0';
    int n = (int)strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    char *p = s; while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    return s[0] != '\0';
}

/* ---------- pass 2: encode ONE cleaned instruction line -------------------*/
static int encode(const char *line, int lineno, uint16_t *out){
    char mn[16], a1[32], a2[32], a3[32];
    a1[0] = a2[0] = a3[0] = '\0';
    int n = sscanf(line, "%15s %31[^,], %31[^,], %31s", mn, a1, a2, a3);
    if (n < 1) { fprintf(stderr, "asm: empty line %d\n", lineno); return -1; }

    int rd = reg_num(a1) & 0xF;
    int rs = reg_num(a2) & 0xF;

    /* zero / one-operand and simple forms */
    if (!strcmp(mn, "HALT")) { *out = (OP_HALT << 12); return 0; }
    if (!strcmp(mn, "RET"))  { *out = (OP_RET  << 12); return 0; }
    if (!strcmp(mn, "LDI"))  { *out = (OP_LDI  << 12) | (rd << 8) | (imm_val(a2) & 0xFF); return 0; }
    if (!strcmp(mn, "ADDI")) { *out = (OP_ADDI << 12) | (rd << 8) | (imm_val(a2) & 0xFF); return 0; }
    if (!strcmp(mn, "MOV"))  { *out = (OP_MOV  << 12) | (rd << 8) | (rs << 4); return 0; }
    if (!strcmp(mn, "PUSH")) { *out = (OP_STK  << 12) | (rd << 8) | STK_PUSH; return 0; }
    if (!strcmp(mn, "POP"))  { *out = (OP_STK  << 12) | (rd << 8) | STK_POP;  return 0; }

    /* LD rd,[addr]  — address is a bare immediate in a2 */
    if (!strcmp(mn, "LD"))   { *out = (OP_LD << 12) | (rd << 8) | (imm_val(a2) & 0xFF); return 0; }
    /* ST addr,rs    — address in a1, source register in a2 */
    if (!strcmp(mn, "ST"))   { *out = (OP_ST << 12) | ((reg_num(a2) & 0xF) << 8) | (imm_val(a1) & 0xFF); return 0; }
    /* LDR rd,[rs+off4] */
    if (!strcmp(mn, "LDR"))  { *out = (OP_LDR << 12) | (rd << 8) | (rs << 4) | (imm_val(a3) & 0xF); return 0; }
    /* STR [rs+off4],rd — base in a2, offset in a3 would reorder; here STR rs,off,rd */
    if (!strcmp(mn, "STR"))  { *out = (OP_STR << 12) | ((reg_num(a3) & 0xF) << 8) | (rs << 4) | (imm_val(a2) & 0xF); return 0; }
    /* ALU rd,rs,FN */
    if (!strcmp(mn, "ALU")){
        int fn = alu_fn(a3);
        if (fn < 0) { fprintf(stderr, "asm: bad ALU fn '%s' on line %d\n", a3, lineno); return -1; }
        *out = (OP_ALU << 12) | (rd << 8) | (rs << 4) | (fn & 0xF); return 0;
    }

    /* jumps / call : target is a label (resolved) or a numeric address in a1 */
    int op = -1;
    if      (!strcmp(mn, "JMP"))  op = OP_JMP;
    else if (!strcmp(mn, "JZ"))   op = OP_JZ;
    else if (!strcmp(mn, "JNZ"))  op = OP_JNZ;
    else if (!strcmp(mn, "JN"))   op = OP_JN;
    else if (!strcmp(mn, "CALL")) op = OP_CALL;
    if (op >= 0){
        int t = find_sym(a1);
        if (t < 0) t = imm_val(a1);
        *out = (uint16_t)((op << 12) | (t & 0xFF));
        return 0;
    }

    fprintf(stderr, "asm: unknown mnemonic '%s' on line %d\n", mn, lineno);
    return -1;
}

/* ---------- public entry point -------------------------------------------*/
int assemble(const char *path, uint16_t *ram, int ram_words){
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "asm: cannot open %s\n", path); return -1; }

    char buf[128];
    g_nlines = 0; g_nsyms = 0;

    /* ---- pass 1: collect labels and keep real instruction lines ---- */
    int addr = 0;
    while (fgets(buf, sizeof buf, f)){
        if (!clean(buf)) continue;                 /* blank / comment-only   */
        char *colon = strchr(buf, ':');
        if (colon){                                /* a label is defined     */
            *colon = '\0';
            if (g_nsyms < MAXLINES){
                strncpy(g_syms[g_nsyms].label, buf, MAXLABEL - 1);
                g_syms[g_nsyms].addr = addr;
                g_nsyms++;
            }
            char *rest = colon + 1;                 /* code after the label?  */
            if (!clean(rest)) continue;             /* label-only line        */
            strncpy(g_line[g_nlines++], rest, 95);
            addr++;
        } else {
            strncpy(g_line[g_nlines++], buf, 95);
            addr++;
        }
    }
    fclose(f);

    if (g_nlines > ram_words){
        fprintf(stderr, "asm: program too big (%d words > %d)\n", g_nlines, ram_words);
        return -1;
    }

    /* ---- pass 2: encode every kept line ---- */
    for (int i = 0; i < g_nlines; i++){
        uint16_t word;
        if (encode(g_line[i], i + 1, &word) != 0) return -1;
        ram[i] = word;
    }
    return g_nlines;
}

/* ===========================================================================
 *  Standalone mode — lets Saima test the assembler without the CPU.
 *  Build:  gcc -DASM_STANDALONE -o asm assembler.c
 * ==========================================================================*/
#ifdef ASM_STANDALONE
int main(int argc, char **argv){
    if (argc < 2){
        printf("usage: %s program.asm [out.hex]\n", argv[0]);
        return 1;
    }
    uint16_t ram[RAM_WORDS];
    memset(ram, 0, sizeof ram);

    int n = assemble(argv[1], ram, RAM_WORDS);
    if (n < 0) return 1;

    printf("Assembled %d instruction words from %s:\n", n, argv[1]);
    for (int i = 0; i < n; i++)
        printf("  0x%02X : 0x%04X\n", i, ram[i]);

    if (argc >= 3){                                 /* optional hex image out */
        FILE *o = fopen(argv[2], "w");
        if (!o){ fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
        for (int i = 0; i < n; i++) fprintf(o, "%04X\n", ram[i]);
        fclose(o);
        printf("Wrote %s (%d words)\n", argv[2], n);
    }
    return 0;
}
#endif
