#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
[ -x ./risc201 ] || ./build.sh
for scenario in tests/viz/v*.txt; do
    output=$(mktemp "${TMPDIR:-/tmp}/risc201-viz.XXXXXX")
    trap 'rm -f "$output"' EXIT
    ./risc201 --script "$scenario" >"$output" 2>&1
    grep -q "RISC201 PIPELINE VISUALIZER" "$output"
    grep -q "Leaving pipeline visualizer" "$output"
    case "$(basename "$scenario")" in
        v1_*|v3_*|v4_*) grep -q "STALL" "$output" ;;
        v2_*) grep -q "FORWARDING" "$output" ;;
        v5_*|v6_*) grep -q "FLUSH" "$output" ;;
        v7_*) grep -q "Invalid choice" "$output" ;;
    esac
    echo "PASS  $(basename "$scenario")"
    rm -f "$output"
    trap - EXIT
done
echo "Pipeline visualizer scenarios: PASS"
