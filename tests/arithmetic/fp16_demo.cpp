#include "fp16.h"
#include <cstdint>
#include <iomanip>
#include <iostream>

static void show(const char* label, uint16_t bits) {
    std::cout << std::left << std::setw(8) << label
              << " = " << std::setw(12) << FP16::toFloat(bits)
              << " bits=0x" << std::hex << std::setw(4) << std::setfill('0')
              << bits << std::dec << std::setfill(' ') << '\n';
}

int main() {
    const uint16_t a = FP16::fromFloat(6.0f);
    const uint16_t b = FP16::fromFloat(2.0f);

    show("A", a);
    show("B", b);
    show("A + B", FP16::add(a, b));
    show("A - B", FP16::sub(a, b));
    show("A * B", FP16::mul(a, b));
    show("A / B", FP16::div(a, b));
    show("1 / 3", FP16::div(FP16::fromFloat(1.0f), FP16::fromFloat(3.0f)));
    show("1 / 0", FP16::div(FP16::fromFloat(1.0f), FP16::fromFloat(0.0f)));
    return 0;
}
