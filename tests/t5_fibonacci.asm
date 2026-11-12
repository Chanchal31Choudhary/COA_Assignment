; t5_fibonacci.asm -- first 10 Fibonacci numbers stored at memory 100..109   (Jiya)
; expect MEM[100..109] = 0 1 1 2 3 5 8 13 21 34 ; R5 = 0022 (34)
        .equ ARR, 100
        LDI  R0, 0
        LDI  R1, 1
        LDI  R2, 10
        LDI  R3, ARR
loop:   STR  R0, [R3]
        ADD  R4, R0, R1
        MOV  R0, R1
        MOV  R1, R4
        ADDI R3, 1
        ADDI R2, -1
        BNZ  R2, loop
        LOAD R5, 109
        HALT
