// microcode.cpp -- OWNER: Kavya (Member D)
#include "microcode.h"
#include "isa.h"
#include <cstdio>
#include <sstream>

using namespace isa;

std::string sigNames(uint32_t mask) {
    static const char* n[SIG_COUNT] = {"RS1","RDA","RS2","IMM","RDV","SPR","ALU","RDW","MEMR","MEMW","ADRI",
        "ADRR","ADRSP","ADRSPM","SPUP","SPDN","SPW","STK","LINK","BR","JMP","RETJ","HALT"};
    std::string s;
    for (int i = 0; i < SIG_COUNT; i++)
        if (mask & (1u << i)) { if (!s.empty()) s += "|"; s += n[i]; }
    return s.empty() ? "-" : s;
}

const char* ControlUnit::uopName(int u) {
    static const char* n[U_COUNT] = {"NOP","RD_RS12","RD_RS1","RD_RDA_IMM","RD_IMM","RD_RDV","SPR","ALU",
        "WB_RD","ADR_IMM","ADR_REG","ADR_SP","ADR_SPM","SP_UP","SP_DN","MEM_RD","MEM_WR","LINK","BR","JMP","RETJ","HALT"};
    return n[u];
}

ControlUnit::ControlUnit(CtlStyle s) : style_(s) { build(); }

