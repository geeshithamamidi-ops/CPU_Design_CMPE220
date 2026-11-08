; ===========================================================================
;  exp.asm  —  8 squared (8^2 = 64), written DIRECTLY in our assembly
;  Method: repeated addition. 64 = 8 + 8 + ... (eight times).
;  Registers: R0 = accumulator, R1 = base (8), R2 = loop counter
; ===========================================================================
        LDI  R0, 0          ; acc   = 0
        LDI  R1, 8          ; base  = 8
        LDI  R2, 8          ; count = 8
loop:   ALU  R0, R1, ADD    ; acc  += base           (R0 = R0 + R1)
        ADDI R2, -1         ; count-- ; sets Z when count reaches 0
        JNZ  loop           ; repeat while count != 0
        ST   0x40, R0       ; store result (64) into data segment word 0x40
        HALT                ; stop
