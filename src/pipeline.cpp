// pipeline.cpp -- OWNER: Kavya (Member D)
#include "pipeline.h"
#include "disassembler.h"
#include <cstdio>
#include <sstream>

using namespace isa;

static std::string hex2(int v) { char b[16]; std::snprintf(b, sizeof b, "0x%02X", v & 0xFFFF); return b; }
static std::string regN(int r) { return r == SP_REG ? "SP" : "R" + std::to_string(r); }

CPU::CPU() { configure(4, CtlStyle::Horizontal, true); }

void CPU::configure(int stages, CtlStyle style, bool forwarding) {
    N = (stages == 6) ? 6 : 4;
    if (N == 4) { iDec = 1; iRead = 1; iEX = 2; iMEM = 2; }
    else        { iDec = 1; iRead = 2; iEX = 3; iMEM = 4; }
    cu.setStyle(style);
    fwd = forwarding;
    reset();
}

void CPU::load(const Program& p) {
    prog = p;
    hasProg = true;
    reset();
}

void CPU::reset() {
    mem.clear();
    regs.reset();
    if (hasProg)
        for (int a = 0; a < MEM_WORDS; a++)
            if (prog.used[a]) { mem.poke(a, prog.mem[a]); mem.setCode(a, prog.isCode[a]); }
    pc = 0;
    st.assign(N, Inst());
    halted = faulted = faultPending = false;
    fetchOn = true;
    stats = Stats();
    alu.resetStats();
    cu.microCycles = cu.dispatches = 0;
    seqCtr = 0;
    chart.clear(); rows.clear(); flushedSeq.clear();
    log.clear();
    events.clear();
    occupancy.assign(N, StageView());
    hitBreak = false;
    lastFetch = -1;
    faultMsg.clear();
}

const char* CPU::stageName(int i) const {
    static const char* n4[] = {"IF", "ID", "EX", "WB"};
    static const char* n6[] = {"IF", "ID", "RF", "EX", "MEM", "WB"};
    return N == 4 ? n4[i] : n6[i];
}

std::string CPU::instText(const Inst& in) const {
    std::string s = "[" + std::to_string(in.pc) + "] ";
    if (in.fetchBad) return s + "<fetch fault>";
    return s + disasm(in.raw);
}

bool CPU::pipeEmpty() const {
    for (auto& i : st) if (i.valid) return false;
    return true;
}

// ---------------------------------------------------------------- fetch
void CPU::fetchInst() {
    Inst in;
    in.valid = true;
    in.seq = ++seqCtr;
    in.pc = pc;
    if (pc >= MEM_WORDS) {
        in.fault = "instruction fetch from " + hex2(pc) + " (outside memory)";
        in.faultKind = ExKind::PCRange;
        in.fetchBad = true;
    } else if (!mem.isCode(pc)) {
        in.fault = "instruction fetch from non-code address " + hex2(pc) + " (missing HALT?)";
        in.faultKind = ExKind::PCRange;
        in.fetchBad = true;
    } else in.raw = mem.peek(pc);
    lastFetch = pc;
    pc++;
    st[0] = in;
    rows[in.seq] = RowInfo{in.pc, in.fetchBad ? std::string("<bad fetch>") : disasm(in.raw)};
    log.push_back("IF   fetch " + instText(in));
}

void CPU::captureOccupancy() {
    occupancy.assign(N, StageView());
    for (int i = 0; i < N; i++) {
        if (!st[i].valid) continue;
        occupancy[i].valid = true;
        occupancy[i].seq = st[i].seq;
        occupancy[i].pc = st[i].pc;
        occupancy[i].fetchBad = st[i].fetchBad;
        occupancy[i].text = st[i].fetchBad ? std::string("<bad fetch>") : disasm(st[i].raw);
    }
}

void CPU::snapshotChart() {
    if (chart.size() > 4000) return;
    for (int i = 0; i < N; i++)
        if (st[i].valid) chart[st[i].seq][stats.cycles] = stageName(i);
}

