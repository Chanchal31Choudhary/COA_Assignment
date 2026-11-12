#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
[ -x ./risc201 ] || ./build.sh
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/risc201-hazards.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT
./risc201 --hazards tests/hazards/no_hazards.asm > "$tmp_dir/no.txt"
grep -q "No hazards detected" "$tmp_dir/no.txt"
./risc201 --hazards tests/hazards/distance_1_2.asm > "$tmp_dir/dist.txt"
grep -q "distance=1" "$tmp_dir/dist.txt"
grep -q "distance=2" "$tmp_dir/dist.txt"
./risc201 --hazards tests/hazards/data_words.asm > "$tmp_dir/data.txt"
grep -q "distance=1" "$tmp_dir/data.txt"
! grep -q "line 2.*word" "$tmp_dir/data.txt"
./risc201 --hazards tests/hazards/stack_sp.asm > "$tmp_dir/sp.txt"
grep -q "reg=R7/SP" "$tmp_dir/sp.txt"
./risc201 --hazards tests/hazards/load_use.asm > "$tmp_dir/load.txt"
grep -q "LOAD-USE" "$tmp_dir/load.txt"
./risc201 --hazards tests/hazards/control.asm > "$tmp_dir/control.txt"
grep -q "CONDITIONAL CONTROL" "$tmp_dir/control.txt"
grep -q "UNCONDITIONAL CONTROL" "$tmp_dir/control.txt"
grep -q "flush penalty" "$tmp_dir/control.txt"
./risc201 --hazards tests/t9_hazards.asm > "$tmp_dir/t9.txt"
grep -q "Simulator check status: HALTED successfully" "$tmp_dir/t9.txt"
grep -q "Simulator statistics (completed execution): data stalls=5" "$tmp_dir/t9.txt"
grep -q "Simulator statistics (completed execution): data stalls=1" "$tmp_dir/t9.txt"
grep -q "Simulator statistics (completed execution): data stalls=10" "$tmp_dir/t9.txt"
echo "Hazard analyzer tests: PASS (no hazards, RAW distances 1/2, data-word skipping, SP dependencies, load-use, control, t9 pipeline cross-check)"
