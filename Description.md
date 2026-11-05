# CPU Design — CMPE 220 

**Course:** CMPE 220 System Software, San José State University, Fall 2026

**Team 02:** Saima Ashraf · Geeshitha Mamidi · Anupama Singh

## Project description

We simulate a CPU of our own design and run programs written both in our own assembly language and in C. C programs are translated into the assembly language we designed.

- **Memory view:** the simulator shows the contents of a small block of memory (RAM) that holds the instructions and data. After each instruction executes, it shows this memory block again, so the change is visible.
- **Programs:** two simple programs:
  - **Exponential (8²)**, implemented directly in our assembly language.
  - **Factorial (8!)**, written in C and then translated into our assembly language before execution.
- **CPU design:** a diagram of the CPU and a table of the instruction encoding.
- **Output:** memory contents and other printed output are formatted neatly, so execution can be followed without long explanations.
