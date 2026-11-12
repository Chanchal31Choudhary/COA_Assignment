// =====================================================================
//  main.cpp  --  RISC201 suite entry point
//  OWNER : Rani (Member C)
//
//  Usage
//    risc201                          interactive CLI
//    risc201 prog.asm                 interactive CLI with prog.asm loaded
//    risc201 --script cmds.txt        run CLI commands from a file
//    risc201 --hazards prog.asm       static hazard report (4/6 stages, forwarding on/off)
//    risc201 --batch prog.asm [--stages 4|6] [--ctl h|v] [--fwd on|off]
//                             [--mem ADDR N] [--stats]
//        non-interactive run used by run_tests.sh: prints STATUS and registers
// =====================================================================
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include "cli.h"
#include "evaluate.h"
#include "hazards.h"

static int batchMode(int argc, char** argv) {
    std::string file;
    int stages = 4, memA = -1, memN = 0;
    CtlStyle style = CtlStyle::Horizontal;
    bool fwd = true, stats = false;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (i == 2 && a[0] != '-') file = a;
        else if (a == "--stages" && i + 1 < argc) stages = std::atoi(argv[++i]);
        else if (a == "--ctl" && i + 1 < argc) style = (argv[++i][0] == 'v') ? CtlStyle::Vertical : CtlStyle::Horizontal;
        else if (a == "--fwd" && i + 1 < argc) fwd = (std::strcmp(argv[++i], "on") == 0);
        else if (a == "--mem" && i + 2 < argc) { memA = std::atoi(argv[++i]); memN = std::atoi(argv[++i]); }
        else if (a == "--stats") stats = true;
    }
    if (file.empty()) { std::cerr << "usage: risc201 --batch prog.asm [options]\n"; return 2; }
    std::string text;
    if (!readFile(file, text)) { std::cerr << "cannot open " << file << "\n"; return 2; }
    Program p;
    bool isHex = file.size() > 4 && (file.substr(file.size() - 4) == ".hex" || file.substr(file.size() - 4) == ".bin");
    if (isHex) {
        std::vector<std::string> errs;
        p = parseImage(text, errs);
        if (!errs.empty()) { std::cout << "STATUS: IMAGE_ERROR\n"; return 1; }
    } else {
        p = assemble(text);
        if (!p.ok()) {
            std::cout << "STATUS: ASSEMBLY_ERROR\n";
            for (auto& e : p.errors) std::cout << "  " << e << "\n";
            return 1;
        }
    }
    CPU cpu;
    cpu.configure(stages, style, fwd);
    cpu.load(p);
    cpu.run(200000, false);
    if (cpu.halted) std::cout << "STATUS: HALTED\n";
    else std::cout << "STATUS: EXCEPTION " << exName(cpu.faultKind) << " " << cpu.faultMsg << "\n";
    char b[32];
    std::cout << "REGS:";
    for (int i = 0; i < 8; i++) { std::snprintf(b, sizeof b, " R%d=%04X", i, cpu.regs.read(i)); std::cout << b; }
    std::cout << "\n";
    if (memA >= 0) {
        std::cout << "MEM[" << memA << ".." << (memA + memN - 1) << "]:";
        for (int i = 0; i < memN && memA + i < 256; i++) { std::snprintf(b, sizeof b, " %04X", cpu.mem.peek(memA + i)); std::cout << b; }
        std::cout << "\n";
    }
    if (stats) {
        std::cout << "CYCLES: " << cpu.stats.cycles << " RETIRED: " << cpu.stats.retired << " STALLS: " << cpu.stats.dataStalls
                  << " FLUSHED: " << cpu.stats.flushedInsts << "\n";
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--batch") == 0) return batchMode(argc, argv);
    if (argc >= 3 && std::strcmp(argv[1], "--hazards") == 0) {
        std::string text;
        if (!readFile(argv[2], text)) { std::cerr << "cannot open " << argv[2] << "\n"; return 2; }
        Program p = assemble(text);
        if (!p.ok()) {
            std::cerr << "Assembly failed:\n";
            for (const auto& e : p.errors) std::cerr << "  " << e << "\n";
            return 1;
        }
        std::cout << analyzeAllHazards(p);
        return 0;
    }

    Cli cli;
    bool interactive = isatty(0);
    if (argc >= 3 && std::strcmp(argv[1], "--script") == 0) {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open script " << argv[2] << "\n"; return 2; }
        return cli.loop(f, false);
    }
    std::cout << "RISC201 simulator  (4-stage / 6-stage pipeline, micro-programmed control)   type 'help'\n";
    if (argc >= 2) cli.loadProgram(argv[1]);
    return cli.loop(std::cin, interactive);
}
