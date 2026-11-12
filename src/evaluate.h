// =====================================================================
//  evaluate.h  --  Evaluation / comparison framework
//  OWNER : Kavya (Member D)
//  ROLE  : Runs one program on every configuration
//          {4-stage, 6-stage} x {horizontal, vertical microcode} x {forwarding off/on}
//          and compares cycles, CPI, stalls, flush penalty, micro-cycles and
//          control-store size.  Also compares the ALU algorithms.
// =====================================================================
#pragma once
#include <string>
#include "assembler.h"
#include "pipeline.h"

struct RunResult {
    bool halted = false;
    std::string status;          // "HALTED" or "EXCEPTION <kind>"
    Stats stats;
    uint64_t microCycles = 0;
    int ctlBits = 0;
    uint16_t regs[8] = {0};
    double cpi = 0;
};

RunResult runConfig(const Program& p, int stages, CtlStyle style, bool fwd, uint64_t maxCycles = 200000);
std::string compareProgram(const Program& p, const std::string& name);
std::string compareAlu();
