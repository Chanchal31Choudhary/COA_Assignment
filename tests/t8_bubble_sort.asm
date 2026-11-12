; t8_bubble_sort.asm -- bubble sort of 8 words at memory 100..107   (Chanchal)
; expect MEM[100..107] = 1 2 3 4 6 7 8 9
        .equ ARR, 100
        .equ N, 8
        LDI  R0, N-1
outer:  LDI  R1, ARR
        MOV  R2, R0
inner:  LDR  R3, [R1]
        ADDI R1, 1
        LDR  R4, [R1]
        SUB  R5, R4, R3
        BNEG R5, swap
        JMP  next
swap:   STR  R3, [R1]
        ADDI R1, -1
        STR  R4, [R1]
        ADDI R1, 1
next:   ADDI R2, -1
        BNZ  R2, inner
        ADDI R0, -1
        BNZ  R0, outer
        HALT
        .org 100
        .word 9, 3, 7, 1, 8, 2, 6, 4
