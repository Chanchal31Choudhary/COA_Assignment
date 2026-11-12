; branch_demo.asm -- tiny loop used by the pipeline-visualizer tests
; one taken branch (control hazard + flush) then fall-through, then HALT
; expect R1=0000
        LDI   R1, 2
loop:   ADDI  R1, -1
        BNZ   R1, loop
        HALT
