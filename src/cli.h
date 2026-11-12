// =====================================================================
//  cli.h  --  Interactive command-line simulator / debugger
//  OWNER : Rani (Member C)
//  ROLE  : Front end of the whole suite. Loads programs, single-steps the
//          pipeline, shows registers / memory / stack / micro-PC state,
//          manages breakpoints and runs the comparison tools.
// =====================================================================
#pragma once
#include <iostream>
#include <string>
#include "assembler.h"
#include "pipeline.h"

class Cli {
public:
    Cli();
    int loop(std::istream& in, bool interactive);
    bool execute(const std::string& line);          // false => quit
    bool loadProgram(const std::string& path);      // .asm is assembled, .hex/.bin is loaded raw
    CPU cpu;
    void printRegs();
    std::string statusLine();
private:
    std::istream* in_ = &std::cin;      // stream the command loop reads (shared with sub-modes)
    bool interactive_ = true;
    std::string progName;
    int stages = 4;
    CtlStyle style = CtlStyle::Horizontal;
    bool fwd = true;
    void reconfigure();
    bool resolveAddr(const std::string& s, int& out);
    void help();
    void cmdStep(int n);
    void cmdMem(const std::string& a, const std::string& n);
    void cmdStats();
    void cmdCompare(const std::vector<std::string>& args);
    void cmdDis(const std::vector<std::string>& args);
    void cmdAsm(const std::vector<std::string>& args);
    void cmdPipeline(const std::vector<std::string>& args);   // pipeline visualizer sub-mode
};
