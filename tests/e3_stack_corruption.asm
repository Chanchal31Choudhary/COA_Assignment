; e3: normal store into the stack area below SP -> StackCorruption
        LDI   R0, 5
        PUSH  R0
        STORE R0, 200
        HALT
