# Arithmetic Subsystem and Division Instructions

## Scope and integration boundary

The arithmetic subsystem includes selectable adder and multiplier implementations, signed integer division, and IEEE-754 binary16 (FP16) arithmetic. `DIV` and `FDIV` are decoded as RISC201 instructions, represented in both microcode formats, executed by the ALU in the pipeline, and covered by CPU-level regression tests.

## Integer datapath

### Adders

The ALU now offers Ripple Carry, Kogge–Stone (the existing `addLookahead` implementation), Carry Select, and Brent–Kung. Carry Select computes the upper block for both possible carry inputs and selects the appropriate result. Brent–Kung uses an up-sweep/down-sweep parallel-prefix network. These are 16-bit software models; the reported delay values are comparative model estimates, not measured silicon timing or area.

Interactive selection: `alu add ripple|cla|carryselect|brentkung`.

### Multipliers

The existing Shift-and-Add and Booth radix-2 options remain. Wallace Tree reduces partial-product columns with carry-save compressors and performs a final carry-propagate addition. The ALU returns the low 16 bits, matching the existing MUL architectural behavior.

Interactive selection: `alu mul shift|booth|wallace`.

## FP16 arithmetic and the `FDIV` instruction

`src/fp16.h` and `src/fp16.cpp` implement binary16 conversion and add, subtract, multiply, and divide. The CPU-level `FDIV rd, rs1, rs2` instruction uses the raw 16-bit binary16 encodings held in the source registers and writes the binary16 quotient to the destination register. Conversion uses round-to-nearest, ties-to-even; infinities, NaNs, signed zero, subnormals, overflow and underflow are handled. Addition, subtraction, and multiplication compute using the host `float` representation and round the result back to binary16. Division handles IEEE-754 special cases explicitly and rounds the double-precision quotient directly to binary16 to avoid an intermediate float-rounding step. This is a software arithmetic model, not a hardware FP execution unit. NaN payloads are canonicalized on conversion.

Example API: `FP16::add(aBits, bBits)`, `FP16::sub(aBits, bBits)`, `FP16::mul(aBits, bBits)`, `FP16::div(aBits, bBits)`, `FP16::fromFloat(value)`, `FP16::toFloat(bits)`.

## CPU-integrated division instructions

- `DIV rd, rs1, rs2`: signed 16-bit two's-complement integer division, truncating toward zero. A zero divisor raises the simulator's precise `DivideByZero` exception. The architectural result is 16 bits; `-32768 / -1` wraps to `0x8000`.
- `FDIV rd, rs1, rs2`: IEEE-754 binary16 division. Division by zero follows IEEE-754 (for example, finite nonzero / +0 produces +infinity; 0 / 0 produces NaN) rather than raising the integer divide exception.
- Both instructions use the R-type format and have micro-routines in the horizontal and vertical control stores. They flow through the same register-read, execute and writeback path as the other ALU instructions.

Assembly-level regression cases: `tests/t11_integer_division.asm`, `tests/t12_fp16_division.asm`, `tests/t13_fp16_division_special.asm`, and `tests/e11_integer_divide_by_zero.asm`.

## Verification

Run `./tests/arithmetic/run.sh`. The deterministic verifier compares adder outputs and carry flags, Wallace low-word products, all 65,536 FP16 bit patterns (NaNs canonicalized), randomized signed integer divisions, and randomized FP16 operations against reference results. It also checks integer divide-by-zero exceptions and FP16 division cases for zero, signed zero, infinities, NaNs, overflow, underflow/subnormal results, and rounding. The CPU regression suite runs both new division instructions in all eight pipeline/control/forwarding configurations.

Run `./run_tests.sh` for the original CPU regression suite. `compare alu` includes randomized checks for the integer arithmetic architectures.

## Limitations

- The Wallace-tree model is an algorithmic compressor simulation, not a gate-level circuit.
- Delay figures are illustrative model units, not physical timing/area measurements.

### FP16 interactive demonstration

Compile and run the demonstration from the project root:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Isrc src/fp16.cpp tests/arithmetic/fp16_demo.cpp -o fp16_demo
./fp16_demo
```

It prints FP16 bit patterns and the decoded results for addition, subtraction, multiplication, division, a repeating quotient (`1 / 3`), and division by zero.
