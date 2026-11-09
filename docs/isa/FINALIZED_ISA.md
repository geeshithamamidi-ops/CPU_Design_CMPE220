# Team 02 — Finalized ISA v1.0 (CMPE 220 CPU Design)

**Owner:** Saima Ashraf (ISA & CPU design) · **Status:** FINALIZED Nov 8, 2026
**Reused by:** CPU Design (Nov 28) and Program Layout & Execution (Dec 5).

> This is the single source of truth for `isa.h`. If anything here changes,
> this file changes first, then `isa.h`.

---

## 1. Machine summary

| Item | Decision |
|------|----------|
| Word size | **16 bits** (2 bytes), word-addressable |
| Smallest unit | 1 word = 1 memory cell = 1 instruction *or* 1 data value |
| RAM | **256 words** × 16 bits (addresses `0x00`–`0xFF`) = 512 bytes |
| General registers | **R0–R15** (16-bit each; 4-bit register select) |
| Special registers | **PC** (program counter), **IR** (instruction register), **SP** (stack pointer), **FLAGS** |
| Flags | **Z** zero · **N** negative (bit 15) · **C** carry · **V** signed overflow |
| Stack | **Full-descending.** SP starts `0xFF`; PUSH/CALL pre-decrement, POP/RET post-increment |
| Instruction length | **Exactly one 16-bit word** — nothing spills into a second word |

### Memory map

```
0x00 – 0x3F   TEXT    (program instructions)
0x40 – 0x7F   DATA    (globals / results)
0x80 – 0xBF   HEAP    (unused by these programs; shown empty)
0xC0 – 0xFF   STACK   (grows DOWN from 0xFF)
```

---

## 2. Instruction formats (16 bits)

```
        bit: 15 14 13 12 | 11 10  9  8 |  7  6  5  4 |  3  2  1  0
  R-format:  [   opcode  ] [    rd     ] [    rs     ] [    fn     ]
  I-format:  [   opcode  ] [    rd     ] [        imm8            ]
  M-format:  [   opcode  ] [    rd     ] [    rs     ] [   off4    ]
```

- **R** — register-to-register (ALU, MOV, HALT, RET, PUSH/POP).
- **I** — one register + 8-bit immediate/address (LDI, LD, ST, ADDI, all jumps, CALL).
- **M** — base register + 4-bit offset (LDR, STR) for indexed/frame access.

### Immediate rules (important — resolves the one real ambiguity)

| Used by | `imm8` interpretation | Range |
|---------|----------------------|-------|
| LDI, LD, ST, JMP, JZ, JNZ, JN, CALL | **unsigned** (address or small constant) | 0 … 255 |
| **ADDI** | **signed** two's-complement | −128 … +127 |

Example: `ADDI R1, -1` encodes `imm8 = 0xFF`.

---

## 3. Opcode table (16 opcodes — fully used)

| Op | Mnemonic | Fmt | Action | Flags set |
|----|----------|-----|--------|-----------|
| `0x0` | `HALT` | R | stop execution | — |
| `0x1` | `ALU rd, rs, fn` | R | `rd = rd ⟨fn⟩ rs` | Z N (C V) |
| `0x2` | `LDI rd, imm8` | I | `rd = imm8` (unsigned) | Z N |
| `0x3` | `LD rd, [addr8]` | I | `rd = RAM[addr8]` | Z N |
| `0x4` | `ST [addr8], rs` | I | `RAM[addr8] = rs` | — |
| `0x5` | `LDR rd, [rs+off4]` | M | `rd = RAM[rs+off4]` | Z N |
| `0x6` | `STR [rs+off4], rd` | M | `RAM[rs+off4] = rd` | — |
| `0x7` | `MOV rd, rs` | R | `rd = rs` | Z N |
| `0x8` | `ADDI rd, imm8` | I | `rd = rd + imm8` (signed) | Z N C V |
| `0x9` | `JMP addr8` | I | `PC = addr8` | — |
| `0xA` | `JZ addr8` | I | if Z: `PC = addr8` | — |
| `0xB` | `JNZ addr8` | I | if ¬Z: `PC = addr8` | — |
| `0xC` | `JN addr8` | I | if N: `PC = addr8` | — |
| `0xD` | `CALL addr8` | I | `SP--; RAM[SP]=PC; PC=addr8` | — |
| `0xE` | `RET` | R | `PC=RAM[SP]; SP++` | — |
| `0xF` | `PUSH/POP rd` | R | `fn=0`→PUSH `SP--; RAM[SP]=rd` · `fn=1`→POP `rd=RAM[SP]; SP++` | — |

### ALU function codes (`fn` nibble when opcode = `0x1`)

| fn | Op | fn | Op |
|----|----|----|----|
| `0x0` | ADD | `0x5` | OR |
| `0x1` | SUB | `0x6` | NOT (`rd = ~rs`) |
| `0x2` | MUL | `0x7` | SHL (`rd = rd << (rs & 0xF)`) |
| `0x3` | DIV | `0x8` | SHR (`rd = rd >> (rs & 0xF)`) |
| `0x4` | AND | `0x9` | CMP (`rd − rs`, sets flags only) |

### PUSH/POP discriminator (`fn` nibble when opcode = `0xF`)

| fn | Op |
|----|----|
| `0x0` | PUSH rd |
| `0x1` | POP rd |

---

## 4. Calling convention (for Dec 5 recursion)

- **Argument** passed in **R1**, **return value** in **R0**.
- `CALL` automatically **pushes the return address**; `RET` pops it.
- A callee that needs to preserve `n` across its own recursive call does
  `PUSH R1` on entry and `POP R1` before `RET`.

---

## 5. Encoding examples (verified by the simulator)

| Assembly | opcode | rd | rs / imm8 | fn | Binary | Hex |
|----------|--------|----|-----------|----|--------|-----|
| `ADD R1, R2`  | 0001 | 0001 | 0010 | 0000 | `0001 0001 0010 0000` | `0x1120` |
| `LDI R1, 8`   | 0010 | 0001 | `0000 1000` | — | `0010 0001 0000 1000` | `0x2108` |
| `ADDI R1, -1` | 1000 | 0001 | `1111 1111` | — | `1000 0001 1111 1111` | `0x81FF` |
| `MUL R0, R1`  | 0001 | 0000 | 0001 | 0010 | `0001 0000 0001 0010` | `0x1012` |
| `CMP R1, R3`  | 0001 | 0001 | 0011 | 1001 | `0001 0001 0011 1001` | `0x1139` |
| `JNZ 0x03`    | 1011 | 0000 | `0000 0011` | — | `1011 0000 0000 0011` | `0xB003` |
| `ST 0x40, R0` | 0100 | 0000 | `0100 0000` | — | `0100 0000 0100 0000` | `0x4040` |
| `PUSH R1`     | 1111 | 0001 | 0000 | 0000 | `1111 0001 0000 0000` | `0xF100` |
| `POP R1`      | 1111 | 0001 | 0000 | 0001 | `1111 0001 0000 0001` | `0xF101` |
| `HALT`        | 0000 | 0000 | 0000 | 0000 | `0000 0000 0000 0000` | `0x0000` |
