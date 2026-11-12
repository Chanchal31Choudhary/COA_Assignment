// memory.cpp -- OWNER: Chanchal (Member B)
#include "memory.h"
#include <cstdio>

static std::string hx(int v) { char b[16]; std::snprintf(b, sizeof b, "0x%02X", v & 0xFFFF); return b; }

void Memory::clear() {
    for (int i = 0; i < isa::MEM_WORDS; i++) { w_[i] = 0; code_[i] = false; }
    stackLow_ = isa::STACK_TOP;
    reads = writes = 0;
}

uint16_t Memory::read(uint16_t addr) const {
    if (addr >= isa::MEM_WORDS)
        throw SimException(ExKind::BadAddress, "read from invalid address " + hx(addr));
    const_cast<Memory*>(this)->reads++;
    return w_[addr];
}

void Memory::write(uint16_t addr, uint16_t v, bool stackOp, uint16_t sp) {
    if (addr >= isa::MEM_WORDS)
        throw SimException(ExKind::BadAddress, "write to invalid address " + hx(addr));
    if (stackOp) {
        // PUSH / CALL: must stay inside the stack segment
        if (addr < isa::STACK_LIMIT)
            throw SimException(ExKind::StackOverflow, "stack push to " + hx(addr) + " is below STACK_LIMIT " + hx(isa::STACK_LIMIT));
    } else {
        if (code_[addr])
            throw SimException(ExKind::CodeWrite, "store into code segment at " + hx(addr));
        if (addr >= isa::STACK_LIMIT && addr < sp)
            throw SimException(ExKind::StackCorruption,
                "store to " + hx(addr) + " is inside the stack segment but below SP=" + hx(sp) + " (unallocated stack)");
    }
    w_[addr] = v;
    writes++;
}

void RegisterFile::reset() {
    for (int i = 0; i < isa::NUM_REGS; i++) r_[i] = 0;
    r_[isa::SP_REG] = isa::STACK_TOP;
    reads = writes = 0;
}

void RegisterFile::write(int r, uint16_t v) {
    r &= 7;
    if (r == isa::SP_REG) {
        if (v < isa::STACK_LIMIT)
            throw SimException(ExKind::StackOverflow, "SP set to " + hx(v) + " below STACK_LIMIT " + hx(isa::STACK_LIMIT));
        if (v > isa::STACK_TOP)
            throw SimException(ExKind::StackUnderflow, "SP set to " + hx(v) + " above STACK_TOP " + hx(isa::STACK_TOP));
    }
    r_[r] = v;
    writes++;
}
