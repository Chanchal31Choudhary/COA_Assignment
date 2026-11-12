#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/risc201-arithmetic.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT
g++ -std=c++17 -O2 -Wall -Wextra -Isrc src/alu.cpp src/fp16.cpp tests/arithmetic/arithmetic_tests.cpp -o "$tmp_dir/arithmetic_tests"
"$tmp_dir/arithmetic_tests"
