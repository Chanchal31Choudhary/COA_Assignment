// disassembler.cpp -- OWNER: Rani (Member C)
#include "disassembler.h"
#include <cstdio>
#include <set>
#include <sstream>

using namespace isa;

static std::string regName(int r) { return r == SP_REG ? "SP" : "R" + std::to_string(r); }

static std::string target(int addr, const std::map<int, std::string>* labels) {
    if (labels) {
        auto it = labels->find(addr);
        if (it != labels->end()) return it->second;
    }
    return std::to_string(addr);
}

std::string disasm(uint16_t w, const std::map<int, std::string>* labels) {
    Fields f = decodeFields(w);
    char b[32];
    if (f.op >= OPCODE_COUNT) {
        std::snprintf(b, sizeof b, ".word 0x%04X", w);
        return b;
    }
    const OpInfo& oi = OP_TABLE[f.op];
    std::string s = oi.name;
    switch (oi.fmt) {
        case F_NONE:  break;
        case F_R3:    s += " " + regName(f.rd) + ", " + regName(f.rs1) + ", " + regName(f.rs2); break;
        case F_R2:    s += " " + regName(f.rd) + ", " + regName(f.rs1); break;
        case F_MEM:   s += " " + regName(f.rd) + ", [" + regName(f.rs1) + "]"; break;
        case F_R1:    s += " " + regName(f.rd); break;
        case F_RIMM:  s += " " + regName(f.rd) + ", " + std::to_string((int)sext8(f.imm8)); break;
        case F_RADDR: s += " " + regName(f.rd) + ", " + std::to_string(f.imm8); break;
        case F_RBR:   s += " " + regName(f.rd) + ", " + target(f.imm8, labels); break;
        case F_TGT:   s += " " + target(f.imm8, labels); break;
    }
    return s;
}

std::string disassembleProgram(const Program& p, bool asmForm) {
    // pass 1: collect branch targets
    std::map<int, std::string> labels;
    for (int a = 0; a <= p.lastAddr; a++) {
        if (!p.isCode[a]) continue;
        Fields f = decodeFields(p.mem[a]);
        if (f.op < OPCODE_COUNT) {
            Format fm = OP_TABLE[f.op].fmt;
            if (fm == F_RBR || fm == F_TGT) labels[f.imm8] = "L" + std::to_string(f.imm8);
        }
    }
    // pass 2: print. Re-assemblable output preserves sparse .org regions;
    // emitting zero-valued .word entries for holes would change the image.
    std::ostringstream o;
    char b[64];
    if (asmForm) {
        int cursor = 0;
        for (int a = 0; a <= p.lastAddr; a++) {
            auto it = labels.find(a);
            if (!p.used[a] && it == labels.end()) continue;
            if (a != cursor) { o << ".org " << a << "\n"; cursor = a; }
            if (it != labels.end()) o << it->second << ":\n";
            if (!p.used[a]) continue;
            if (p.isCode[a]) o << "    " << disasm(p.mem[a], &labels) << "\n";
            else {
                std::snprintf(b, sizeof b, ".word 0x%04X", p.mem[a]);
                o << "    " << b << "\n";
            }
            cursor = a + 1;
        }
    } else {
        for (int a = 0; a <= p.lastAddr; a++) {
            auto it = labels.find(a);
            if (it != labels.end()) o << it->second << ":\n";
            std::string text;
            if (p.isCode[a]) text = disasm(p.mem[a], &labels);
            else { std::snprintf(b, sizeof b, ".word 0x%04X", p.mem[a]); text = b; }
            std::snprintf(b, sizeof b, "  %03d:  %04X   ", a, p.mem[a]);
            o << b << text << "\n";
        }
    }
    return o.str();
}
