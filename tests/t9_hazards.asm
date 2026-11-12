; t9_hazards.asm -- back-to-back RAW dependencies and a load-use hazard   (Kavya)
; expect R0=0005 R1=000A R2=000F R3=0019 R4=0019 R5=0032
        LDI   R0, 5
        ADD   R1, R0, R0
        ADD   R2, R1, R0
        ADD   R3, R2, R1
        STORE R3, 100
        LOAD  R4, 100
        ADD   R5, R4, R4
        HALT
