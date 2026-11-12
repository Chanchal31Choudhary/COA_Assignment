// =====================================================================
// Static pipeline hazard analyzer
//
// Algorithm: walk only assembled code words (never .word/.space data),
// decode each instruction, and ask the same microcode ControlUnit used by
// CPU::decodeInst() which registers it reads/writes. Pair each read with
// the nearest earlier writer. Stall estimates follow CPU::readOperands():
// the read stage checks older in-flight destinations, forwards only when
// enabled and ready, and otherwise holds the consumer. Control penalties
// use CPU::redirect(): taken EX redirects cost the EX stage index; RET
// redirects in MEM cost the MEM stage index. Estimates are per dependency;
// overlapping stalls are not necessarily additive in a real execution.
// =====================================================================
#include "hazards.h"
#include "microcode.h"
#include "pipeline.h"
#include <algorithm>
#include <iomanip>
#include <map>
#include <limits>
#include <sstream>
#include <vector>

using namespace isa;

namespace {
struct Insn {
    int pc = 0, line = 0, op = 0;
    Fields f{};
    uint32_t sig = 0;
    unsigned reads = 0, writes = 0;
    bool load = false, control = false, conditional = false, ret = false;
    std::string text;
};

static std::string regName(int r) { return r == SP_REG ? "R7/SP" : "R" + std::to_string(r); }
static std::string instructionName(const Fields& f) {
    return f.op >= 0 && f.op < OPCODE_COUNT ? OP_TABLE[f.op].name : "<illegal>";
}
static int findLine(const Program& p, int addr) {
    for (const auto& l : p.listing) if (l.addr == addr) return l.line;
    return 0;
}
static std::string findSource(const Program& p, int addr, const Fields& f) {
    for (const auto& l : p.listing) if (l.addr == addr && !l.src.empty()) return l.src;
    return instructionName(f);
}

static std::vector<Insn> collectInstructions(const Program& p) {
    std::vector<Insn> out;
    ControlUnit cu(CtlStyle::Horizontal);
    for (int pc = 0; pc <= p.lastAddr && pc < MEM_WORDS; ++pc) {
        if (!p.used[pc] || !p.isCode[pc]) continue; // skip embedded data words
        Insn in;
        in.pc = pc;
        in.line = findLine(p, pc);
        in.f = decodeFields(p.mem[pc]);
        in.op = in.f.op;
        in.text = findSource(p, pc, in.f);
        if (in.op < 0 || in.op >= OPCODE_COUNT) { out.push_back(in); continue; }
        ControlWord cw = cu.fetch(in.op, nullptr);
        if (!cw.valid) { out.push_back(in); continue; }
        in.sig = cw.sig;
        // Keep this mapping identical to CPU::decodeInst().
        if (in.sig & (SG_RS1 | SG_ADRR)) in.reads |= 1u << in.f.rs1;
        if (in.sig & SG_RS2) in.reads |= 1u << in.f.rs2;
        if (in.sig & (SG_RDA | SG_RDV)) in.reads |= 1u << in.f.rd;
        if (in.sig & (SG_SPR | SG_ADRSP | SG_ADRSPM | SG_SPUP | SG_SPDN)) in.reads |= 1u << SP_REG;
        if (in.sig & SG_RDW) in.writes |= 1u << in.f.rd;
        if (in.sig & SG_SPW) in.writes |= 1u << SP_REG;
        in.load = (in.sig & SG_MEMR) && (in.sig & SG_RDW);
        in.ret = (in.sig & SG_RETJ) != 0;
        in.conditional = (in.sig & SG_BR) != 0;
        in.control = (in.sig & (SG_JMP | SG_BR | SG_RETJ)) != 0;
        out.push_back(in);
    }
    return out;
}

// These pairwise values mirror the stage positions and ready timing in
// CPU::step(), CPU::readOperands(), executeInst(), and memoryInst().
static int pairStalls(int stages, bool forwarding, bool load, int distance) {
    if (stages == 4) {
        if (distance != 1) return 0; // older producer has reached WB/committed
        if (!forwarding) return 1;
        return 0; // EX and MEM are the same stage in this 4-stage design
    }
    if (distance == 1) {
        if (!forwarding) return 2;
        return load ? 1 : 0; // load value is not ready until MEM
    }
    if (distance == 2 && !forwarding) return 1;
    return 0;
}


} // namespace

