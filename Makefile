# Team 02 CPU simulator — CMPE 220
#
#   make            build ./cpu
#   make run        run both programs (8^2 and 8!)
#   make test       build and run the unit tests
#   make traces     save full step-by-step traces to output/
#   make clean      remove build files

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2
SRC     = programs/assembler.c programs/cpu.c programs/dump.c
HEADERS = programs/isa.h programs/assembler.h programs/cpu.h programs/dump.h

all: cpu

cpu: $(SRC) programs/main.c $(HEADERS)
	$(CC) $(CFLAGS) -Iprograms -o $@ $(SRC) programs/main.c

test_cpu: $(SRC) tests/test_cpu.c $(HEADERS)
	$(CC) $(CFLAGS) -Iprograms -o $@ $(SRC) tests/test_cpu.c

run: cpu
	./cpu programs/exp.asm
	./cpu programs/fact.asm

test: test_cpu
	./test_cpu

traces: cpu
	mkdir -p output
	./cpu programs/exp.asm  > output/exp_trace.txt
	./cpu programs/fact.asm > output/fact_trace.txt

clean:
	rm -f cpu test_cpu cpu.exe test_cpu.exe

.PHONY: all run test traces clean