void ControlUnit::build() {
    // ---- vertical decode ROM: micro-op -> control signals -----------------
    for (int i = 0; i < U_COUNT; i++) decodeRom_[i] = 0;
    decodeRom_[U_RD_RS12]   = SG_RS1 | SG_RS2;
    decodeRom_[U_RD_RS1]    = SG_RS1;
    decodeRom_[U_RD_RDA_IMM]= SG_RDA | SG_IMM;
    decodeRom_[U_RD_IMM]    = SG_IMM;
    decodeRom_[U_RD_RDV]    = SG_RDV;
    decodeRom_[U_SPR]       = SG_SPR;
    decodeRom_[U_ALU]       = SG_ALU;
    decodeRom_[U_WB_RD]     = SG_RDW;
    decodeRom_[U_ADR_IMM]   = SG_ADRI;
    decodeRom_[U_ADR_REG]   = SG_ADRR;
    decodeRom_[U_ADR_SP]    = SG_ADRSP | SG_STK;
    decodeRom_[U_ADR_SPM]   = SG_ADRSPM | SG_STK;
    decodeRom_[U_SP_UP]     = SG_SPUP | SG_SPW;
    decodeRom_[U_SP_DN]     = SG_SPDN | SG_SPW;
    decodeRom_[U_MEM_RD]    = SG_MEMR;
    decodeRom_[U_MEM_WR]    = SG_MEMW;
    decodeRom_[U_LINK]      = SG_LINK;
    decodeRom_[U_BR]        = SG_BR;
    decodeRom_[U_JMP]       = SG_JMP;
    decodeRom_[U_RETJ]      = SG_RETJ;
    decodeRom_[U_HALT]      = SG_HALT;

    // ---- micro-programs (one routine per opcode) ------------------------------
    std::vector<std::vector<MicroDef>> prog(32);
    auto M = [](UOp u, AluOp a = AluOp::NONE, Cond c = Cond::NONE) { return MicroDef{u, a, c}; };
    auto alu3 = [&](Opcode op, AluOp a) { prog[op] = {M(U_RD_RS12), M(U_ALU, a), M(U_WB_RD)}; };

    prog[NOP]  = {M(U_NOP)};
    prog[HALT] = {M(U_HALT)};
    alu3(ADD, AluOp::ADD); alu3(SUB, AluOp::SUB); alu3(AND, AluOp::AND); alu3(OR, AluOp::OR);
    alu3(XOR, AluOp::XOR); alu3(SHL, AluOp::SHL); alu3(SHR, AluOp::SHR); alu3(SAR, AluOp::SAR);
    alu3(MUL, AluOp::MUL);
    alu3(DIV, AluOp::DIV);
    alu3(FDIV, AluOp::FDIV);
    prog[NOT]   = {M(U_RD_RS1), M(U_ALU, AluOp::NOT),   M(U_WB_RD)};
    prog[MOV]   = {M(U_RD_RS1), M(U_ALU, AluOp::PASSA), M(U_WB_RD)};
    prog[LDI]   = {M(U_RD_IMM), M(U_ALU, AluOp::PASSB), M(U_WB_RD)};
    prog[ADDI]  = {M(U_RD_RDA_IMM), M(U_ALU, AluOp::ADD), M(U_WB_RD)};
    prog[LOAD]  = {M(U_ADR_IMM), M(U_MEM_RD), M(U_WB_RD)};
    prog[STORE] = {M(U_RD_RDV), M(U_SPR), M(U_ADR_IMM), M(U_MEM_WR)};
    prog[LDR]   = {M(U_ADR_REG), M(U_MEM_RD), M(U_WB_RD)};
    prog[STR]   = {M(U_RD_RDV), M(U_SPR), M(U_ADR_REG), M(U_MEM_WR)};
    prog[JMP]   = {M(U_JMP)};
    prog[BZ]    = {M(U_RD_RDV), M(U_BR, AluOp::NONE, Cond::Z)};
    prog[BNZ]   = {M(U_RD_RDV), M(U_BR, AluOp::NONE, Cond::NZ)};
    prog[BNEG]  = {M(U_RD_RDV), M(U_BR, AluOp::NONE, Cond::NEG)};
    prog[PUSH]  = {M(U_RD_RDV), M(U_ADR_SPM), M(U_SP_DN), M(U_MEM_WR)};
    prog[POP]   = {M(U_ADR_SP), M(U_SP_UP), M(U_MEM_RD), M(U_WB_RD)};
    prog[CALL]  = {M(U_LINK), M(U_ADR_SPM), M(U_SP_DN), M(U_MEM_WR), M(U_JMP)};
    prog[RET]   = {M(U_ADR_SP), M(U_SP_UP), M(U_MEM_RD), M(U_RETJ)};

    // ---- lay the routines out in the control store -----------------------------
    hStore_.clear();
    vStore_.clear();
    for (int i = 0; i < 32; i++) dispatch_[i] = -1;
    for (int op = 0; op < 32; op++) {
        if (prog[op].empty()) continue;
        dispatch_[op] = (int)hStore_.size();
        for (size_t k = 0; k < prog[op].size(); k++) {
            const MicroDef& d = prog[op][k];
            uint32_t end = (k + 1 == prog[op].size()) ? 1u : 0u;
            uint32_t h = decodeRom_[d.uop] | ((uint32_t)d.alu << 23) | ((uint32_t)d.cond << 27) | (end << 29);
            uint16_t v = (uint16_t)((uint32_t)d.uop | ((uint32_t)d.alu << 5) | ((uint32_t)d.cond << 9) | (end << 11));
            hStore_.push_back(h);
            vStore_.push_back(v);
        }
    }
}

