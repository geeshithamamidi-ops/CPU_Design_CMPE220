# Expected results — for Anupama's simulator tests (Saima's task #6)

These are the authoritative values the production simulator must reproduce.
All values verified against the finalized ISA v1.0.

---

## Program A — `exp.asm` (8² = 64)

**Loaded text segment (0x00–0x07):**
`2000 2108 2208 1010 82FF B003 4040 0000`

**Final state at HALT:**

| Where | Value |
|-------|-------|
| R0 (accumulator) | `0x0040` = **64** |
| R1 (base) | `0x0008` |
| R2 (counter) | `0x0000` |
| RAM[0x40] (result) | `0x0040` = **64** |
| PC | `0x08` · SP `0xFF` · Flags Z=1 |
| Total steps | 29 |

**R0 checkpoints across the 8 loop passes:** 8 → 16 → 24 → 32 → 40 → 48 → 56 → 64
(hex: 08, 10, 18, 20, 28, 30, 38, 40)

---

## Program B — `fact.asm` (8! = 40320)

**Loaded text segment (0x00–0x08):**
`2001 2108 2301 1012 81FF 1139 B003 4040 0000`

**Final state at HALT:**

| Where | Value |
|-------|-------|
| R0 (result) | `0x9D80` = **40320** |
| R1 (i) | `0x0001` |
| R3 (const 1) | `0x0001` |
| RAM[0x40] (result) | `0x9D80` = **40320** |
| PC | `0x09` · SP `0xFF` · Flags Z=1 |
| Total steps | 33 |

**R0 checkpoints after each MUL:** 8 → 56 → 336 → 1680 → 6720 → 20160 → 40320
(hex: 0008, 0038, 0150, 0690, 1A40, 4EC0, 9D80)

---

## Quick self-check for the production sim

```
exp.asm  -> RAM[0x40] == 0x0040 (64)     and R0 == 0x0040
fact.asm -> RAM[0x40] == 0x9D80 (40320)  and R0 == 0x9D80
```

If both hold with the stack pointer back at `0xFF` and no other RAM cell
outside the text/data regions touched, the run is correct.
