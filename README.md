# RISC201 Processor Design Project

**Course:** CSC-201 — Computer Organization & Architecture  
**Term:** Autumn 2026  
**Language:** C++17  
**Project type:** Software simulation of a 16-bit RISC processor

## Project overview

RISC201 is a C++ simulator for a 16-bit RISC processor. It supports configurable 4-stage and 6-stage pipelines, a micro-programmed control unit with horizontal and vertical control-store representations, an assembler and disassembler, an interactive debugger, and tools for examining and comparing processor execution.

The project also includes an interactive animated pipeline visualizer, selectable integer arithmetic implementations, FP16 arithmetic, and a static pipeline hazard analyzer.

This is a **software simulator**; it is not a physical processor implementation.

## Team Members

| Name | Enrollment Number |
|---|---|
| **Kavya Chandak** | **25114051** |
| Jiya Patel | 25114067 |
| Rani Gupta | 25114074 |
| Chanchal Choudhary | 25114023 |

## Contributions

- **Kavya Chandak:** Pipeline, micro-programmed control unit, runtime hazard handling, evaluation, integration and final verification.
- **Jiya Patel:** ISA, assembler, preprocessing and static pipeline hazard analyzer.
- **Rani Gupta:** Disassembler, CLI/debugger, stack visualizer and pipeline visualizer.
- **Chanchal Choudhary:** ALU and arithmetic extensions, including integer arithmetic architectures and FP16 arithmetic.

## Main features

### Processor and control

- 16-bit RISC201 instruction set and register/memory model.
- Configurable 4-stage and 6-stage pipelines.
- Micro-programmed control with horizontal and vertical control styles.
- Forwarding configuration and pipeline hazard handling.
- Cycle stepping, instruction stepping, breakpoints, register/memory inspection and execution statistics.

### Assembly tools

- Two-pass assembler with `.hex`, `.bin` and listing output.
- Disassembler, including re-assemblable output mode.
- Stack-macro preprocessing.
- Runtime stack bounds and memory-safety checks with exception reporting.

### Pipeline analysis and visualization

- Interactive terminal pipeline visualizer (`pipeline` or `pipe`).
- Cycle snapshots, playback, previous/next navigation and 4-stage/6-stage switching.
- Static RAW, load-use and control-transfer hazard report (`hazards`).
- Execution comparison across pipeline depths, control styles and forwarding settings (`compare`).

### Arithmetic

- Integer adders: Ripple Carry, Kogge–Stone carry lookahead, Carry Select and Brent–Kung.
- Integer multipliers: Shift-and-Add, Booth Radix-2 and Wallace Tree.
- Signed 16-bit integer division (`DIV`) is integrated into the ALU, assembler, microcode and CPU pipeline. Quotients truncate toward zero; division by zero raises a precise `DivideByZero` exception.
- IEEE-754 binary16 (`FP16`) arithmetic supports conversion, add, subtract, multiply and divide. The `FDIV` instruction is integrated into the CPU pipeline; source and destination registers contain raw 16-bit binary16 encodings. IEEE-754 division-by-zero, infinity, NaN, signed-zero and subnormal behaviour is preserved.

`DIV` and `FDIV` are distinct instructions: `DIV` interprets operands as signed two's-complement integers, while `FDIV` interprets them as IEEE-754 binary16 values.

## Repository layout

```text
RISC201/
├── src/                  # CPU, pipeline, ISA, microcode, ALU, assembler, CLI, tools
├── tests/                # Assembly programs and expected regression outputs
│   ├── arithmetic/       # Integer and FP16 arithmetic verification + FP16 demo
│   ├── hazards/          # Hazard analyzer test programs
│   └── viz/              # Pipeline visualizer scenarios and expected output
├── docs/                 # Arithmetic extension and hazard analyzer documentation
├── About_RISC201_Team.pdf
├── PIPELINE_VISUALIZER.md
├── build.sh
├── Makefile
└── run_tests.sh
```

## Requirements

- A C++17-compatible compiler (for example, `g++`).
- `make` is optional; the build script can be used directly.

## Build and run

From the project root:

