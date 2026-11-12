; e5: self-modifying store into the code segment -> CodeWrite
        LDI   R0, 1
        STORE R0, 0
        HALT
