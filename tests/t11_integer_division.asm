; Signed 16-bit integer division: quotient truncates toward zero.
LDI R1, -21
LDI R2, 4
DIV R3, R1, R2
LDI R4, 21
DIV R5, R4, R2
HALT
