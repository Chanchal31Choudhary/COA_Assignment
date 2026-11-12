; t6_stack_macros.asm -- full-descending stack through the preprocessor macros   (Jiya)
; expect R0=000B R1=0016 R2=0021 R3=002C, SP back to 0100
        LDI   R0, 11
        LDI   R1, 22
        LDI   R2, 33
        PUSHM R0
        PUSHM R1
        PUSHM R2
        LDI   R0, 0
        LDI   R1, 0
        LDI   R2, 0
        POPM  R2
        POPM  R1
        POPM  R0
        LDI   R3, 44
        PUSHALL              ; save R0..R6
        LDI   R0, 99
        LDI   R1, 98
        POPALL               ; restore R0..R6
        ALLOC 4              ; reserve 4 words
        FREE  4
        HALT
