; Registers hold raw IEEE-754 binary16 values: 6.0 / 2.0 = 3.0.
LOAD R1, 8
LOAD R2, 9
FDIV R3, R1, R2
HALT
.org 8
.word 0x4600
.word 0x4000
