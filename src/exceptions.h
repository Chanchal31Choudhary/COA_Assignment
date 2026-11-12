// =====================================================================
//  exceptions.h  --  Runtime exception framework
//  OWNER : Chanchal (Member B)
//  ROLE  : One exception type used by memory protection, stack safety,
//          the control unit and the pipeline to report run-time faults.
// =====================================================================
#pragma once
#include <stdexcept>
#include <string>

enum class ExKind {
    IllegalOpcode,    // opcode has no micro-routine
    MicrocodeFault,   // micro-PC ran away / bad control-store address
    BadAddress,       // memory access outside 0..255
    StackOverflow,    // SP would go below STACK_LIMIT
    StackUnderflow,   // POP / RET on an empty stack
    StackCorruption,  // normal store into unallocated stack area (below SP)
    CodeWrite,        // store into the code segment
    BadReturn,        // RET popped an address that is not code
    PCRange,          // instruction fetch from non-code address
    CycleLimit,       // simulation exceeded the cycle limit
    DivideByZero      // integer DIV attempted with a zero divisor
};

inline const char* exName(ExKind k) {
    switch (k) {
        case ExKind::IllegalOpcode:   return "IllegalOpcode";
        case ExKind::MicrocodeFault:  return "MicrocodeFault";
        case ExKind::BadAddress:      return "BadAddress";
        case ExKind::StackOverflow:   return "StackOverflow";
        case ExKind::StackUnderflow:  return "StackUnderflow";
        case ExKind::StackCorruption: return "StackCorruption";
        case ExKind::CodeWrite:       return "CodeWrite";
        case ExKind::BadReturn:       return "BadReturn";
        case ExKind::PCRange:         return "PCRange";
        case ExKind::CycleLimit:      return "CycleLimit";
        case ExKind::DivideByZero:    return "DivideByZero";
    }
    return "Unknown";
}

struct SimException : std::runtime_error {
    ExKind kind;
    SimException(ExKind k, const std::string& msg) : std::runtime_error(msg), kind(k) {}
};
