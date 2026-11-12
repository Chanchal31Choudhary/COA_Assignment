// =====================================================================
//  preprocessor.h  --  Stack macro preprocessor
//  OWNER : Jiya (Member A)
//  ROLE  : Runs BEFORE the assembler. Expands
//            #define NAME value        (textual constants)
//            PUSHM Rn   ->  ADDI SP,-1 ; STR Rn,[SP]        (full-descending push)
//            POPM  Rn   ->  LDR Rn,[SP] ; ADDI SP,1         (pop)
//            PUSHALL    ->  PUSHM R0 .. PUSHM R6
//            POPALL     ->  POPM R6 .. POPM R0
//            ALLOC n    ->  ADDI SP,0-n                     (reserve n stack words)
//            FREE n     ->  ADDI SP,n                       (release n stack words)
// =====================================================================
#pragma once
#include <string>
#include <vector>

struct SrcLine {
    std::string text;   // one source line (comment removed)
    int line = 0;       // original line number in the user's file
    std::string from;   // non-empty if produced by a macro, e.g. "PUSHM R1"
};

struct PPResult {
    std::vector<SrcLine> lines;
    std::vector<std::string> errors;
};

PPResult preprocess(const std::string& source);
std::string renderExpanded(const PPResult& r);   // pretty listing for the CLI 'expand' command