// ---------------------------------------------------------------- decode (micro-programmed)
void CPU::decodeInst(Inst& in) {
    if (in.decoded) return;
    in.decoded = true;
    if (!in.fault.empty()) return;
    in.f = decodeFields(in.raw);
    std::vector<std::string> tr;
    ControlWord cw = cu.fetch(in.f.op, &tr);
    if (upcTrace) {
        log.push_back(std::string(stageName(iDec)) + "   uCODE " + instText(in) + "   (" +
                      (cu.style() == CtlStyle::Horizontal ? "horizontal" : "vertical") + " control store)");
        for (auto& t : tr) log.push_back("          " + t);
    } else {
        log.push_back(std::string(stageName(iDec)) + "   decode " + instText(in) + "  ctl={" + sigNames(cw.sig) + "}");
    }
    if (!cw.valid) { in.fault = cw.err; in.faultKind = cw.kind; return; }
    in.cw = cw;
    uint32_t s = cw.sig;
    if (s & (SG_RS1 | SG_ADRR)) in.need[in.f.rs1] = true;
    if (s & SG_RS2) in.need[in.f.rs2] = true;
    if (s & (SG_RDA | SG_RDV)) in.need[in.f.rd] = true;
    if (s & (SG_SPR | SG_ADRSP | SG_ADRSPM | SG_SPUP | SG_SPDN)) in.need[SP_REG] = true;
    if (s & SG_RDW) { in.dRd = in.nd; in.dst[in.nd++].reg = in.f.rd; }
    if (s & SG_SPW) { in.dSp = in.nd; in.dst[in.nd++].reg = SP_REG; }
}

// ---------------------------------------------------------------- register read + hazard check
bool CPU::readOperands(Inst& in, int idx) {
    if (!in.fault.empty()) return true;           // fault will be raised in EX
    bool hazard = false;
    std::string why;
    unsigned hazMask = 0;
    uint64_t hazProducer = 0;
    for (int r = 0; r < NUM_REGS; r++) {
        if (!in.need[r]) continue;
        bool found = false;
        for (int j = idx + 1; j < N && !found; j++) {
            const Inst& p = st[j];
            if (!p.valid) continue;
            for (int k = 0; k < p.nd; k++) {
                if (p.dst[k].reg != r) continue;
                found = true;
                if (fwd && p.dst[k].ready) {
                    in.rv[r] = p.dst[k].val;
                    log.push_back(std::string(stageName(idx)) + "   forward " + regN(r) + " from " + stageName(j) + " (" + instText(p) + ")");
                    PipeEvent ev; ev.kind = EvKind::Forward; ev.stage = idx; ev.seq = in.seq; ev.other = p.seq; ev.reg = r;
                    events.push_back(ev);
                } else {
                    if (!hazard) hazProducer = p.seq;
                    hazard = true; why += " " + regN(r); hazMask |= 1u << r;
                }
                break;
            }
        }
        if (!found) in.rv[r] = regs.read(r);
    }
    if (hazard) {
        stats.dataStalls++;
        in.stalled = true;
        chart[in.seq][stats.cycles] += "*";
        PipeEvent ev; ev.kind = EvKind::Stall; ev.stage = idx; ev.seq = in.seq; ev.other = hazProducer; ev.regMask = hazMask;
        events.push_back(ev);
        log.push_back(std::string(stageName(idx)) + "   STALL (RAW hazard on" + why + ") " + instText(in));
        return false;
    }
    in.stalled = false;
    regs.reads++;
    return true;
}

// ---------------------------------------------------------------- flush / redirect
void CPU::flushYounger(int idx) {
    PipeEvent ev; ev.kind = EvKind::Flush; ev.stage = idx;
    for (int i = 0; i < idx; i++) {
        if (st[i].valid) { stats.flushedInsts++; flushedSeq.insert(st[i].seq); ev.seqs.push_back(st[i].seq); st[i].valid = false; }
    }
    if (!ev.seqs.empty()) events.push_back(ev);
}

void CPU::redirect(int idx, uint16_t target) {
    int n = 0;
    for (int i = 0; i < idx; i++) if (st[i].valid) n++;
    { PipeEvent ev; ev.kind = EvKind::Redirect; ev.stage = idx; ev.seq = st[idx].seq; ev.target = target; events.push_back(ev); }
    flushYounger(idx);
    stats.controlBubbles += idx;              // idx cycles of fetch work are lost
    pc = target;
    log.push_back(std::string(stageName(idx)) + "   redirect PC -> " + std::to_string(target) + ", flushed " + std::to_string(n) + " instr(s)");
}

