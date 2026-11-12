; IEEE-754 binary16: 1.0 / +0.0 = +infinity (0x7C00).
LOAD R1, 8
LOAD R2, 9
FDIV R3, R1, R2
HALT
.org 8
.word 0x3C00
.word 0x0000