ControlWord ControlUnit::fetch(int op, std::vector<std::string>* trace) {
    ControlWord cw;
    dispatches++;
    char b[200];
    if (op < 0 || op >= 32 || dispatch_[op] < 0) {
        std::snprintf(b, sizeof b, "no micro-routine in dispatch ROM for opcode %d (illegal instruction)", op);
        cw.err = b; cw.kind = ExKind::IllegalOpcode;
        if (trace) trace->push_back(std::string("dispatch[") + std::to_string(op) + "] = NONE  -> raise IllegalOpcode");
        return cw;
    }
    int upc = dispatch_[op];
    if (trace) {
        std::snprintf(b, sizeof b, "dispatch[%d] -> uPC=%03d", op, upc);
        trace->push_back(b);
    }
    for (int step = 0; step < 8; step++) {
        if (upc < 0 || upc >= storeWords()) {
            cw.err = "micro-PC " + std::to_string(upc) + " outside control store"; cw.kind = ExKind::MicrocodeFault;
            return cw;
        }
        uint32_t mask; int alu, cond, end, uop = -1;
        if (style_ == CtlStyle::Horizontal) {
            uint32_t w = hStore_[upc];
            mask = w & 0x7FFFFF; alu = (w >> 23) & 0xF; cond = (w >> 27) & 3; end = (w >> 29) & 1;
        } else {
            uint16_t w = vStore_[upc];
            uop = w & 0x1F; alu = (w >> 5) & 0xF; cond = (w >> 9) & 3; end = (w >> 11) & 1;
            mask = decodeRom_[uop];                        // extra decode-ROM lookup
        }
        cw.sig |= mask;
        if (alu) cw.alu = (AluOp)alu;
        if (cond) cw.cond = (Cond)cond;
        cw.steps++;
        microCycles++;
        if (trace) {
            if (style_ == CtlStyle::Horizontal)
                std::snprintf(b, sizeof b, "uPC=%03d  H[%08X]  sig=%s%s%s  %s", upc, hStore_[upc], sigNames(mask).c_str(),
                              alu ? " alu=" : "", alu ? aluOpName((AluOp)alu) : "", end ? "  END" : "  next");
            else
                std::snprintf(b, sizeof b, "uPC=%03d  V[%03X]  uop=%s -> sig=%s%s%s  %s", upc, vStore_[upc], uopName(uop),
                              sigNames(mask).c_str(), alu ? " alu=" : "", alu ? aluOpName((AluOp)alu) : "", end ? "  END" : "  next");
            trace->push_back(b);
        }
        if (end) { cw.valid = true; return cw; }
        upc++;
    }
    cw.err = "micro-PC ran away (no END bit within 8 micro-instructions)";
    cw.kind = ExKind::MicrocodeFault;
    return cw;
}

std::string ControlUnit::dump() const {
    std::ostringstream o;
    char b[160];
    bool H = style_ == CtlStyle::Horizontal;
    o << "CONTROL STORE  (" << (H ? "HORIZONTAL" : "VERTICAL") << ")\n";
    std::snprintf(b, sizeof b, "  words=%d  bits/word=%d  store=%d bits  decode-ROM=%d bits  dispatch-ROM=%d bits  TOTAL=%d bits\n",
                  storeWords(), wordBits(), storeWords() * wordBits(), decodeRomBits(), 32 * 8, totalBits());
    o << b;
    for (int op = 0; op < OPCODE_COUNT; op++) {
        if (dispatch_[op] < 0) continue;
        o << "  " << OP_TABLE[op].name << " @uPC=" << dispatch_[op] << "\n";
        for (int u = dispatch_[op];; u++) {
            uint32_t mask; int alu, end, uop = -1;
            if (H) { uint32_t w = hStore_[u]; mask = w & 0x7FFFFF; alu = (w >> 23) & 0xF; end = (w >> 29) & 1; }
            else { uint16_t w = vStore_[u]; uop = w & 0x1F; alu = (w >> 5) & 0xF; end = (w >> 11) & 1; mask = decodeRom_[uop]; }
            if (H) std::snprintf(b, sizeof b, "     %03d  %08X  %s%s%s", u, hStore_[u], sigNames(mask).c_str(), alu ? " alu=" : "", alu ? aluOpName((AluOp)alu) : "");
            else   std::snprintf(b, sizeof b, "     %03d  %03X  %-11s %s%s%s", u, vStore_[u], uopName(uop), sigNames(mask).c_str(), alu ? " alu=" : "", alu ? aluOpName((AluOp)alu) : "");
            o << b << (end ? "  END\n" : "\n");
            if (end) break;
        }
    }
    return o.str();
}
