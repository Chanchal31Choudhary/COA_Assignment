; t4_factorial.asm -- 5! with MUL and a loop   (Kavya)
; expect R0=0078 (120)
        LDI  R0, 1
        LDI  R1, 5
loop:   MUL  R0, R0, R1
        ADDI R1, -1
        BNZ  R1, loop
        HALT
