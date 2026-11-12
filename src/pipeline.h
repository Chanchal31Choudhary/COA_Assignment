// =====================================================================
//  pipeline.h  --  4-stage and 6-stage pipelined RISC201 CPU
//  OWNER : Kavya (Member D)
//  ROLE  : Cycle-accurate in-order pipeline that ties together the
//          register file, memory, ALU and the micro-programmed control unit.
//
//   4-stage :  IF | ID (decode + register read) | EX (ALU + memory) | WB
//   6-stage :  IF | ID (decode)  | RF (register read) | EX | MEM | WB
//
//  Hazards
//    * RAW data hazards : interlock (stall in the register-read stage), with an
//      optional forwarding network (--fwd on/off).
//    * Control hazards  : branches/jumps/CALL resolve in EX (RET in MEM for the
//      6-stage design); no prediction, younger instructions are flushed.
//  Exceptions are PRECISE: on a fault younger instructions are discarded and
//  older ones are allowed to finish before the machine stops.
// =====================================================================
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include "alu.h"
#include "assembler.h"
#include "exceptions.h"
#include "memory.h"
#include "microcode.h"

struct Dst { int reg = -1; uint16_t val = 0; bool ready = false; };

struct Inst {
    bool valid = false;
    uint64_t seq = 0;
    uint16_t pc = 0, raw = 0;
    bool decoded = false;
    isa::Fields f{};
    ControlWord cw;
    std::string fault;
    ExKind faultKind = ExKind::IllegalOpcode;
    bool need[8] = {false};
    uint16_t rv[8] = {0};
    Dst dst[2];
    int nd = 0, dRd = -1, dSp = -1;
    uint16_t val = 0, addr = 0, loaded = 0, spNew = 0, target = 0;
    bool taken = false;
    bool stalled = false;
    bool fetchBad = false;      // instruction fetch itself faulted
};

// ---- structured, per-cycle observation API (used by the pipeline visualizer) ----
// Filled by CPU::step(); it only records what the pipeline already did, it never
// influences timing or results.
struct StageView {                 // what occupied a stage DURING the cycle
    bool valid = false;
    uint64_t seq = 0;              // unique, increasing instruction number (fetch order)
    uint16_t pc = 0;
    bool fetchBad = false;
    std::string text;              // disassembly
};

enum class EvKind {
    Stall,      // RAW data hazard, instruction held in its register-read stage
    Forward,    // RAW data hazard resolved by the forwarding network
    Redirect,   // control hazard: taken branch/jump/CALL/RET changed the PC
    Flush,      // younger instructions discarded (branch, HALT or exception)
    Halt,       // HALT reached EX: fetch stopped
    Exception,  // precise exception raised
    Retire      // instruction left the last stage and committed
};

struct PipeEvent {
    EvKind kind = EvKind::Retire;
    int stage = -1;                // stage index where it happened (consumer / branch / faulting)
    uint64_t seq = 0;              // instruction concerned (consumer / branch / retiree)
    uint64_t other = 0;            // producer instruction (Stall / Forward)
    int reg = -1;                  // register concerned (Forward) -- Stall uses regMask
    unsigned regMask = 0;          // bit r set => hazard on register r (Stall)
    uint16_t target = 0;           // new PC (Redirect)
    std::vector<uint64_t> seqs;    // flushed instruction numbers (Flush)
    std::string text;              // exception message
};

struct Stats {
    uint64_t cycles = 0, retired = 0, dataStalls = 0, flushedInsts = 0, controlBubbles = 0;
    uint64_t branchesTaken = 0, loads = 0, stores = 0;
};

class CPU {
public:
    CPU();
    void configure(int stages, CtlStyle style, bool forwarding);   // resets the machine
    void load(const Program& p);
    void reset();

    bool step();                                   // one clock cycle; false when finished
    void run(uint64_t maxCycles, bool useBreakpoints);
    bool stepInstruction();                        // until one instruction retires

    int stages() const { return N; }
    bool forwarding() const { return fwd; }
    const char* stageName(int i) const;
    std::string pipelineText() const;
    std::string chartText(int maxCols = 48) const;
    bool loaded() const { return hasProg; }
    const Program& program() const { return prog; }

    Memory mem;
    RegisterFile regs;
    ALU alu;
    ControlUnit cu;
    Stats stats;

    uint16_t pc = 0;
    bool halted = false, faulted = false;
    ExKind faultKind = ExKind::CycleLimit;
    std::string faultMsg;
    uint16_t faultPC = 0;

    bool upcTrace = false;                         // show micro-PC state at decode
    std::vector<std::string> log;                  // events of the last cycle
    std::set<int> breakpoints;
    bool hitBreak = false;
    int lastFetch = -1;

    // per-cycle observation (valid after step())
    std::vector<StageView> occupancy;              // stage contents during the cycle just simulated
    std::vector<PipeEvent> events;                 // hazards / flushes / retires of that cycle

private:
    int N = 4, iDec = 1, iRead = 1, iEX = 2, iMEM = 2;
    bool fwd = true, hasProg = false, fetchOn = true, faultPending = false;
    ExKind pendKind = ExKind::CycleLimit;
    std::string pendMsg;
    uint16_t pendPC = 0;
    uint64_t seqCtr = 0;
    Program prog;
    std::vector<Inst> st;

    struct RowInfo { uint16_t pc; std::string text; };
    std::map<uint64_t, std::map<uint64_t, std::string>> chart;   // seq -> cycle -> stage
    std::map<uint64_t, RowInfo> rows;
    std::set<uint64_t> flushedSeq;

    void fetchInst();
    void snapshotChart();
    void captureOccupancy();
    bool stageWork(int i);
    void decodeInst(Inst& in);
    bool readOperands(Inst& in, int stageIdx);
    void executeInst(Inst& in);
    void memoryInst(Inst& in);
    void doWB();
    void redirect(int idx, uint16_t target);
    void flushYounger(int idx);
    void handleFault(int idx, const SimException& e, const Inst& in);
    bool pipeEmpty() const;
    std::string instText(const Inst& in) const;
};
