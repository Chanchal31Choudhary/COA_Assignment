; t2_shift_mul.asm -- shifts, multiply, NOT, MOV (negative numbers)   (Chanchal)
; expect R0=000C R1=FFF3 R2=FFE0 R3=3FFE R4=FFFE R5=000C R6=FFA0
        LDI  R0, -8
        LDI  R1, 2
        SHL  R2, R0, R1      ; -8 << 2  = -32
        SHR  R3, R0, R1      ; logical  = 0x3FFE
        SAR  R4, R0, R1      ; arithmetic = -2
        LDI  R5, 12
        MUL  R6, R5, R0      ; 12 * -8 = -96
        NOT  R1, R5          ; ~12
        MOV  R0, R5
        HALT