// ---------------------------------------------------------------- execute
void CPU::executeInst(Inst& in) {
    if (!in.fault.empty()) throw SimException(in.faultKind, in.fault);
    const Fields& f = in.f;
    uint32_t s = in.cw.sig;
    uint16_t A = 0, B = 0, val = 0, res = 0;
    if (s & SG_RS1) A = in.rv[f.rs1]; else if (s & SG_RDA) A = in.rv[f.rd];
    if (s & SG_RS2) B = in.rv[f.rs2]; else if (s & SG_IMM) B = (uint16_t)sext8(f.imm8);
    if (s & SG_RDV) val = in.rv[f.rd];
    if (s & SG_LINK) val = (uint16_t)(in.pc + 1);
    uint16_t sp = in.rv[SP_REG];
    if (s & SG_ALU) res = alu.execute(in.cw.alu, A, B).value;

    uint16_t addr = 0;
    if (s & SG_ADRI) addr = (uint16_t)f.imm8;
    else if (s & SG_ADRR) addr = in.rv[f.rs1];
    else if (s & SG_ADRSP) addr = sp;
    else if (s & SG_ADRSPM) addr = (uint16_t)(sp - 1);

    // ---- stack bounds verification ----
    if ((s & SG_ADRSPM) && sp <= STACK_LIMIT)
        throw SimException(ExKind::StackOverflow, "push with SP=" + hex2(sp) + " would go below STACK_LIMIT " + hex2(STACK_LIMIT));
    if ((s & SG_ADRSP) && sp >= STACK_TOP)
        throw SimException(ExKind::StackUnderflow, "pop/ret with empty stack (SP=" + hex2(sp) + ")");

    uint16_t spNew = sp;
    if (s & SG_SPUP) spNew = (uint16_t)(sp + 1);
    if (s & SG_SPDN) spNew = (uint16_t)(sp - 1);

    in.val = val; in.addr = addr; in.spNew = spNew;
    if (in.dRd >= 0 && !(s & SG_MEMR)) { in.dst[in.dRd].val = res; in.dst[in.dRd].ready = true; }
    if (in.dSp >= 0) { in.dst[in.dSp].val = spNew; in.dst[in.dSp].ready = true; }

    bool taken = false;
    if (s & SG_JMP) taken = true;
    else if (s & SG_BR) {
        switch (in.cw.cond) {
            case Cond::Z:   taken = (val == 0); break;
            case Cond::NZ:  taken = (val != 0); break;
            case Cond::NEG: taken = ((int16_t)val < 0); break;
            default: break;
        }
    }
    if (taken) {
        in.taken = true;
        in.target = (uint16_t)f.imm8;
        stats.branchesTaken++;
        redirect(iEX, in.target);
    }
    if (s & SG_HALT) {
        { PipeEvent ev; ev.kind = EvKind::Halt; ev.stage = iEX; ev.seq = in.seq; events.push_back(ev); }
        flushYounger(iEX);
        fetchOn = false;
        log.push_back(std::string(stageName(iEX)) + "   HALT reached execute: fetch stopped");
    }
}

// ---------------------------------------------------------------- memory
void CPU::memoryInst(Inst& in) {
    uint32_t s = in.cw.sig;
    if (s & SG_MEMW) {
        mem.write(in.addr, in.val, (s & SG_STK) != 0, in.rv[SP_REG]);
        stats.stores++;
    }
    if (s & SG_MEMR) {
        in.loaded = mem.read(in.addr);
        stats.loads++;
        if (in.dRd >= 0) { in.dst[in.dRd].val = in.loaded; in.dst[in.dRd].ready = true; }
        if (s & SG_RETJ) {
            if (!mem.isCode(in.loaded))
                throw SimException(ExKind::BadReturn, "RET popped " + hex2(in.loaded) + " which is not a code address (stack corrupted?)");
            in.taken = true;
            in.target = in.loaded;
            stats.branchesTaken++;
            redirect(iMEM, in.loaded);
        }
    }
}

// ---------------------------------------------------------------- write back
void CPU::doWB() {
    Inst in = st[N - 1];
    if (!in.valid) return;
    st[N - 1].valid = false;
    try {
        for (int k = 0; k < in.nd; k++) {
            regs.write(in.dst[k].reg, in.dst[k].val);
            if (in.dst[k].reg == SP_REG) mem.notifySP(in.dst[k].val);
        }
    } catch (SimException& e) {
        handleFault(N - 1, e, in);
        return;
    }
    stats.retired++;
    { PipeEvent ev; ev.kind = EvKind::Retire; ev.stage = N - 1; ev.seq = in.seq; events.push_back(ev); }
    log.push_back(std::string(stageName(N - 1)) + "   retire " + instText(in));
    if (in.cw.sig & SG_HALT) halted = true;
}

// ---------------------------------------------------------------- precise exceptions
void CPU::handleFault(int idx, const SimException& e, const Inst& in) {
    PipeEvent fl; fl.kind = EvKind::Flush; fl.stage = idx;
    for (int i = 0; i < idx; i++) if (st[i].valid) { flushedSeq.insert(st[i].seq); fl.seqs.push_back(st[i].seq); st[i].valid = false; }
    if (!fl.seqs.empty()) events.push_back(fl);
    { PipeEvent ev; ev.kind = EvKind::Exception; ev.stage = idx; ev.seq = in.seq; ev.text = std::string(exName(e.kind)) + ": " + e.what(); events.push_back(ev); }
    st[idx].valid = false;
    faultPending = true;
    fetchOn = false;
    pendKind = e.kind;
    pendPC = in.pc;
    pendMsg = std::string(e.what()) + "  [at PC=" + hex2(in.pc) + ": " + (in.fetchBad ? std::string("instruction fetch") : disasm(in.raw)) + "]";
    log.push_back(std::string(stageName(idx)) + "   EXCEPTION " + exName(e.kind) + ": " + pendMsg);
}

