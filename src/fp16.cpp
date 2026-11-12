#include "fp16.h"
#include <cstring>
#include <cmath>
#include <limits>

namespace {
// Convert a finite/non-finite double directly to binary16 using
// round-to-nearest, ties-to-even. This avoids an intermediate float
// rounding step in division.
uint16_t doubleToHalf(double value) {
    const uint16_t sign = std::signbit(value) ? 0x8000u : 0u;
    const double magnitude = std::fabs(value);

    if (std::isnan(value)) return static_cast<uint16_t>(sign | 0x7e00u);
    if (std::isinf(value)) return static_cast<uint16_t>(sign | 0x7c00u);
    if (magnitude == 0.0) return sign;

    // 65520 is the round-to-nearest overflow threshold between max finite
    // binary16 (65504) and infinity.
    if (magnitude >= 65520.0) return static_cast<uint16_t>(sign | 0x7c00u);

    if (magnitude < 0x1p-14) {
        // Subnormal quantum is 2^-24. The rounded integer may become 1024,
        // which is exactly the smallest normal binary16 value.
        const double scaled = std::ldexp(magnitude, 24);
        const double floorValue = std::floor(scaled);
        uint32_t rounded = static_cast<uint32_t>(floorValue);
        const double fraction = scaled - floorValue;
        if (fraction > 0.5 || (fraction == 0.5 && (rounded & 1u))) ++rounded;
        return static_cast<uint16_t>(sign | rounded);
    }

    int exponent = 0;
    const double fraction = std::frexp(magnitude, &exponent);
    // frexp gives [0.5, 1); scale to the 11-bit significand [1024, 2048).
    const double significand = std::ldexp(fraction, 11);
    const double floorValue = std::floor(significand);
    uint32_t rounded = static_cast<uint32_t>(floorValue);
    const double remainder = significand - floorValue;
    if (remainder > 0.5 || (remainder == 0.5 && (rounded & 1u))) ++rounded;

    int unbiasedExponent = exponent - 1;
    if (rounded == 2048u) {
        rounded = 1024u;
        ++unbiasedExponent;
    }
    if (unbiasedExponent > 15) return static_cast<uint16_t>(sign | 0x7c00u);

    const uint16_t biasedExponent = static_cast<uint16_t>(unbiasedExponent + 15);
    return static_cast<uint16_t>(sign | (biasedExponent << 10) | (rounded - 1024u));
}
}

uint16_t FP16::fromFloat(float value) {
    uint32_t x; std::memcpy(&x, &value, sizeof x);
    uint16_t sign = (uint16_t)((x >> 16) & 0x8000);
    uint32_t exp = (x >> 23) & 0xff, frac = x & 0x7fffff;
    if (exp == 0xff) return (uint16_t)(sign | (frac ? 0x7e00 : 0x7c00));
    int e = (int)exp - 127 + 15;
    if (e >= 31) return (uint16_t)(sign | 0x7c00);
    if (e <= 0) {
        if (e < -10) return sign;
        frac |= 0x800000;
        int shift = 14 - e;
        uint32_t base = frac >> shift, rem = frac & ((1u << shift)-1), halfway = 1u << (shift-1);
        if (rem > halfway || (rem == halfway && (base & 1))) ++base;
        return (uint16_t)(sign | base);
    }
    uint32_t base = frac >> 13, rem = frac & 0x1fff;
    if (rem > 0x1000 || (rem == 0x1000 && (base & 1))) {
        ++base; if (base == 0x400) { base=0; ++e; if(e>=31) return (uint16_t)(sign|0x7c00); }
    }
    return (uint16_t)(sign | ((uint16_t)e << 10) | (uint16_t)base);
}

float FP16::toFloat(uint16_t h) {
    uint32_t sign = (uint32_t)(h & 0x8000) << 16;
    uint32_t exp = (h >> 10) & 0x1f, frac = h & 0x3ff, out;
    if (exp == 0) {
        if (frac == 0) out = sign;
        else { int e=-14; while((frac & 0x400)==0){frac<<=1;--e;} frac &= 0x3ff; out=sign | (uint32_t)(e+127)<<23 | frac<<13; }
    } else if(exp==31) out=sign | 0x7f800000 | frac<<13;
    else out=sign | (exp-15+127)<<23 | frac<<13;
    float f; std::memcpy(&f,&out,sizeof f); return f;
}
uint16_t FP16::add(uint16_t a,uint16_t b){ return fromFloat(toFloat(a)+toFloat(b)); }
uint16_t FP16::sub(uint16_t a,uint16_t b){ return fromFloat(toFloat(a)-toFloat(b)); }
uint16_t FP16::mul(uint16_t a,uint16_t b){ return fromFloat(toFloat(a)*toFloat(b)); }

uint16_t FP16::div(uint16_t a, uint16_t b) {
    const double numerator = static_cast<double>(toFloat(a));
    const double denominator = static_cast<double>(toFloat(b));

    // Spell out IEEE-754 division corner cases to make signed-zero and
    // invalid-operation behaviour explicit and independent of host traps.
    const bool sign = std::signbit(numerator) != std::signbit(denominator);
    const uint16_t signBit = sign ? 0x8000u : 0u;
    const bool numeratorZero = numerator == 0.0;
    const bool denominatorZero = denominator == 0.0;
    const bool numeratorInf = std::isinf(numerator);
    const bool denominatorInf = std::isinf(denominator);
    const bool numeratorNaN = std::isnan(numerator);
    const bool denominatorNaN = std::isnan(denominator);

    if (numeratorNaN || denominatorNaN ||
        (numeratorZero && denominatorZero) ||
        (numeratorInf && denominatorInf)) {
        return 0x7e00u; // canonical quiet NaN
    }
    if (numeratorInf || denominatorZero) return static_cast<uint16_t>(signBit | 0x7c00u);
    if (numeratorZero || denominatorInf) return signBit;

    // Binary16 inputs are exactly representable in double. The quotient is
    // rounded directly from double to binary16 (not via float).
    return doubleToHalf(numerator / denominator);
}
