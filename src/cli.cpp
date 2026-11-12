// cli.cpp -- OWNER: Rani (Member C)
#include "cli.h"
#include "disassembler.h"
#include "evaluate.h"
#include "hazards.h"
#include "pipeviz.h"
#include "preprocessor.h"
#include "stackviz.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <unistd.h>

using namespace isa;

static std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v;
    std::istringstream is(s);
    std::string t;
    while (is >> t) v.push_back(t);
    return v;
}
static std::string lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
static bool endsWith(const std::string& s, const std::string& e) {
    return s.size() >= e.size() && lower(s.substr(s.size() - e.size())) == e;
}
static bool parseInt(const std::string& s, int& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    long x = (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? std::strtol(s.c_str() + 2, &end, 16)
                                                                          : std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return false;
    v = (int)x;
    return true;
}

Cli::Cli() { reconfigure(); }

void Cli::reconfigure() { cpu.configure(stages, style, fwd); }

bool Cli::resolveAddr(const std::string& s, int& out) {
    if (parseInt(s, out)) return out >= 0 && out < MEM_WORDS;
    if (cpu.loaded()) {
        auto it = cpu.program().symbols.find(s);
        if (it != cpu.program().symbols.end()) { out = it->second; return true; }
    }
    return false;
}

std::string Cli::statusLine() {
    char b[64];
    std::string s;
    if (cpu.halted) {
        std::snprintf(b, sizeof b, "%llu", (unsigned long long)cpu.stats.cycles);
        s = std::string("HALTED after ") + b + " cycles";
    } else if (cpu.faulted) s = std::string("EXCEPTION ") + exName(cpu.faultKind) + ": " + cpu.faultMsg;
    else s = "running (not halted)";
    return s;
}

bool Cli::loadProgram(const std::string& path) {
    std::string text;
    if (!readFile(path, text)) { std::cout << "cannot open '" << path << "'\n"; return false; }
    Program p;
    if (endsWith(path, ".asm") || endsWith(path, ".s")) {
        p = assemble(text);
        if (!p.ok()) {
            std::cout << "Assembly failed:\n";
            for (auto& e : p.errors) std::cout << "  " << e << "\n";
            return false;
        }
    } else {
        std::vector<std::string> errs;
        p = parseImage(text, errs);
        if (!errs.empty()) {
            for (auto& e : errs) std::cout << "  " << e << "\n";
            return false;
        }
    }
    cpu.load(p);
    progName = path;
    std::cout << "Loaded '" << path << "': " << (p.lastAddr + 1) << " words, " << p.symbols.size() << " symbols. Machine reset.\n";
    return true;
}

void Cli::printRegs() {
    std::cout << " REGISTER FILE\n";
    for (int i = 0; i < NUM_REGS; i++) {
        uint16_t v = cpu.regs.read(i);
        char b[80];
        std::snprintf(b, sizeof b, "  %-6s = 0x%04X  (%6d)%s", i == SP_REG ? "R7/SP" : ("R" + std::to_string(i)).c_str(), v, (int16_t)v,
                      (i % 2 == 1) ? "\n" : "     ");
        std::cout << b;
    }
    AluFlags f = cpu.alu.lastFlags;
    std::cout << "  PC = " << cpu.pc << "   last ALU flags: Z=" << f.z << " N=" << f.n << " C=" << f.c << " V=" << f.v << "\n";
}

void Cli::cmdMem(const std::string& a, const std::string& n) {
    int addr = 0, cnt = 16;
    if (!a.empty() && !resolveAddr(a, addr)) { std::cout << "bad address '" << a << "'\n"; return; }
    if (!n.empty() && !parseInt(n, cnt)) { std::cout << "bad count\n"; return; }
    for (int i = 0; i < cnt; i += 8) {
        char b[16];
        std::snprintf(b, sizeof b, "  %3d:", addr + i);
        std::cout << b;
        for (int j = 0; j < 8 && i + j < cnt && addr + i + j < MEM_WORDS; j++) {
            std::snprintf(b, sizeof b, " %04X", cpu.mem.peek(addr + i + j));
            std::cout << b;
        }
        std::cout << "\n";
    }
}

void Cli::cmdStats() {
    const Stats& s = cpu.stats;
    double cpi = s.retired ? (double)s.cycles / (double)s.retired : 0.0;
    std::cout << " PERFORMANCE (" << cpu.stages() << "-stage, " << (cpu.cu.style() == CtlStyle::Horizontal ? "horizontal" : "vertical")
              << " microcode, forwarding " << (cpu.forwarding() ? "on" : "off") << ")\n";
    std::cout << "  cycles=" << s.cycles << "  retired=" << s.retired << "  CPI=" << cpi << "\n";
    std::cout << "  data-hazard stall cycles=" << s.dataStalls << "  branches taken=" << s.branchesTaken
              << "  flushed instrs=" << s.flushedInsts << "  control bubbles=" << s.controlBubbles << "\n";
    std::cout << "  loads=" << s.loads << "  stores=" << s.stores << "\n";
    std::cout << "  micro-instructions executed=" << cpu.cu.microCycles << " for " << cpu.cu.dispatches << " decodes\n";
    std::cout << "  control store: " << cpu.cu.storeWords() << " words x " << cpu.cu.wordBits() << " bits, total " << cpu.cu.totalBits() << " bits\n";
    std::cout << "  ALU: " << cpu.alu.describe() << "\n";
    std::cout << "  ALU ops=" << cpu.alu.totalOps << "  modelled gate delay=" << cpu.alu.delayUnits << "  multiplier add/sub ops=" << cpu.alu.mulAddOps << "\n";
}

void Cli::cmdStep(int n) {
    for (int i = 0; i < n; i++) {
        bool alive = cpu.step();
        std::cout << "--- cycle " << cpu.stats.cycles << " ---\n";
        for (auto& l : cpu.log) std::cout << "  " << l << "\n";
        if (i == n - 1 || !alive) std::cout << cpu.pipelineText();
        if (cpu.lastFetch >= 0 && cpu.breakpoints.count(cpu.lastFetch)) { std::cout << "  (breakpoint at " << cpu.lastFetch << ")\n"; break; }
        if (!alive) break;
    }
    if (cpu.halted || cpu.faulted) std::cout << " " << statusLine() << "\n";
}

void Cli::cmdCompare(const std::vector<std::string>& args) {
    if (args.empty()) {
        if (cpu.loaded()) std::cout << compareProgram(cpu.program(), progName.empty() ? "loaded program" : progName);
        std::cout << compareAlu();
        return;
    }
    for (auto& a : args) {
        if (lower(a) == "alu") { std::cout << compareAlu(); continue; }
        std::string text;
        if (!readFile(a, text)) { std::cout << "cannot open '" << a << "'\n"; continue; }
        Program p = assemble(text);
        if (!p.ok()) { std::cout << a << ": assembly errors\n"; for (auto& e : p.errors) std::cout << "  " << e << "\n"; continue; }
        std::cout << compareProgram(p, a);
    }
}

void Cli::cmdDis(const std::vector<std::string>& args) {
    bool asmForm = false;
    std::string file;
    for (auto& a : args) { if (a == "-a") asmForm = true; else file = a; }
    if (file.empty()) {
        if (!cpu.loaded()) { std::cout << "no program loaded\n"; return; }
        std::cout << disassembleProgram(cpu.program(), asmForm);
        return;
    }
    std::string text;
    if (!readFile(file, text)) { std::cout << "cannot open '" << file << "'\n"; return; }
    std::vector<std::string> errs;
    Program p = parseImage(text, errs);
    for (auto& e : errs) std::cout << "  " << e << "\n";
    std::cout << disassembleProgram(p, asmForm);
}

void Cli::cmdAsm(const std::vector<std::string>& args) {
    if (args.empty()) { std::cout << "usage: asm <file.asm> [outbase]\n"; return; }
    std::string text;
    if (!readFile(args[0], text)) { std::cout << "cannot open '" << args[0] << "'\n"; return; }
    Program p = assemble(text);
    if (!p.ok()) {
        std::cout << "Assembly failed:\n";
        for (auto& e : p.errors) std::cout << "  " << e << "\n";
        return;
    }
    std::string base = args.size() > 1 ? args[1] : args[0];
    size_t dot = base.rfind('.');
    if (args.size() == 1 && dot != std::string::npos) base = base.substr(0, dot);
    writeFile(base + ".hex", toHex(p));
    writeFile(base + ".bin", toBin(p));
    writeFile(base + ".lst", toListing(p));
    std::cout << "Assembled " << (p.lastAddr + 1) << " words -> " << base << ".hex, " << base << ".bin, " << base << ".lst\n";
    cpu.load(p);
    progName = args[0];
    std::cout << "Program loaded into the simulator.\n";
}

void Cli::cmdPipeline(const std::vector<std::string>& args) {
    if (!cpu.loaded()) { std::cout << "no program loaded (use: load <file>)\n"; return; }
    int pre = 0;
    if (!args.empty()) {
        int v;
        if (parseInt(args[0], v) && (v == 4 || v == 6)) pre = v;
        else { std::cout << "usage: pipeline [4|6]   (no argument shows the architecture menu)\n"; return; }
    }
    PipelineViz viz(cpu.program(), progName, style, fwd, *in_, interactive_);
    viz.session(pre);          // runs on its own CPU instance: main machine state is untouched
}

void Cli::help() {
    std::cout <<
"RISC201 simulator commands\n"
"  FILES     asm <f.asm> [base]   assemble -> .hex .bin .lst and load it\n"
"            load <f>             load .asm (assembled) or .hex/.bin\n"
"            expand <f.asm>       show source after stack-macro preprocessing\n"
"            dis [-a] [f.hex]     disassemble loaded program or a hex/bin file (-a = re-assemblable)\n"
"  RUN       run [maxcycles]      run until HALT / exception / breakpoint\n"
"            step [n]             advance n clock cycles (shows every stage event)\n"
"            next                 run until one more instruction retires\n"
"            reset                reset machine, keep the program\n"
"  BREAK     break <addr|label>   breakpoint on instruction fetch      breaks / delbreak <a|all>\n"
"  INSPECT   regs                 register file             mem <addr> [n]  memory dump\n"
"            stack                ASCII stack visualiser\n"
"            pipe / pipeline [4|6] animated pipeline visualizer (Rani's 4-stage / 6-stage view);\n"
"                                 inside: next prev run pause reset status switch exit\n"
"            snapshot             compact CPU pipeline-state diagnostic\n"
"            chart                cycle-by-stage pipeline chart\n"
"            upc [on|off]         show micro-PC / control-store steps at every decode\n"
"            stats                cycles, CPI, stalls, ALU statistics\n"
"            hazards              static RAW/load-use/control hazard report (4/6 stages, fwd on/off)\n"
"  CONFIG    mode <4|6>           pipeline depth (resets)    fwd <on|off>   forwarding\n"
"            ctl <h|v|dump>       horizontal / vertical microcode, or dump control store\n"
"            alu                  show ALU algorithms; alu add <ripple|cla>; alu mul <shift|booth>\n"
"  EVALUATE  compare [f.asm..|alu]   run every configuration side by side\n"
"  OTHER     help, quit\n";
}

bool Cli::execute(const std::string& line) {
    std::vector<std::string> t = split(line);
    if (t.empty()) return true;
    std::string c = lower(t[0]);
    std::vector<std::string> args(t.begin() + 1, t.end());

    if (c == "quit" || c == "exit" || c == "q") return false;
    else if (c == "help" || c == "?") help();
    else if (c == "load") { if (args.empty()) std::cout << "usage: load <file>\n"; else loadProgram(args[0]); }
    else if (c == "asm") cmdAsm(args);
    else if (c == "dis") cmdDis(args);
    else if (c == "expand") {
        std::string text;
        if (args.empty() || !readFile(args[0], text)) std::cout << "usage: expand <file.asm>\n";
        else std::cout << renderExpanded(preprocess(text));
    }
    else if (c == "run") {
        if (!cpu.loaded()) { std::cout << "no program loaded (use: load <file>)\n"; return true; }
        int mx = 200000;
        if (!args.empty()) parseInt(args[0], mx);
        if (cpu.halted || cpu.faulted) { std::cout << "machine already finished; use 'reset'\n"; return true; }
        cpu.run((uint64_t)mx, true);
        if (cpu.hitBreak) std::cout << " breakpoint: instruction at " << cpu.lastFetch << " fetched (cycle " << cpu.stats.cycles << ")\n" << cpu.pipelineText();
        else std::cout << " " << statusLine() << "\n";
        double cpi = cpu.stats.retired ? (double)cpu.stats.cycles / (double)cpu.stats.retired : 0;
        std::cout << " cycles=" << cpu.stats.cycles << " retired=" << cpu.stats.retired << " CPI=" << cpi << "\n";
    }
    else if (c == "step" || c == "s") {
        if (!cpu.loaded()) { std::cout << "no program loaded\n"; return true; }
        int n = 1;
        if (!args.empty()) parseInt(args[0], n);
        if (cpu.halted || cpu.faulted) { std::cout << " " << statusLine() << "  (use 'reset')\n"; return true; }
        cmdStep(n);
    }
    else if (c == "next" || c == "n") {
        if (!cpu.loaded()) { std::cout << "no program loaded\n"; return true; }
        if (cpu.halted || cpu.faulted) { std::cout << " " << statusLine() << "  (use 'reset')\n"; return true; }
        uint64_t r = cpu.stats.retired;
        std::vector<std::string> all;
        bool alive = true;
        while (alive) { alive = cpu.step(); for (auto& l : cpu.log) all.push_back(l); if (cpu.stats.retired > r) break; }
        for (auto& l : all) std::cout << "  " << l << "\n";
        std::cout << cpu.pipelineText();
        if (cpu.halted || cpu.faulted) std::cout << " " << statusLine() << "\n";
    }
    else if (c == "reset") { cpu.reset(); std::cout << "machine reset\n"; }
    else if (c == "regs" || c == "r") printRegs();
    else if (c == "mem") cmdMem(args.size() > 0 ? args[0] : "", args.size() > 1 ? args[1] : "");
    else if (c == "stack") std::cout << renderStack(cpu.mem, cpu.regs.sp());
    else if (c == "pipe" || c == "pipeline" || c == "viz") cmdPipeline(args);
    else if (c == "snapshot") std::cout << cpu.pipelineText();
    else if (c == "chart") std::cout << cpu.chartText();
    else if (c == "stats") cmdStats();
    else if (c == "upc") {
        if (!args.empty()) cpu.upcTrace = (lower(args[0]) == "on");
        else cpu.upcTrace = !cpu.upcTrace;
        std::cout << "micro-PC trace " << (cpu.upcTrace ? "ON" : "OFF") << "\n";
    }
    else if (c == "break") {
        int a;
        if (args.empty() || !resolveAddr(args[0], a)) std::cout << "usage: break <addr|label>\n";
        else { cpu.breakpoints.insert(a); std::cout << "breakpoint set at " << a << "\n"; }
    }
    else if (c == "breaks") {
        if (cpu.breakpoints.empty()) std::cout << "no breakpoints\n";
        for (int a : cpu.breakpoints) std::cout << "  " << a << "\n";
    }
    else if (c == "delbreak") {
        int a;
        if (!args.empty() && lower(args[0]) == "all") { cpu.breakpoints.clear(); std::cout << "all breakpoints removed\n"; }
        else if (!args.empty() && resolveAddr(args[0], a)) { cpu.breakpoints.erase(a); std::cout << "removed\n"; }
        else std::cout << "usage: delbreak <addr|label|all>\n";
    }
    else if (c == "mode") {
        int m;
        if (args.empty() || !parseInt(args[0], m) || (m != 4 && m != 6)) std::cout << "usage: mode <4|6>   (currently " << stages << ")\n";
        else { stages = m; reconfigure(); std::cout << "pipeline is now " << stages << "-stage (machine reset)\n"; }
    }
    else if (c == "fwd") {
        if (args.empty()) std::cout << "forwarding is " << (fwd ? "on" : "off") << "\n";
        else { fwd = lower(args[0]) == "on"; reconfigure(); std::cout << "forwarding " << (fwd ? "on" : "off") << " (machine reset)\n"; }
    }
    else if (c == "ctl") {
        if (args.empty()) std::cout << "control unit: " << (style == CtlStyle::Horizontal ? "horizontal" : "vertical") << "\n";
        else if (lower(args[0]) == "dump") std::cout << cpu.cu.dump();
        else if (lower(args[0]) == "h") { style = CtlStyle::Horizontal; reconfigure(); std::cout << "horizontal microcode selected (machine reset)\n"; }
        else if (lower(args[0]) == "v") { style = CtlStyle::Vertical; reconfigure(); std::cout << "vertical microcode selected (machine reset)\n"; }
        else std::cout << "usage: ctl <h|v|dump>\n";
    }
    else if (c == "alu") {
        if (args.size() == 2 && lower(args[0]) == "add") {
            if (lower(args[1]) == "ripple") cpu.alu.setAddAlgo(AddAlgo::Ripple);
            else if (lower(args[1]) == "cla" || lower(args[1]) == "kogge-stone") cpu.alu.setAddAlgo(AddAlgo::Lookahead);
            else if (lower(args[1]) == "carryselect") cpu.alu.setAddAlgo(AddAlgo::CarrySelect);
            else if (lower(args[1]) == "brentkung") cpu.alu.setAddAlgo(AddAlgo::BrentKung);
        } else if (args.size() == 2 && lower(args[0]) == "mul") {
            if (lower(args[1]) == "shift") cpu.alu.setMulAlgo(MulAlgo::ShiftAdd);
            else if (lower(args[1]) == "booth") cpu.alu.setMulAlgo(MulAlgo::Booth);
            else if (lower(args[1]) == "wallace") cpu.alu.setMulAlgo(MulAlgo::Wallace);
        }
        std::cout << " " << cpu.alu.describe() << "\n";
    }
    else if (c == "hazards") {
        if (!cpu.loaded()) std::cout << "no program loaded (use: load <file.asm>)\n";
        else std::cout << analyzeAllHazards(cpu.program());
    }
    else if (c == "compare") cmdCompare(args);
    else if (c == "pipeline") cmdPipeline(args);
    else std::cout << "unknown command '" << c << "' (type 'help')\n";
    return true;
}

int Cli::loop(std::istream& in, bool interactive) {
    in_ = &in;
    interactive_ = interactive;
    std::string line;
    while (true) {
        if (interactive) { std::cout << "risc201> " << std::flush; }
        if (!std::getline(in, line)) break;
        if (!interactive && !line.empty() && line[0] != '#') std::cout << "risc201> " << line << "\n";
        if (!line.empty() && line[0] == '#') continue;
        if (!execute(line)) break;
    }
    return 0;
}