```bash
chmod +x build.sh run_tests.sh tests/arithmetic/run.sh tests/hazards/run.sh tests/toolchain/run.sh tests/viz/run.sh
./build.sh
./risc201
```

To start the simulator with an assembly program:

```bash
./risc201 tests/t1_arith.asm
```

Type `help` inside the simulator to see the available commands.

## Useful simulator commands

| Command | Purpose |
|---|---|
| `load <file>` | Load an assembly, HEX or binary program |
| `run` | Execute until halt, exception or breakpoint |
| `step [n]` | Advance by clock cycles |
| `next` | Run until one more instruction retires |
| `regs` / `mem <addr> [n]` | Inspect registers or memory |
| `pipeline` | Open the interactive pipeline visualizer |
| `hazards` | Display the static hazard analysis |
| `stats` | Display execution and ALU statistics |
| `compare` | Compare execution configurations |
| `alu` | Display current ALU implementation selections |
| `help` | Display the complete command list |

For the visualizer's playback controls, see [PIPELINE_VISUALIZER.md](PIPELINE_VISUALIZER.md).
For the hazard analyzer's method and limitations, see [docs/HAZARD_ANALYZER.md](docs/HAZARD_ANALYZER.md).
For arithmetic algorithms, integer division and FP16 instruction semantics, see [docs/ARITHMETIC_EXTENSION.md](docs/ARITHMETIC_EXTENSION.md).

## Division instruction examples

```asm
; Signed integer division: -21 / 4 = -5 (truncates toward zero)
LDI R1, -21
LDI R2, 4
DIV R3, R1, R2

; FP16 division: registers contain binary16 bit patterns
; 0x4600 = 6.0, 0x4000 = 2.0, result 0x4200 = 3.0
LOAD R1, 8
LOAD R2, 9
FDIV R3, R1, R2
HALT
.org 8
.word 0x4600
.word 0x4000
```

Run the focused CPU examples with `./risc201 --batch tests/t11_integer_division.asm --stats` and `./risc201 --batch tests/t12_fp16_division.asm --stats`.

## Verification

Run the test suites from the project root:

```bash
./tests/arithmetic/run.sh
./tests/hazards/run.sh
./tests/toolchain/run.sh
./tests/viz/run.sh
./run_tests.sh
```

The CPU regression script checks the assembly test programs across eight configurations:

- 4-stage and 6-stage pipelines;
- horizontal and vertical control;
- forwarding enabled and disabled.

The arithmetic suite checks the integer arithmetic extensions, randomized signed division, FP16 operations and IEEE-754 division edge cases. CPU assembly regressions also execute `DIV` and `FDIV` across all eight pipeline/control/forwarding configurations. The hazard suite checks dependency classification and selected pipeline cases. The toolchain suite checks assembler/disassembler round trips for the assembly fixtures, including both division instructions. The visualizer suite smoke-tests all seven interactive scenarios, including stalls, forwarding, branch flushes and menu handling.

Latest clean-extraction verification snapshot: 24/24 CPU regression programs passed in all eight configurations; 2,535,551 arithmetic checks passed with zero failures; 24 toolchain round-trip tests passed; all hazard-analyzer tests passed; and all seven visualizer scenarios passed. These are software regression results, not a proof of correctness for every possible input.

## Analysis limitations

- The hazard analyzer is static: it scans assembled instruction order and does not perform complete control-flow/path analysis. Its estimates can differ from dynamic execution when branches skip instructions or loops repeat them.
- `FDIV` is a binary16 instruction, not a general-purpose single- or double-precision floating-point unit. Other FP16 arithmetic functions remain available through the standalone arithmetic API.
- Adder delay figures are comparative model estimates, not measured silicon timing or area.
- The Wallace Tree is an algorithmic software model, not a gate-level circuit implementation.

## Project documentation

- [Team and assignment overview](About_RISC201_Team.pdf)
- [Pipeline visualizer guide](PIPELINE_VISUALIZER.md)
- [Arithmetic extension and division instruction details](docs/ARITHMETIC_EXTENSION.md)
- [Hazard analyzer methodology and limitations](docs/HAZARD_ANALYZER.md)
