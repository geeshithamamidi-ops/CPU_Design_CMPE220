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
 *    - memory operands use brackets           e.g.  LDR R2, [R5+3]
 *                                                   STR [R5+3], R2
 *    - jump/CALL target may be a label or a number
 *    - .equ NAME, value  names an address, then LD/ST can use it:
 *                                .equ result, 0x40
 *                                ST   [result], R0
 *
 *  Error checking (reports the line and returns -1, assembles nothing):
 *    - bad register (not R0..R15)      - unknown label      - bad ALU fn
 *    - offset outside 0..15            - malformed [reg+off] - unknown mnemonic
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

/* Validate a register token "R0".."R15": must be 'R' followed by digits only,
 * in range 0..15. Returns the number, or -1 with an error message. */
static int getreg(const char *t, int lineno){
    if (t[0] == 'R' || t[0] == 'r'){
        const char *p = t + 1;
        if (isdigit((unsigned char)*p)){
            while (isdigit((unsigned char)*p)) p++;     /* consume all digits */
            if (*p == '\0'){                            /* no trailing junk   */
                int r = atoi(t + 1);
                if (r >= 0 && r < NREG) return r;
            }
        }
    }
    fprintf(stderr, "asm: bad register '%s' on line %d\n", t, lineno);
    return -1;
}

