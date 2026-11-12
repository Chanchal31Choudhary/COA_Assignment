; t10_stack_demo.asm -- leaves 4 words on the stack: try  'stack'  in the CLI   (Rani)
; expect SP=00FC (252)
        LDI   R0, 1
        LDI   R1, 2
        PUSHM R0
        ADD   R0, R0, R1
        PUSHM R0
        ADD   R0, R0, R1
        PUSHM R0
        ADD   R0, R0, R1
        PUSHM R0
        CALL  sub
        HALT
sub:    LDI   R2, 77
        PUSH  R2
        POP   R3
        RET
