; t7_recursion.asm -- recursive factorial with CALL / RET / PUSH / POP   (Kavya)
; expect R0=0005 R1=0078 (120), SP=0100
        LDI  R0, 5
        CALL fact
        HALT
fact:   BNZ  R0, rec
        LDI  R1, 1
        RET
rec:    PUSH R0
        ADDI R0, -1
        CALL fact
        POP  R0
        MUL  R1, R1, R0
        RET
