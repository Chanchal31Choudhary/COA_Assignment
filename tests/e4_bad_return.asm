; e4: return address overwritten on the stack -> BadReturn
        CALL f
        HALT
f:      LDI  R0, 100
        STR  R0, [SP]
        RET
