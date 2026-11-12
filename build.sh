#!/bin/bash
# Build the RISC201 simulator from any working directory.
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O2 -Wall -Wextra -Isrc src/*.cpp -o risc201
echo "Build OK -> $(pwd)/risc201"
