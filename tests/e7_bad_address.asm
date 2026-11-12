; e7: register-indirect load from address 508 -> BadAddress
        LDI  R0, 127
        ADD  R0, R0, R0
        ADD  R0, R0, R0
        LDR  R1, [R0]
        HALT
