; t1_arith.asm  -- basic ALU: ADD SUB AND OR XOR            (Jiya)
; expect R0=0019 R1=0007 R2=0020 R3=0012 R4=0001 R5=001F R6=001E
        LDI  R0, 25
        LDI  R1, 7
        ADD  R2, R0, R1
        SUB  R3, R0, R1
        AND  R4, R0, R1
        OR   R5, R0, R1
        XOR  R6, R0, R1
        HALT