/* Is this token a number (decimal, -decimal, or 0xHEX) rather than a label? */
static int is_numeric(const char *t){
    if (*t == '-' || *t == '+') t++;
    return isdigit((unsigned char)*t);
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

/* Parse an absolute address operand: "[0x40]", "0x40", or "64".
 * Brackets are optional (the spec writes LD/ST with them). Must be a number
 * in range 0..255. Returns 0 on success, -1 with a message. */
static int parse_addr(const char *t, int lineno, int *addr){
    char tmp[32];
    if (t[0] == '['){                                  /* strip [ ... ]      */
        const char *end = strchr(t, ']');
        if (!end){ fprintf(stderr, "asm: missing ']' in '%s' on line %d\n", t, lineno); return -1; }
        int len = (int)(end - (t + 1));
        if (len < 0 || len > 31) len = 31;
        memcpy(tmp, t + 1, len); tmp[len] = '\0';
    } else {
        snprintf(tmp, sizeof tmp, "%.31s", t);
    }
    char *p = tmp;                                     /* trim spaces        */
    while (*p == ' ') p++;
    char *e = p + strlen(p);
    while (e > p && e[-1] == ' ') *--e = '\0';

    int v;
    if (is_numeric(p)){
        v = imm_val(p);                                /* plain number       */
    } else {
        v = find_sym(p);                               /* name from .equ/label */
        if (v < 0){
            fprintf(stderr, "asm: unknown label '%s' on line %d\n", p, lineno);
            return -1;
        }
    }
    if (v < 0 || v > 255){
        fprintf(stderr, "asm: address %d out of range 0..255 on line %d\n", v, lineno);
        return -1;
    }
    *addr = v;
    return 0;
}

/* Parse a memory operand "[Rb+off]" or "[Rb]" into base register + offset.
 * Offset must be 0..15 (4-bit field). Returns 0 on success, -1 with a message. */
static int parse_mem(const char *t, int lineno, int *base, int *off){
    if (t[0] != '['){
        fprintf(stderr, "asm: expected [reg+off], got '%s' on line %d\n", t, lineno);
        return -1;
    }
    const char *end = strchr(t, ']');
    if (!end){
        fprintf(stderr, "asm: missing ']' in '%s' on line %d\n", t, lineno);
        return -1;
    }
    char inner[32];
    int len = (int)(end - (t + 1));
    if (len < 0 || len > 31) len = 31;
    memcpy(inner, t + 1, len); inner[len] = '\0';      /* text between [ ]    */

    int o = 0;
    char *plus = strchr(inner, '+');
    if (plus){ *plus = '\0'; o = (int)strtol(plus + 1, NULL, 0); }  /* +offset */

    char *rp = inner;                                  /* trim spaces around reg */
    while (*rp == ' ') rp++;
    char *re = rp + strlen(rp);
    while (re > rp && re[-1] == ' ') *--re = '\0';

    int b = getreg(rp, lineno);
    if (b < 0) return -1;
    if (o < 0 || o > 15){
        fprintf(stderr, "asm: offset %d out of range 0..15 on line %d\n", o, lineno);
        return -1;
    }
    *base = b; *off = o;
    return 0;
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

    /* ---- zero-operand ---- */
    if (!strcmp(mn, "HALT")) { *out = (OP_HALT << 12); return 0; }
    if (!strcmp(mn, "RET"))  { *out = (OP_RET  << 12); return 0; }

    /* ---- LDI rd, imm8 : immediate is UNSIGNED 0..255 ---- */
    if (!strcmp(mn, "LDI")){
        int rd = getreg(a1, lineno); if (rd < 0) return -1;
        int v = imm_val(a2);
        if (v < 0 || v > 255){
            fprintf(stderr, "asm: LDI immediate %d out of range 0..255 on line %d\n", v, lineno);
            return -1;
        }
        *out = (OP_LDI << 12) | (rd << 8) | (v & 0xFF); return 0;
    }
    /* ---- ADDI rd, imm8 : immediate is SIGNED -128..127 ---- */
    if (!strcmp(mn, "ADDI")){
        int rd = getreg(a1, lineno); if (rd < 0) return -1;
        int v = imm_val(a2);
        if (v < -128 || v > 127){
            fprintf(stderr, "asm: ADDI immediate %d out of range -128..127 on line %d\n", v, lineno);
            return -1;
        }
        *out = (OP_ADDI << 12) | (rd << 8) | (v & 0xFF); return 0;
    }
    /* ---- MOV rd, rs ---- */
    if (!strcmp(mn, "MOV")){
        int rd = getreg(a1, lineno), rs = getreg(a2, lineno);
        if (rd < 0 || rs < 0) return -1;
        *out = (OP_MOV << 12) | (rd << 8) | (rs << 4); return 0;
    }
    /* ---- PUSH / POP rd ---- */
    if (!strcmp(mn, "PUSH") || !strcmp(mn, "POP")){
        int rd = getreg(a1, lineno); if (rd < 0) return -1;
        int k = !strcmp(mn, "PUSH") ? STK_PUSH : STK_POP;
        *out = (OP_STK << 12) | (rd << 8) | k; return 0;
    }
    /* ---- LD rd, [addr8]  — address may be [0x40] or bare 0x40 ---- */
    if (!strcmp(mn, "LD")){
        int rd = getreg(a1, lineno); if (rd < 0) return -1;
        int ad; if (parse_addr(a2, lineno, &ad) != 0) return -1;
        *out = (OP_LD << 12) | (rd << 8) | (ad & 0xFF); return 0;
    }
    /* ---- ST [addr8], rs  — address in a1 ([0x40] or 0x40), source reg in a2 ---- */
    if (!strcmp(mn, "ST")){
        int rs = getreg(a2, lineno); if (rs < 0) return -1;
        int ad; if (parse_addr(a1, lineno, &ad) != 0) return -1;
        *out = (OP_ST << 12) | (rs << 8) | (ad & 0xFF); return 0;
    }
    /* ---- LDR rd, [rs+off4]  (M-format) ---- */
    if (!strcmp(mn, "LDR")){
        int rd = getreg(a1, lineno); if (rd < 0) return -1;
        int base, off; if (parse_mem(a2, lineno, &base, &off) != 0) return -1;
        *out = (OP_LDR << 12) | (rd << 8) | (base << 4) | (off & 0xF); return 0;
    }
    /* ---- STR [rs+off4], rd  (M-format): rd field = data reg, rs field = base ---- */
    if (!strcmp(mn, "STR")){
        int base, off; if (parse_mem(a1, lineno, &base, &off) != 0) return -1;
        int rd = getreg(a2, lineno); if (rd < 0) return -1;
        *out = (OP_STR << 12) | (rd << 8) | (base << 4) | (off & 0xF); return 0;
    }
    /* ---- ALU rd, rs, FN ---- */
    if (!strcmp(mn, "ALU")){
        int rd = getreg(a1, lineno), rs = getreg(a2, lineno);
        if (rd < 0 || rs < 0) return -1;
        int fn = alu_fn(a3);
        if (fn < 0) { fprintf(stderr, "asm: bad ALU fn '%s' on line %d\n", a3, lineno); return -1; }
        *out = (OP_ALU << 12) | (rd << 8) | (rs << 4) | (fn & 0xF); return 0;
    }

    /* ---- jumps / CALL : target is a label (must exist) or a numeric address ---- */
    int op = -1;
    if      (!strcmp(mn, "JMP"))  op = OP_JMP;
    else if (!strcmp(mn, "JZ"))   op = OP_JZ;
    else if (!strcmp(mn, "JNZ"))  op = OP_JNZ;
    else if (!strcmp(mn, "JN"))   op = OP_JN;
    else if (!strcmp(mn, "CALL")) op = OP_CALL;
    if (op >= 0){
        int t;
        if (is_numeric(a1)){
            t = imm_val(a1);
        } else {
            t = find_sym(a1);                           /* must be a real label */
            if (t < 0){ fprintf(stderr, "asm: unknown label '%s' on line %d\n", a1, lineno); return -1; }
        }
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

        /* .equ NAME, value  -> names an address/constant; emits no word */
        if (!strncmp(buf, ".equ", 4) && isspace((unsigned char)buf[4])){
            char name[MAXLABEL], val[32];
            if (sscanf(buf + 4, " %31[^, ] , %31s", name, val) != 2 || !is_numeric(val)){
                fprintf(stderr, "asm: bad .equ (use: .equ NAME, value): '%s'\n", buf);
                fclose(f); return -1;
            }
            if (find_sym(name) >= 0){
                fprintf(stderr, "asm: label '%s' defined more than once\n", name);
                fclose(f); return -1;
            }
            if (g_nsyms < MAXLINES){
                snprintf(g_syms[g_nsyms].label, MAXLABEL, "%.*s", MAXLABEL - 1, name);
                g_syms[g_nsyms].addr = imm_val(val);
                g_nsyms++;
            }
            continue;
        }

        char *colon = strchr(buf, ':');
        if (colon){                                /* a label is defined     */
            *colon = '\0';
            int ln = (int)strlen(buf);             /* trim space before ':'  */
            while (ln > 0 && isspace((unsigned char)buf[ln - 1])) buf[--ln] = '\0';
            if (find_sym(buf) >= 0){               /* reject a repeated label */
                fprintf(stderr, "asm: label '%s' defined more than once\n", buf);
                fclose(f); return -1;
            }
            if (g_nsyms < MAXLINES){
                snprintf(g_syms[g_nsyms].label, MAXLABEL, "%.*s", MAXLABEL - 1, buf);
                g_syms[g_nsyms].addr = addr;
                g_nsyms++;
            }
            char *rest = colon + 1;                 /* code after the label?  */
            if (!clean(rest)) continue;             /* label-only line        */
            snprintf(g_line[g_nlines++], sizeof g_line[0], "%.*s", (int)sizeof g_line[0] - 1, rest);
            addr++;
        } else {
            snprintf(g_line[g_nlines++], sizeof g_line[0], "%.*s", (int)sizeof g_line[0] - 1, buf);
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
