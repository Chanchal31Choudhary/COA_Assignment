        LDI R0, 0
        BZ R0, done
        ADD R1, R0, R0
        JMP done
 done:  HALT
