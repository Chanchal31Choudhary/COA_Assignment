// =====================================================================
//  memory.h  --  Main memory + register file with protection
//  OWNER : Chanchal (Member B)
//  ROLE  : 256 x 16-bit word memory, 8 x 16-bit registers, and the
//          STACK SAFETY layer (bounds verification + run-time exceptions).
//
//  Memory map
//      0   ..  (code and .word data placed by the assembler)
//      192 .. 255  stack segment (full-descending, guarded)
//
//  Protection rules (all raise SimException)
//      * any access outside 0..255                  -> BadAddress
//      * normal store into code words               -> CodeWrite
//      * normal store into stack area BELOW SP      -> StackCorruption
//      * PUSH/CALL when SP == STACK_LIMIT           -> StackOverflow
//      * POP/RET  when SP == STACK_TOP              -> StackUnderflow
//      * SP written outside [STACK_LIMIT, STACK_TOP]-> StackOverflow/Underflow
// =====================================================================
#pragma once
#include <cstdint>
#include "isa.h"
#include "exceptions.h"

class Memory {
public:
    Memory() { clear(); }
    void clear();
    uint16_t read(uint16_t addr) const;                       // checked read
    void write(uint16_t addr, uint16_t v, bool stackOp, uint16_t sp);   // checked write
    uint16_t peek(int addr) const { return w_[addr & 0xFF]; } // unchecked (debugger)
    void poke(int addr, uint16_t v) { w_[addr & 0xFF] = v; }
    bool isCode(int addr) const { return addr >= 0 && addr < isa::MEM_WORDS && code_[addr]; }
    void setCode(int addr, bool c) { code_[addr] = c; }
    void notifySP(uint16_t sp) { if (sp < stackLow_) stackLow_ = sp; }
    uint16_t stackLow() const { return stackLow_; }          // deepest SP seen (peak usage)
    uint64_t reads = 0, writes = 0;
private:
    uint16_t w_[isa::MEM_WORDS];
    bool code_[isa::MEM_WORDS];
    uint16_t stackLow_ = isa::STACK_TOP;
};

class RegisterFile {
public:
    RegisterFile() { reset(); }
    void reset();
    uint16_t read(int r) const { return r_[r & 7]; }
    void write(int r, uint16_t v);            // SP writes are range-checked
    uint16_t sp() const { return r_[isa::SP_REG]; }
    uint64_t reads = 0, writes = 0;
private:
    uint16_t r_[isa::NUM_REGS];
};
