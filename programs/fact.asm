; ===========================================================================
;  fact.asm  —  8! = 40320, hand-translated from fact.c (iterative)
;
;  C source (fact.c):
;      int main(void){
;          int result = 1;
;          for (int i = 8; i > 1; i--)   // multiply 8*7*6*...*2
;              result = result * i;
;          return result;                // 40320
;      }
;
;  Registers: R0 = result, R1 = i (loop var), R3 = constant 1 (loop bound)
; ===========================================================================
        LDI  R0, 1          ; result = 1
        LDI  R1, 8          ; i      = 8
        LDI  R3, 1          ; const  = 1  (compare i against this)
loop:   ALU  R0, R1, MUL    ; result = result * i     (R0 = R0 * R1)
        ADDI R1, -1         ; i--
        ALU  R1, R3, CMP    ; compare i with 1  -> sets Z when i == 1
        JNZ  loop           ; repeat while i != 1
        ST   0x40, R0       ; store result (40320) into data segment word 0x40
        HALT                ; stop
