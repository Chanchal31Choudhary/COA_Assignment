# Static Pipeline Hazard Analyzer

## Usage

```sh
./build.sh
./risc201 --hazards tests/t9_hazards.asm
```

After loading an assembly program in the interactive simulator, use:

```text
hazards
```

The report covers 4-stage and 6-stage pipelines, each with forwarding enabled and disabled.

## How it works

1. Walk the assembled `Program` by address and inspect only words where both `used[address]` and `isCode[address]` are true. Embedded `.word` and `.space` data are skipped.
2. Decode each instruction with `isa::decodeFields()`.
3. Ask the existing `ControlUnit` for the opcode's microcode control signals. The analyzer uses the same source/destination mapping as `CPU::decodeInst()` in `pipeline.cpp` (`SG_RS1`, `SG_RS2`, `SG_RDA`, `SG_RDV`, stack-pointer signals, `SG_RDW`, and `SG_SPW`). This covers STORE/STR, branches, and stack instructions without a second hand-maintained opcode table.
4. For each source register, find the nearest earlier instruction that writes it. Report the producer/consumer pair, source lines, register, and instruction distance.
5. Classify a dependency as `LOAD-USE` only when a load dependency actually causes a read-stage stall in the selected pipeline configuration. Otherwise report it as ordinary `RAW`. Each dependency is listed once. Control transfers are separated into conditional branches and unconditional transfers. Conditional penalties are labelled `if taken`; unconditional transfers are not.
6. Show pairwise stall estimates and run a separate temporary CPU instance to display the actual data-stall/control-bubble counts on the path it executes. This validation does not alter the user's loaded CPU state.

## Interpretation and limits

- Pairwise estimates model one producer/consumer dependency in isolation using the stage placement and result-ready timing in `pipeline.cpp`. Multiple dependencies can overlap, so the sum of pairwise estimates can exceed the actual total stall cycles. The report explicitly shows both the pairwise estimate and a cycle-accurate simulator check.
- Conditional branch flush costs are potential costs, shown only as `if taken`. Unconditional transfers always redirect when reached. The redirect penalties use the stage indices in `CPU::redirect()` (EX for JMP/conditional branches/CALL, MEM for RET).
- This is a read-only analyzer. It does not insert NOPs, reorder instructions, alter labels, change encodings, or modify pipeline execution.

- For a consumer with multiple simultaneous register dependencies, estimated read-stage stalls use the maximum dependency delay for that consumer, rather than adding overlapping stalls. The simulator validation separately reports whether execution halted, faulted, or reached the cycle limit; incomplete runs are not presented as completed statistics.
