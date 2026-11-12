// =====================================================================
//  microcode.h  --  Micro-programmed control unit (control memory)
//  OWNER : Kavya (Member D)
//  ROLE  : The CPU has NO hard-wired decoder. When an instruction is
//          decoded, the opcode is looked up in a DISPATCH ROM giving the
//          start micro-address; the micro-sequencer then steps the
//          micro-PC through the CONTROL STORE, OR-ing together the control
//          signals of every micro-instruction until an END bit is seen.
//
//  Two control-store organisations (same micro-programs, different encoding)
//    HORIZONTAL : each micro-instruction is 30 bits wide, one bit per control
//                 signal (+ALU field, cond field, end bit).  No decoder needed.
//    VERTICAL   : each micro-instruction is 12 bits: a 5-bit micro-operation
//                 code that is expanded by a small DECODE ROM into signals.
//                 Narrower store, but one extra ROM lookup per step.
// =====================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "alu.h"
#include "exceptions.h"

// ---- control signals (bits of the control word) ----------------------------
enum Sig : uint32_t {
    SG_RS1   = 1u << 0,   // operand A <- R[rs1]
    SG_RDA   = 1u << 1,   // operand A <- R[rd]
    SG_RS2   = 1u << 2,   // operand B <- R[rs2]
    SG_IMM   = 1u << 3,   // operand B <- sign-extended imm8
    SG_RDV   = 1u << 4,   // value    <- R[rd]   (store data / branch test)
    SG_SPR   = 1u << 5,   // SP is read as a source
    SG_ALU   = 1u << 6,   // result   <- ALU(A,B)
    SG_RDW   = 1u << 7,   // write R[rd] (ALU result or loaded value)
    SG_MEMR  = 1u << 8,   // memory read
    SG_MEMW  = 1u << 9,   // memory write
    SG_ADRI  = 1u << 10,  // address <- imm8
    SG_ADRR  = 1u << 11,  // address <- R[rs1]
    SG_ADRSP = 1u << 12,  // address <- SP
    SG_ADRSPM= 1u << 13,  // address <- SP-1
    SG_SPUP  = 1u << 14,  // new SP  <- SP+1
    SG_SPDN  = 1u << 15,  // new SP  <- SP-1
    SG_SPW   = 1u << 16,  // write SP
    SG_STK   = 1u << 17,  // memory access is a stack access (guarded)
    SG_LINK  = 1u << 18,  // value <- PC+1 (return address)
    SG_BR    = 1u << 19,  // conditional branch to imm8
    SG_JMP   = 1u << 20,  // unconditional jump to imm8
    SG_RETJ  = 1u << 21,  // PC <- loaded value (RET)
    SG_HALT  = 1u << 22   // stop the machine
};
constexpr int SIG_COUNT = 23;
std::string sigNames(uint32_t mask);

enum class Cond : uint8_t { NONE = 0, Z, NZ, NEG };
enum class CtlStyle { Horizontal, Vertical };

// ---- micro-operations (the vertical encoding alphabet) ------------------------
enum UOp : uint8_t {
    U_NOP, U_RD_RS12, U_RD_RS1, U_RD_RDA_IMM, U_RD_IMM, U_RD_RDV, U_SPR, U_ALU,
    U_WB_RD, U_ADR_IMM, U_ADR_REG, U_ADR_SP, U_ADR_SPM, U_SP_UP, U_SP_DN,
    U_MEM_RD, U_MEM_WR, U_LINK, U_BR, U_JMP, U_RETJ, U_HALT, U_COUNT
};

struct ControlWord {
    uint32_t sig = 0;
    AluOp alu = AluOp::NONE;
    Cond cond = Cond::NONE;
    bool valid = false;
    ExKind kind = ExKind::IllegalOpcode;   // set when valid == false
    std::string err;
    int steps = 0;                          // micro-instructions executed
};

class ControlUnit {
public:
    explicit ControlUnit(CtlStyle s = CtlStyle::Horizontal);
    void setStyle(CtlStyle s) { style_ = s; }
    CtlStyle style() const { return style_; }

    // Run the micro-sequencer for one opcode. 'trace' (optional) receives one
    // text line per micro-instruction showing the micro-PC state.
    ControlWord fetch(int opcode, std::vector<std::string>* trace);

    int storeWords() const { return (int)hStore_.size(); }
    int wordBits() const { return style_ == CtlStyle::Horizontal ? 30 : 12; }
    int decodeRomBits() const { return style_ == CtlStyle::Horizontal ? 0 : (int)U_COUNT * SIG_COUNT; }
    int totalBits() const { return storeWords() * wordBits() + decodeRomBits() + 32 * 8; }  // +dispatch ROM
    std::string dump() const;

    uint64_t microCycles = 0;   // micro-instructions executed so far
    uint64_t dispatches = 0;    // opcodes decoded so far

private:
    struct MicroDef { UOp uop; AluOp alu; Cond cond; };
    void build();
    static const char* uopName(int u);
    CtlStyle style_;
    std::vector<uint32_t> hStore_;     // horizontal control store
    std::vector<uint16_t> vStore_;     // vertical control store
    int dispatch_[32];                 // opcode -> start micro-address (-1 = none)
    uint32_t decodeRom_[U_COUNT];      // micro-op code -> control signals (vertical)
};
