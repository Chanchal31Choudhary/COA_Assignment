// =====================================================================
//  isa.h  --  RISC201 Instruction Set Architecture definition
//  OWNER : Jiya (Member A)
//  ROLE  : Single source of truth for opcodes, formats, encodings and
//          machine constants. Every other module includes this file.
// =====================================================================
#pragma once
#include <cstdint>
#include <string>

namespace isa {

// ---- machine constants -------------------------------------------------
constexpr int      MEM_WORDS   = 256;   // word-addressed memory, 16-bit words
constexpr int      NUM_REGS    = 8;     // R0..R7
constexpr int      SP_REG      = 7;     // R7 doubles as the stack pointer (SP)
constexpr uint16_t STACK_TOP   = 256;   // SP value when stack is empty
constexpr uint16_t STACK_LIMIT = 192;   // lowest legal stack address (guard)
                                        // stack segment = [192 .. 255], 64 words

// ---- opcodes (5 bits, instruction bits [15:11]) ---------------------------
enum Opcode : int {
    NOP = 0, HALT, ADD, SUB, AND, OR, XOR, NOT, SHL, SHR, SAR, MUL, MOV,
    LDI, LOAD, STORE, LDR, STR, JMP, BZ, BNZ, BNEG, PUSH, POP, CALL, RET, ADDI, DIV, FDIV,
    OPCODE_COUNT
};

// ---- operand formats ------------------------------------------------------
//  Encoding (16 bit):   [15:11] op | [10:8] rd | [7:5] rs1 | [4:2] rs2 | [1:0] 0
//                       [15:11] op | [10:8] rd | [7:0] imm8            (I-type)
enum Format {
    F_NONE,   // NOP, HALT, RET
    F_R3,     // rd, rs1, rs2         ADD SUB AND OR XOR SHL SHR SAR MUL
    F_R2,     // rd, rs1              NOT MOV
    F_MEM,    // rd, [rs1]            LDR STR
    F_R1,     // rd                   PUSH POP
    F_RIMM,   // rd, simm8            LDI ADDI   (sign-extended -128..127)
    F_RADDR,  // rd, addr8            LOAD STORE (0..255)
    F_RBR,    // rd, target           BZ BNZ BNEG
    F_TGT     // target               JMP CALL
};

struct OpInfo { const char* name; Format fmt; };

inline const OpInfo OP_TABLE[OPCODE_COUNT] = {
    {"NOP",F_NONE},  {"HALT",F_NONE}, {"ADD",F_R3},   {"SUB",F_R3},
    {"AND",F_R3},    {"OR",F_R3},     {"XOR",F_R3},   {"NOT",F_R2},
    {"SHL",F_R3},    {"SHR",F_R3},    {"SAR",F_R3},   {"MUL",F_R3},
    {"MOV",F_R2},    {"LDI",F_RIMM},  {"LOAD",F_RADDR},{"STORE",F_RADDR},
    {"LDR",F_MEM},   {"STR",F_MEM},   {"JMP",F_TGT},  {"BZ",F_RBR},
    {"BNZ",F_RBR},   {"BNEG",F_RBR},  {"PUSH",F_R1},  {"POP",F_R1},
    {"CALL",F_TGT},  {"RET",F_NONE},  {"ADDI",F_RIMM},
    {"DIV",F_R3},    {"FDIV",F_R3}
};

// ---- encode / decode helpers ---------------------------------------------
inline uint16_t encR(int op, int rd, int rs1, int rs2) {
    return (uint16_t)((op << 11) | (rd << 8) | (rs1 << 5) | (rs2 << 2));
}
inline uint16_t encI(int op, int rd, int imm8) {
    return (uint16_t)((op << 11) | (rd << 8) | (imm8 & 0xFF));
}

struct Fields { int op, rd, rs1, rs2, imm8; };

inline Fields decodeFields(uint16_t w) {
    Fields f;
    f.op   = (w >> 11) & 0x1F;
    f.rd   = (w >> 8) & 7;
    f.rs1  = (w >> 5) & 7;
    f.rs2  = (w >> 2) & 7;
    f.imm8 = w & 0xFF;
    return f;
}

inline int16_t sext8(int v) { return (int16_t)(int8_t)(v & 0xFF); }

inline bool isBranchOp(int op) { return op == JMP || op == BZ || op == BNZ || op == BNEG || op == CALL; }

}  // namespace isa
