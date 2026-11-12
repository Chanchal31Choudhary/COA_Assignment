#!/bin/bash
# =====================================================================
#  run_tests.sh -- regression test for the whole RISC201 suite   (Team)
#  Runs every program in tests/ on ALL 8 configurations
#     {4-stage, 6-stage} x {horizontal, vertical} x {forwarding on, off}
#  and checks that the architectural result (status + registers + memory)
#  equals tests/expected/<name>.txt in every configuration.
#  Usage:  ./run_tests.sh            check
#          ./run_tests.sh --update   regenerate expected files (4-stage, H, fwd on)
# =====================================================================
cd "$(dirname "$0")"
[ -x ./risc201 ] || ./build.sh || exit 1
pass=0; fail=0
for f in tests/t*.asm tests/e*.asm tests/e*.hex; do
    [ -f "$f" ] || continue
    name=$(basename "$f"); name=${name%.*}
    exp="tests/expected/$name.txt"
    if [ "$1" == "--update" ]; then
        ./risc201 --batch "$f" --mem 100 10 > "$exp"
        echo "updated $exp"; continue
    fi
    ok=1
    for st in 4 6; do for ct in h v; do for fw in on off; do
        out=$(./risc201 --batch "$f" --stages $st --ctl $ct --fwd $fw --mem 100 10)
        if [ "$out" != "$(cat $exp)" ]; then
            ok=0; echo "  MISMATCH: $name  (stages=$st ctl=$ct fwd=$fw)"
        fi
    done; done; done
    if [ $ok -eq 1 ]; then pass=$((pass+1)); echo "PASS  $name  (8/8 configurations)"; else fail=$((fail+1)); echo "FAIL  $name"; fi
done
if [ "${1:-}" = "--update" ]; then exit 0; fi
echo "----------------------------------------"
echo "programs passed: $pass   failed: $fail"
if [ $fail -ne 0 ]; then exit 1; fi
./tests/toolchain/run.sh || exit 1
./tests/viz/run.sh || exit 1