std::string analyzeHazards(const Program& program, int stages, bool forwarding) {
    stages = stages == 6 ? 6 : 4;
    const auto ins = collectInstructions(program);
    std::ostringstream out;
    out << "\nHAZARD ANALYSIS — " << stages << "-stage pipeline, forwarding "
        << (forwarding ? "ON" : "OFF") << "\n";
    out << "Stages: " << (stages == 4 ? "IF | ID(read) | EX+MEM | WB" : "IF | ID | RF(read) | EX | MEM | WB") << "\n";
    out << "--------------------------------------------------------------------------------------------------------------\n";

    struct Hazard { std::string type; int producer, consumer, reg, distance, stalls; };
    std::vector<Hazard> hazards;
    std::map<int, int> lastWriter; // register -> instruction index
    int controlCount = 0, controlPenalty = 0;

    for (int i = 0; i < (int)ins.size(); ++i) {
        const Insn& c = ins[i];
        // Find the closest preceding writer for every source register.
        for (int r = 0; r < NUM_REGS; ++r) {
            if (!(c.reads & (1u << r))) continue;
            auto it = lastWriter.find(r);
            if (it == lastWriter.end()) continue;
            const Insn& p = ins[it->second];
            int distance = i - it->second;
            int stalls = pairStalls(stages, forwarding, p.load, distance);
            // LOAD-USE means this dependency actually delays the consumer.
            // A load dependency with no timing penalty remains an ordinary RAW.
            const char* type = (p.load && stalls > 0) ? "LOAD-USE" : "RAW";
            hazards.push_back({type, p.pc, c.pc, r, distance, stalls});
        }
        if (c.control) {
            ++controlCount;
            int penalty = c.ret ? (stages == 6 ? 4 : 2) : (stages == 6 ? 3 : 2);
            controlPenalty += penalty;
            out << (c.conditional ? "CONDITIONAL CONTROL" : "UNCONDITIONAL CONTROL")
                << "  line " << std::setw(3) << c.line << " PC " << std::setw(3) << c.pc
                << "  " << c.text << "  -> flush penalty " << penalty << " cycles"
                << (c.conditional ? " if taken" : "")
                << (c.ret ? " (RET resolves in MEM)" : " (resolves in EX)") << "\n";
        }
        for (int r = 0; r < NUM_REGS; ++r) if (c.writes & (1u << r)) lastWriter[r] = i;
    }

    // The CPU has one in-order read stage. If one consumer depends on
    // multiple older writers, its simultaneous dependencies do not create
    // separate stall cycles: use the maximum delay for that consumer.
    std::map<int, int> stallsByConsumer;
    for (const auto& h : hazards)
        stallsByConsumer[h.consumer] = std::max(stallsByConsumer[h.consumer], h.stalls);
    int dataStalls = 0;
    for (const auto& item : stallsByConsumer) dataStalls += item.second;

    for (const auto& h : hazards) {
        const Insn* prod = nullptr; const Insn* cons = nullptr;
        for (const auto& x : ins) { if (x.pc == h.producer) prod = &x; if (x.pc == h.consumer) cons = &x; }
        out << std::left << std::setw(9) << h.type << " line " << std::setw(3) << (prod ? prod->line : 0)
            << " PC " << std::setw(3) << h.producer << " [" << (prod ? prod->text : "?") << "] -> line "
            << std::setw(3) << (cons ? cons->line : 0) << " PC " << std::setw(3) << h.consumer
            << " [" << (cons ? cons->text : "?") << "]  reg=" << regName(h.reg)
            << "  distance=" << h.distance << "  estimated stalls=" << h.stalls << "\n";
    }
    if (hazards.empty() && controlCount == 0) out << "No hazards detected.\n";
    out << "--------------------------------------------------------------------------------------------------------------\n";
    out << "Total hazards: " << (hazards.size() + controlCount) << " (data: " << hazards.size() << ", control: " << controlCount << ")\n";
    out << "Estimated data stall cycles: " << dataStalls << "\n";
    out << "Potential control flush cycles (assuming every conditional branch is taken): " << controlPenalty << "\n";
    out << "Estimated total penalty (pairwise data + potential control): " << (dataStalls + controlPenalty) << " cycles\n";

    // Validate against the actual cycle-accurate pipeline on the path it executes.
    // This is deliberately a separate CPU instance; analysis never mutates caller state.
    CPU check;
    check.configure(stages, CtlStyle::Horizontal, forwarding);
    check.load(program);
    check.run(200000, false);
    out << "Simulator check status: ";
    if (check.halted) out << "HALTED successfully";
    else if (check.faulted && check.faultKind == ExKind::CycleLimit)
        out << "CYCLE LIMIT reached (" << check.faultMsg << ")";
    else if (check.faulted)
        out << "EXCEPTION " << exName(check.faultKind) << ": " << check.faultMsg;
    else out << "INCOMPLETE (execution stopped without HALT or exception)";
    out << "\n";
    if (check.halted || (check.faulted && check.faultKind != ExKind::CycleLimit)) {
        out << "Simulator statistics (" << (check.halted ? "completed execution" : "partial execution before exception")
            << "): data stalls=" << check.stats.dataStalls
            << ", control flush penalty=" << check.stats.controlBubbles
            << ", total=" << (check.stats.dataStalls + check.stats.controlBubbles) << " cycles\n";
    } else {
        out << "Simulator statistics: not presented as complete because execution did not finish.\n";
    }
    out << "Note: static analysis scans instructions in assembled order; it does not perform complete control-flow/path analysis. "
        << "Branches may skip instructions and loops may execute instructions repeatedly, so static estimates can differ from actual execution. "
        << "Conditional branch penalties apply only when taken. Dependencies of the same consumer share a read-stage stall; "
        << "the estimate uses the maximum pairwise delay for that consumer.\n";
    return out.str();
}

std::string analyzeHazards(const Program& program, int stages) {
    return analyzeHazards(program, stages, true) + analyzeHazards(program, stages, false);
}

std::string analyzeAllHazards(const Program& program) {
    std::ostringstream out;
    for (int stages : {4, 6}) for (bool fwd : {true, false})
        out << analyzeHazards(program, stages, fwd) << "\n";
    return out.str();
}
