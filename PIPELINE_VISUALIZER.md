# RISC201 Pipeline Visualizer

The `pipeline` command opens a separate visualizer instance for the currently
loaded program. It does not alter the main simulator's registers, memory, or
execution position.

## Moving animation

Choose a 4-stage or 6-stage pipeline, then use:

- `run` — continuously animate instruction tokens moving between stages.
- `run 250` — use a 250 ms cycle duration.
- `run 250 20` — animate at 250 ms per cycle, then stop after 20 cycles.
- `speed 250` — set the default cycle duration for later `run` commands.
- Press Enter or type `pause` during playback to pause.
- `next [n]` / `prev [n]` — inspect exact cycle snapshots.
- `reset` — restart the visualizer from cycle zero.
- `switch` — switch between 4-stage and 6-stage modes.

During playback, the terminal redraws intermediate animation frames between
real CPU cycles. These are presentation frames only: the CPU advances exactly
once per cycle. Instructions move according to the recorded stage occupancy;
a stalled instruction stays in place, flushed instructions are marked and
removed locally, and retired instructions leave the final stage.

For the clearest animation, run in a terminal (not a captured/non-interactive
console) with a reasonably wide window. `run 0` intentionally disables the
transition animation and prints cycle snapshots immediately.
