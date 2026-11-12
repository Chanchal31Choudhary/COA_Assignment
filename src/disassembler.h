// =====================================================================
//  disassembler.h  --  RISC201 disassembler
//  OWNER : Rani (Member C)
//  ROLE  : Turns 16-bit words (from a .hex / .bin file or the loaded
//          program) back into assembly text. Two passes: pass 1 finds all
//          branch targets, pass 2 prints instructions and generates the
//          labels (L<addr>), so the output can be re-assembled.
// =====================================================================
#pragma once
#include <map>
#include <string>
#include "assembler.h"

// one instruction; 'labels' (optional) maps address -> label name for branch targets
std::string disasm(uint16_t word, const std::map<int, std::string>* labels = nullptr);

// whole program.  asmForm=true -> clean re-assemblable source
//                 asmForm=false -> listing with address and hex word columns
std::string disassembleProgram(const Program& p, bool asmForm);
