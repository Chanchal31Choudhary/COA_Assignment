; e8: SP moved below STACK_LIMIT by ADDI (caught at write-back) -> StackOverflow
        LDI   R0, 3
        ADDI  SP, -100
        HALT
