# CPU Design — CMPE 220 (Team 02)

**Team 02:** Saima Ashraf · Geeshitha Mamidi · Anupama Singh

A simulator for a 16-bit CPU of our own design. It assembles a program written in our assembly language, runs it, and prints the RAM block holding the program and its data after every instruction.

- Project description: [`Description.md`](Description.md)
- Instruction set: [`docs/isa/FINALIZED_ISA.md`](docs/isa/FINALIZED_ISA.md)
- Diagrams: [`docs/diagrams/`](docs/diagrams/)

## How to run

Requires a C compiler (`gcc` or `clang`) and `make`. No other libraries.

```bash
git clone https://github.com/ragwort15/CPU_Design_CMPE220.git
cd CPU_Design_CMPE220

make                          # builds ./cpu
./cpu programs/exp.asm        # 8²  -> RAM[0x40] = 64
./cpu programs/fact.asm       # 8!  -> RAM[0x40] = 40320
```

Without `make`, build with one command:

```bash
gcc -std=c99 -O2 -Iprograms -o cpu programs/assembler.c programs/cpu.c programs/dump.c programs/main.c
```

Each run prints the RAM block once after loading, again after **every** instruction, and a final state at `HALT`.

| Command | What it does |
| --- | --- |
| `./cpu programs/fact.asm --quiet` | Print only the loaded and final memory |
| `./cpu programs/fact.asm > fact_trace.txt` | Save the full step-by-step trace to a file |
| `make run` | Build and run both programs |
| `make test` | Build and run the unit tests (checks every value in `docs/EXPECTED_RESULTS.md`) |
| `make traces` | Regenerate the saved traces in `output/` |

Saved traces of both programs are in [`output/`](output/).

## Reading the output

```
     Addr  Seg    Hex     Binary               Contents
 PC> 0x04  text   0x81FF  1000 0001 1111 1111  ADDI R1, -1
     0x40  data   0x9D80  1001 1101 1000 0000  40320  (signed -25216)  *
```

| Mark | Meaning |
| --- | --- |
| `PC>` | Next instruction to be fetched |
| `SP>` | Top of the stack (most recently pushed item) |
| `*` | Cell changed by the instruction just executed |
| Contents | Text segment: decoded instruction · Data segment: value |

Below each memory block: registers R0–R15 (hex), PC, IR, SP and the flags Z, N, C, V.

## Repository layout

| Path | Purpose | Owner |
| --- | --- | --- |
| `programs/isa.h` | Finalized ISA constants | Saima |
| `programs/assembler.c`, `.h` | Two-pass assembler: `.asm` → 16-bit words in RAM | Saima |
| `programs/cpu.c`, `.h` | CPU core: registers, flags, full-descending stack, fetch → decode → execute | Anupama |
| `programs/main.c` | Command-line entry point | Anupama |
| `programs/dump.c`, `.h` | Memory and register printouts after every instruction | Anupama (baseline) / Geeshitha |
| `programs/exp.asm` | 8² written directly in our assembly | Saima |
| `programs/fact.c`, `fact.asm` | 8! in C and its hand translation | Saima |
| `tests/test_cpu.c` | Unit tests | Anupama |
| `docs/` | ISA, diagrams, C ↔ assembly translation, expected results | Saima |
| `output/` | Saved step-by-step traces | — |
| `Makefile` | Build, run, test and trace targets | Anupama |
