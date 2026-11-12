; t3_sum_loop.asm -- sum of 1..10 using a loop (control hazards)   (Rani)
; expect R0=0037 (55) R1=0000
        LDI  R0, 0
        LDI  R1, 10
loop:   ADD  R0, R0, R1
        ADDI R1, -1
        BNZ  R1, loop
        HALT
