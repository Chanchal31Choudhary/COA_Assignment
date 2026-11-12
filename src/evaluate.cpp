// evaluate.cpp -- OWNER: Kavya (Member D)
#include "evaluate.h"
#include <cstdio>
#include <sstream>

RunResult runConfig(const Program& p, int stages, CtlStyle style, bool fwd, uint64_t maxCycles) {
    CPU cpu;
    cpu.configure(stages, style, fwd);
    cpu.load(p);
    cpu.run(maxCycles, false);
    RunResult r;
    r.halted = cpu.halted;
    r.status = cpu.halted ? "HALTED" : std::string("EXC:") + exName(cpu.faultKind);
    r.stats = cpu.stats;
    r.microCycles = cpu.cu.microCycles;
    r.ctlBits = cpu.cu.totalBits();
    for (int i = 0; i < 8; i++) r.regs[i] = cpu.regs.read(i);
    r.cpi = cpu.stats.retired ? (double)cpu.stats.cycles / (double)cpu.stats.retired : 0.0;
    return r;
}

std::string compareProgram(const Program& p, const std::string& name) {
    std::ostringstream o;
    char b[220];
    o << "=== Comparison for " << name << " ===\n";
    std::snprintf(b, sizeof b, " %-22s %-14s %7s %6s %6s %7s %8s %8s %9s\n",
                  "configuration", "status", "cycles", "instr", "CPI", "stalls", "flushBub", "uCycles", "ctlBits");
    o << b;
    RunResult base;
    bool haveBase = false, same = true;
    uint64_t c4 = 0, c6 = 0;
    for (int stages : {4, 6})
        for (int sty = 0; sty < 2; sty++)
            for (int fw = 0; fw < 2; fw++) {
                CtlStyle style = sty == 0 ? CtlStyle::Horizontal : CtlStyle::Vertical;
                RunResult r = runConfig(p, stages, style, fw == 1);
                std::string cfg = std::to_string(stages) + "-stage " + (sty == 0 ? "H" : "V") + " fwd=" + (fw ? "on " : "off");
                std::snprintf(b, sizeof b, " %-22s %-14s %7llu %6llu %6.2f %7llu %8llu %8llu %9d\n", cfg.c_str(), r.status.c_str(),
                              (unsigned long long)r.stats.cycles, (unsigned long long)r.stats.retired, r.cpi,
                              (unsigned long long)r.stats.dataStalls, (unsigned long long)r.stats.controlBubbles,
                              (unsigned long long)r.microCycles, r.ctlBits);
                o << b;
                if (!haveBase) { base = r; haveBase = true; }
                else {
                    if (r.status != base.status) same = false;
                    for (int i = 0; i < 8; i++) if (r.regs[i] != base.regs[i]) same = false;
                }
                if (sty == 0 && fw == 1) { if (stages == 4) c4 = r.stats.cycles; else c6 = r.stats.cycles; }
            }
    o << " Architectural results (registers + status) identical in all 8 configurations: " << (same ? "YES" : "NO  <-- BUG") << "\n";
    if (c4 && c6) {
        std::snprintf(b, sizeof b, " 6-stage / 4-stage cycle ratio (H, fwd on): %.2f  (>1 means the deeper pipeline needs MORE cycles;\n"
                                   "  it only wins if its shorter clock period outweighs the extra cycles)\n", (double)c6 / (double)c4);
        o << b;
    }
    return o.str();
}

std::string compareAlu() {
    std::ostringstream o;
    char b[200];
    uint32_t x = 12345;
    auto rnd = [&]() { x = x * 1664525u + 1013904223u; return (uint16_t)(x >> 16); };

    // adders
    int mismatch = 0;
    for (int i = 0; i < 20000; i++) {
        uint16_t a = rnd(), c = rnd();
        bool co1, c151, co2, c152;
        uint16_t s1 = ALU::addRipple(a, c, false, co1, c151);
        uint16_t s2 = ALU::addLookahead(a, c, false, co2, c152);
        if (s1 != s2 || co1 != co2 || c151 != c152) mismatch++;
    }
    o << "=== ALU comparison ===\n";
    o << "Adders (20000 random operand pairs):\n";
    std::snprintf(b, sizeof b, "  ripple-carry     : %d gate delays per add   (results differ from CLA: %d)\n", ALU::addDelay(AddAlgo::Ripple), mismatch);
    o << b;
    std::snprintf(b, sizeof b, "  carry-lookahead  : %d gate delays per add   (Kogge-Stone prefix network)\n", ALU::addDelay(AddAlgo::Lookahead));
    o << b;

    // multipliers
    long long sumSA = 0, sumBooth = 0;
    int mm = 0;
    for (int i = 0; i < 20000; i++) {
        uint16_t a = rnd(), c = rnd();
        int a1, a2;
        uint16_t p1 = ALU::mulShiftAdd(a, c, a1);
        uint16_t p2 = ALU::mulBooth(a, c, a2);
        if (p1 != p2 || p1 != (uint16_t)(a * c)) mm++;
        sumSA += a1; sumBooth += a2;
    }
    o << "Multipliers (20000 random operand pairs, 16 iterations each):\n";
    std::snprintf(b, sizeof b, "  shift-and-add    : %.2f additions per multiply  (results wrong: %d)\n", sumSA / 20000.0, mm);
    o << b;
    std::snprintf(b, sizeof b, "  Booth radix-2    : %.2f add/sub per multiply\n", sumBooth / 20000.0);
    o << b;
    int s1, s2;
    ALU::mulShiftAdd(1000, 0x7FFF, s1);
    ALU::mulBooth(1000, 0x7FFF, s2);
    std::snprintf(b, sizeof b, "  worst/best case  : multiplier 0x7FFF (long run of 1s): shift-add=%d ops, Booth=%d ops\n", s1, s2);
    o << b;
    o << "  Shifter          : barrel shifter, 4 mux levels for any shift amount 0..15\n";
    // New architectures: compare against the existing reference implementations.
    int csBad=0,bkBad=0,wallaceBad=0; long long wallaceOps=0;
    for(int i=0;i<20000;i++){
        uint16_t a=rnd(),b=rnd(); bool cA,cB,cC,cD,cE,cF;
        uint16_t ref=ALU::addRipple(a,b,false,cA,cB);
        if(ALU::addCarrySelect(a,b,false,cC,cD)!=ref || cC!=cA || cD!=cB) csBad++;
        if(ALU::addBrentKung(a,b,false,cE,cF)!=ref || cE!=cA || cF!=cB) bkBad++;
        int ops=0; if(ALU::mulWallace(a,b,ops)!=(uint16_t)((uint32_t)a*b)) wallaceBad++; wallaceOps+=ops;
    }
    o << "Additional architectures (20000 random operand pairs):\\n";
    std::snprintf(b,sizeof b,"  carry-select     : %d mismatches; modelled delay %d\\n",csBad,ALU::addDelay(AddAlgo::CarrySelect)); o<<b;
    std::snprintf(b,sizeof b,"  Brent-Kung       : %d mismatches; modelled delay %d\\n",bkBad,ALU::addDelay(AddAlgo::BrentKung)); o<<b;
    std::snprintf(b,sizeof b,"  Wallace tree     : %d mismatches; average compressor reductions %.2f\\n",wallaceBad,wallaceOps/20000.0); o<<b;
    o << "  FP16 / FDIV      : binary16 add/subtract/multiply/divide; FDIV is CPU-integrated\\n";

    return o.str();
}
