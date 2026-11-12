// =====================================================================
//  assembler.h  --  Two-pass RISC201 assembler + object-file I/O
//  OWNER : Jiya (Member A)
//  ROLE  : Pass 1 builds the symbol table (labels, .equ) and assigns
//          addresses.  Pass 2 encodes every instruction to a 16-bit word.
//          Produces binary / hexadecimal output and a listing file.
//
//  Syntax
//      label:  MNEMONIC operands   ; comment
//      Directives: .org expr | .word e1,e2,.. | .space n | .equ NAME, expr
//      Numbers: 12  -5  0x1F  0b1010     Expressions: label+2, N-1
//      Registers: R0..R7, SP (= R7)
// =====================================================================
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "isa.h"

struct ListLine {
    int addr = 0;
    uint16_t word = 0;
    bool isCode = true;
    std::string src;    // source text of the line
    int line = 0;       // original line number
    std::string from;   // macro that generated this line (if any)
};

struct Program {
    uint16_t mem[isa::MEM_WORDS];
    bool isCode[isa::MEM_WORDS];
    bool used[isa::MEM_WORDS];
    int lastAddr = -1;                       // highest used address
    std::map<std::string, int> symbols;
    std::vector<ListLine> listing;
    std::vector<std::string> errors;
    Program();
    bool ok() const { return errors.empty(); }
};

Program assemble(const std::string& source);                 // preprocess + pass1 + pass2
std::string toHex(const Program& p);                          // one word per line, ";D" marks data
std::string toBin(const Program& p);                          // 16-char binary strings
std::string toListing(const Program& p);                      // address | word | source
Program parseImage(const std::string& text, std::vector<std::string>& errors);  // load .hex/.bin text
bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& data);
