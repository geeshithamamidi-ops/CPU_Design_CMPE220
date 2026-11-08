# C ↔ Assembly side-by-side — `fact.c` → `fact.asm` (8! = 40320)

**Register map:** `R0` = result · `R1` = i (loop variable) · `R3` = constant 1 (loop bound)

| # | C source | Assembly (our ISA) | What it does |
|---|----------|--------------------|--------------|
| 1 | `int result = 1;` | `LDI R0, 1` | result = 1 |
| 2 | `int i = 8;` | `LDI R1, 8` | i = 8 |
| 3 | *(loop bound)* | `LDI R3, 1` | const 1, to compare i against |
| 4 | `result = result * i;` | `loop: ALU R0, R1, MUL` | result = result × i |
| 5 | `i--` | `ADDI R1, -1` | i = i − 1 |
| 6 | `i > 1 ?` | `ALU R1, R3, CMP` | compare i with 1 → sets Z when i == 1 |
| 7 | *(back-edge)* | `JNZ loop` | if i ≠ 1, repeat the loop |
| 8 | `return result;` | `ST 0x40, R0` | store result (40320) to DATA word 0x40 |
| 9 | *(end)* | `HALT` | stop |

### Encoded text segment (addresses 0x00–0x08)

```
Addr  Hex    Instruction
0x00  2001   LDI  R0, 1
0x01  2108   LDI  R1, 8
0x02  2301   LDI  R3, 1
0x03  1012   ALU  R0, R1, MUL      ; loop:
0x04  81FF   ADDI R1, -1
0x05  1139   ALU  R1, R3, CMP
0x06  B003   JNZ  0x03             ; -> loop
0x07  4040   ST   0x40, R0
0x08  0000   HALT
```

### Trace of the loop (how 40320 is built)

| iteration | i (before MUL) | result after `result*=i` |
|-----------|----------------|--------------------------|
| 1 | 8 | 8 |
| 2 | 7 | 56 |
| 3 | 6 | 336 |
| 4 | 5 | 1680 |
| 5 | 4 | 6720 |
| 6 | 3 | 20160 |
| 7 | 2 | **40320** |

After iteration 7, `ADDI R1,-1` makes i = 1, `CMP R1,R3` sets Z, and `JNZ` falls
through to the store. **Result = 40320 = 0x9D80.**
