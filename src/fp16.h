#pragma once
#include <cstdint>

// Standalone IEEE-754 binary16 arithmetic. Not wired to the RISC201 ISA/pipeline.
class FP16 {
public:
    static uint16_t fromFloat(float value); // round-to-nearest, ties-to-even
    static float toFloat(uint16_t bits);
    static uint16_t add(uint16_t a, uint16_t b);
    static uint16_t sub(uint16_t a, uint16_t b);
    static uint16_t mul(uint16_t a, uint16_t b);
    static uint16_t div(uint16_t a, uint16_t b);
};