// ---------------------------------------------------------------- one pipeline stage
bool CPU::stageWork(int i) {
    Inst& in = st[i];
    if (i == 0) return true;                    // fetch was done when the instruction entered IF
    bool ok = true;
    if (i == iDec) decodeInst(in);
    if (i == iRead) ok = readOperands(in, i);
    if (ok && i == iEX) executeInst(in);
    if (ok && i == iMEM) memoryInst(in);
    return ok;
}

// ---------------------------------------------------------------- clock
bool CPU::step() {
    if (halted || faulted) return false;
    stats.cycles++;
    log.clear();
    events.clear();
    lastFetch = -1;
    if (!faultPending && fetchOn && !st[0].valid) fetchInst();
    captureOccupancy();
    snapshotChart();
    doWB();
    for (int i = N - 2; i >= 0; i--) {
        if (!st[i].valid || st[i + 1].valid) continue;
        bool moved = false;
        try {
            moved = stageWork(i);
        } catch (SimException& e) {
            handleFault(i, e, st[i]);
            continue;
        }
        if (moved && st[i].valid) { st[i + 1] = st[i]; st[i].valid = false; }
    }
    if (faultPending && pipeEmpty()) {
        faulted = true;
        faultKind = pendKind;
        faultMsg = pendMsg;
        faultPC = pendPC;
    }
    return !(halted || faulted);
}

void CPU::run(uint64_t maxCycles, bool useBreakpoints) {
    uint64_t start = stats.cycles;
    hitBreak = false;
    while (step()) {
        if (stats.cycles - start >= maxCycles) {
            faulted = true;
            faultKind = ExKind::CycleLimit;
            faultMsg = "cycle limit " + std::to_string(maxCycles) + " reached (infinite loop?)";
            faultPC = pc;
            break;
        }
        if (useBreakpoints && lastFetch >= 0 && breakpoints.count(lastFetch)) { hitBreak = true; break; }
    }
}

bool CPU::stepInstruction() {
    uint64_t r = stats.retired;
    while (step()) if (stats.retired > r) return true;
    return stats.retired > r;
}

// ---------------------------------------------------------------- views
std::string CPU::pipelineText() const {
    std::ostringstream o;
    o << " after cycle " << stats.cycles << " (" << N << "-stage):\n";
    for (int i = 0; i < N; i++) {
        char b[160];
        std::string t = st[i].valid ? instText(st[i]) + (st[i].stalled ? "  (stalled)" : "") : "-- bubble --";
        std::snprintf(b, sizeof b, "   %-3s | %s\n", stageName(i), t.c_str());
        o << b;
    }
    return o.str();
}

std::string CPU::chartText(int maxCols) const {
    std::ostringstream o;
    char b[128];
    uint64_t last = stats.cycles < (uint64_t)maxCols ? stats.cycles : (uint64_t)maxCols;
    o << " pipeline chart (" << N << "-stage), cycles 1.." << last << "   '*' = stalled, 'X' = flushed\n";
    std::snprintf(b, sizeof b, "  %-24s", "instruction");
    o << b;
    for (uint64_t c = 1; c <= last; c++) { std::snprintf(b, sizeof b, "%-4d", (int)c); o << b; }
    o << "\n";
    int shown = 0;
    for (auto& kv : chart) {
        if (shown++ > 60) { o << "  ... (truncated)\n"; break; }
        auto ri = rows.find(kv.first);
        std::string label = "[" + std::to_string(ri->second.pc) + "] " + ri->second.text;
        std::snprintf(b, sizeof b, "  %-24s", label.substr(0, 23).c_str());
        o << b;
        uint64_t lastCycle = kv.second.empty() ? 0 : kv.second.rbegin()->first;
        for (uint64_t c = 1; c <= last; c++) {
            auto it = kv.second.find(c);
            std::string cell = it == kv.second.end() ? "" : it->second;
            if (cell.empty() && flushedSeq.count(kv.first) && c == lastCycle + 1) cell = "X";
            std::snprintf(b, sizeof b, "%-4s", cell.c_str());
            o << b;
        }
        o << "\n";
    }
    return o.str();
}
