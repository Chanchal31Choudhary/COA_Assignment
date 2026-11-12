#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/risc201-toolchain.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT
g++ -std=c++17 -O2 -Wall -Wextra -Isrc src/assembler.cpp src/preprocessor.cpp src/disassembler.cpp tests/toolchain/toolchain_tests.cpp -o "$tmp_dir/toolchain_tests"
shopt -s nullglob
files=(tests/t*.asm tests/e*.asm tests/viz/*.asm)
"$tmp_dir/toolchain_tests" "${files[@]}"
