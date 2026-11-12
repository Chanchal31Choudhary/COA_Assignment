// pipeviz.cpp -- OWNER: Rani (Member C)  (visualizer)  /  CPU model: Kavya (Member D)
#include "pipeviz.h"
#include "disassembler.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unistd.h>
#ifdef _WIN32
#include <conio.h>      // _kbhit
#include <windows.h>    // Sleep, console mode
#else
#include <poll.h>
#endif

using namespace isa;

namespace {

constexpr uint64_t kMaxCycles = 10000;     // safety net for endless programs
constexpr int kChartCols = 12;             // cycles shown in the pipeline chart
constexpr int kChartRows = 12;             // instructions shown in the pipeline chart

struct Glyph { const char *h, *dh, *v, *cross, *move, *block, *none, *from, *sep; };
const Glyph UNI = {"\xE2\x94\x80", "\xE2\x95\x90", "\xE2\x94\x82", "\xE2\x94\xBC",
                   "\xE2\x94\x81\xE2\x94\x81\xE2\x96\xBA", "\xE2\x94\x80x\xE2\x94\x80",
                   "\xC2\xB7\xC2\xB7\xC2\xB7", "\xE2\x97\x84", "\xE2\x96\xBA"};
const Glyph ASC = {"-", "=", "|", "+", "==>", "-x-", "...", "<-", ">"};

// ---- text helpers (UTF-8 and ANSI aware) --------------------------------
size_t vlen(const std::string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1B) { while (i < s.size() && s[i] != 'm') i++; i++; continue; }
        if ((c & 0xC0) != 0x80) n++;
        i++;
    }
    return n;
}
std::string padR(const std::string& s, size_t w) { size_t l = vlen(s); return l >= w ? s : s + std::string(w - l, ' '); }
std::string center(const std::string& s, size_t w) {
    size_t l = vlen(s);
    if (l >= w) return s;
    size_t left = (w - l) / 2;
    return std::string(left, ' ') + s + std::string(w - l - left, ' ');
}
std::string clip(const std::string& s, size_t w) { return s.size() <= w ? s : s.substr(0, w); }
std::string rep(const std::string& s, int n) { std::string r; for (int i = 0; i < n; i++) r += s; return r; }
std::string lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v; std::istringstream is(s); std::string t;
    while (is >> t) v.push_back(t);
    return v;
}
bool parseNum(const std::string& s, int& v) {
    if (s.empty()) return false;
    char* e = nullptr;
    long x = std::strtol(s.c_str(), &e, 10);
    if (*e != '\0') return false;
    v = (int)x;
    return true;
}
std::string regName(int r) { return r == SP_REG ? "SP" : "R" + std::to_string(r); }
std::string maskNames(unsigned m) {
    std::string s;
    for (int r = 0; r < NUM_REGS; r++) if (m & (1u << r)) s += (s.empty() ? "" : ",") + regName(r);
    return s;
}
std::string firstWord(const std::string& s) { size_t p = s.find(' '); return p == std::string::npos ? s : s.substr(0, p); }
bool isLoadLike(const std::string& m) { return m == "LOAD" || m == "LDR" || m == "POP" || m == "RET"; }
bool isCondBranch(const std::string& m) { return m == "BZ" || m == "BNZ" || m == "BNEG"; }

bool utf8Locale() {
    for (const char* k : {"LC_ALL", "LC_CTYPE", "LANG"}) {
        const char* v = std::getenv(k);
        if (v && *v) { std::string s = lower(v); return s.find("utf-8") != std::string::npos || s.find("utf8") != std::string::npos; }
    }
    return false;
}

}  // namespace

// ======================================================================
//  construction / machine
// ======================================================================
PipelineViz::PipelineViz(const Program& prog, const std::string& progName, CtlStyle style, bool forwarding,
                         std::istream& in, bool interactive)
    : prog_(prog), name_(progName.empty() ? "loaded program" : progName), style_(style), fwd_(forwarding),
      in_(in), interactive_(interactive) {
#ifdef _WIN32
    {   // let the Windows console understand ANSI colour codes (Windows 10+)
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);
    }
#endif
    const char* term = std::getenv("TERM");
    color_ = isatty(STDOUT_FILENO) && !std::getenv("NO_COLOR") && !(term && std::strcmp(term, "dumb") == 0);
    unicode_ = utf8Locale();
#ifdef _WIN32
    unicode_ = false;       // Windows consoles are rarely UTF-8: default to plain ASCII glyphs ('unicode on' to opt in)
#endif
    delayMs_ = 500;
}

void PipelineViz::configure() {
    cpu_.configure(stages_, style_, fwd_);       // resets the machine
    cpu_.load(prog_);                            // program image back into memory
    frames_.clear();
    info_.clear();
    frames_.push_back(initialFrame());
    cur_ = 0;
    running_ = false;
    pending_.clear();
}

PipelineViz::Frame PipelineViz::initialFrame() const {
    Frame f;
    f.cycle = 0;
    f.cells.assign(stages_, Cell());
    for (auto& c : f.cells) c.bubble = Bubble::Fill;
    for (int r = 0; r < 8; r++) f.regs[r] = cpu_.regs.read(r);
    f.pc = cpu_.pc;
    return f;
}

bool PipelineViz::finished() const {
    return cpu_.halted || cpu_.faulted || frames_.back().cycle >= kMaxCycles;
}

int PipelineViz::stageIndex(const std::string& name) const {
    for (int i = 0; i < cpu_.stages(); i++) if (name == cpu_.stageName(i)) return i;
    return -1;
}

int PipelineViz::stageOf(const Frame& f, uint64_t seq) const {
    for (size_t i = 0; i < f.cells.size(); i++) if (f.cells[i].valid && f.cells[i].seq == seq) return (int)i;
    return -1;
}

int PipelineViz::inFlight() const {
    int n = 0;
    for (auto& c : frames_.back().cells) if (c.valid) n++;
    return n;
}

// Advance the real CPU by one clock and record what happened as a Frame.
void PipelineViz::stepLive() {
    const Frame prev = frames_.back();
    cpu_.step();
    const int N = cpu_.stages();

    Frame f;
    f.cycle = cpu_.stats.cycles;
    f.cells.assign(N, Cell());
    f.events = cpu_.events;
    f.stats = cpu_.stats;
    f.pc = cpu_.pc;
    f.halted = cpu_.halted;
    f.faulted = cpu_.faulted;
    f.faultMsg = cpu_.faultMsg;
    for (int r = 0; r < 8; r++) f.regs[r] = cpu_.regs.read(r);

    // 1. what occupied each stage during this cycle, and how it got there
    for (int i = 0; i < N; i++) {
        const StageView& v = cpu_.occupancy[i];
        if (!v.valid) continue;
        Cell& c = f.cells[i];
        c.valid = true; c.seq = v.seq; c.pc = v.pc; c.text = v.text;
        info_[v.seq] = {v.pc, v.text};
        int j = stageOf(prev, v.seq);
        if (j == i) c.held = true;
        else { c.entered = true; c.from = (j >= 0) ? j : -1; }
    }
    // 2. mark hazards / flushes / retirement reported by the CPU
    for (const PipeEvent& ev : f.events) {
        auto at = [&](int s) -> Cell* { return (s >= 0 && s < N) ? &f.cells[s] : nullptr; };
        switch (ev.kind) {
            case EvKind::Stall:
                if (Cell* c = at(ev.stage)) { c->stalled = true; c->stallProducer = ev.other; c->stallMask = ev.regMask; }
                break;
            case EvKind::Forward:
                if (Cell* c = at(ev.stage)) c->fwd.push_back({ev.reg, ev.other});
                break;
            case EvKind::Redirect:
                if (Cell* c = at(ev.stage)) { c->taken = true; c->target = ev.target; }
                break;
            case EvKind::Flush:
                for (uint64_t s : ev.seqs) { int k = stageOf(f, s); if (k >= 0) f.cells[k].flushed = true; }
                break;
            case EvKind::Halt:
                if (Cell* c = at(ev.stage)) c->halt = true;
                break;
            case EvKind::Retire:
                if (Cell* c = at(ev.stage)) c->retire = true;
                break;
            case EvKind::Exception:
                if (Cell* c = at(ev.stage)) { c->fault = true; c->faultText = ev.text; }
                break;
        }
    }
    // 3. explain every bubble
    for (int i = 0; i < N; i++) {
        Cell& c = f.cells[i];
        if (c.valid) continue;
        if (f.cycle <= (uint64_t)i) { c.bubble = Bubble::Fill; continue; }
        bool flushedHere = false;
        for (auto& ev : prev.events) {
            if (ev.kind != EvKind::Flush) continue;
            for (uint64_t s : ev.seqs) if (stageOf(prev, s) + 1 == i) flushedHere = true;
        }
        if (flushedHere) { c.bubble = Bubble::Flush; continue; }
        if (i > 0 && prev.cells[i - 1].valid && prev.cells[i - 1].stalled) { c.bubble = Bubble::Stall; continue; }
        bool upstreamEmpty = true;
        for (int k = 0; k < i; k++) if (f.cells[k].valid) upstreamEmpty = false;
        c.bubble = upstreamEmpty ? Bubble::Drain : Bubble::Idle;
    }
    frames_.push_back(f);
}

bool PipelineViz::forwardOne() {
    if (cur_ + 1 < frames_.size()) { cur_++; return true; }
    if (finished()) return false;
    stepLive();
    cur_ = frames_.size() - 1;
    return true;
}

// ======================================================================
//  small drawing helpers
// ======================================================================
std::string PipelineViz::paint(const std::string& s, const char* code) const {
    return color_ ? std::string("\033[") + code + "m" + s + "\033[0m" : s;
}
const char* PipelineViz::stageBg(const std::string& n) const {
    if (n == "IF") return "30;104";
    if (n == "ID") return "30;106";
    if (n == "RF") return "30;105";
    if (n == "EX") return "30;102";
    if (n == "MEM") return "30;103";
    return "30;107";   // WB
}
const char* PipelineViz::stageFg(const std::string& n) const {
    if (n == "IF") return "94";
    if (n == "ID") return "96";
    if (n == "RF") return "95";
    if (n == "EX") return "92";
    if (n == "MEM") return "93";
    return "97";
}
std::string PipelineViz::stageTag(const std::string& name, const std::string& s) const {
    return paint(s, (std::string("1;") + stageFg(name)).c_str());
}
std::string PipelineViz::rule(char c) const {
    const Glyph& g = unicode_ ? UNI : ASC;
    return rep(c == '=' ? g.dh : g.h, 86);
}
std::string PipelineViz::label(uint64_t seq) const {
    auto it = info_.find(seq);
    if (it == info_.end()) return "#" + std::to_string(seq);
    return "#" + std::to_string(seq) + " [" + std::to_string(it->second.first) + "] " + it->second.second;
}
std::string PipelineViz::mnemonic(uint64_t seq) const {
    auto it = info_.find(seq);
    return it == info_.end() ? "" : firstWord(it->second.second);
}

// Role of each stage -- taken from the CPU model (pipeline.h / pipeline.cpp), not invented.
std::string PipelineViz::stageRole(int i) const {
    std::string n = cpu_.stageName(i);
    if (n == "IF") return "instruction fetch";
    if (n == "ID") return cpu_.stages() == 4 ? "decode (micro-ROM) + register read" : "decode (micro-ROM)";
    if (n == "RF") return "register read + RAW check";
    if (n == "EX") return cpu_.stages() == 4 ? "ALU + memory access" : "ALU / branch resolve";
    if (n == "MEM") return "data memory (LOAD/STORE/POP/RET)";
    return "register write-back (retire)";
}

// ======================================================================
//  rendering
// ======================================================================
std::string PipelineViz::header(const Frame& f) const {
    std::ostringstream o;
    std::string arch;
    for (int i = 0; i < cpu_.stages(); i++) {
        if (i) arch += std::string(" ") + (unicode_ ? UNI.sep : ASC.sep) + " ";
        arch += stageTag(cpu_.stageName(i), cpu_.stageName(i));
    }
    o << paint(rule('='), "1") << "\n";
    o << " " << paint("RISC201 PIPELINE VISUALIZER", "1") << "   " << cpu_.stages() << "-stage: " << arch
      << "   forwarding " << (fwd_ ? paint("ON", "1;92") : paint("OFF", "1;91")) << "   "
      << (name_.find('/') == std::string::npos ? name_ : name_.substr(name_.rfind('/') + 1)) << "\n";
    std::string state;
    if (f.cycle == frames_.back().cycle && finished()) {
        state = f.faulted ? paint("FINISHED: EXCEPTION " + std::string(exName(cpu_.faultKind)), "1;91")
                          : (f.halted ? paint("FINISHED: HALTED", "1;92") : paint("FINISHED: cycle cap reached", "1;93"));
    } else if (running_) state = paint("RUNNING", "1;92");
    else state = paint("PAUSED", "1;93");
    std::string pos = (cur_ + 1 == frames_.size()) ? "latest" : "history " + std::to_string(cur_) + "/" + std::to_string(frames_.size() - 1);
    o << " " << paint("CYCLE " + std::to_string(f.cycle), "1;97") << "  (" << pos << ")   " << state
      << "   next fetch PC=" << f.pc << "\n";
    o << paint(rule('='), "1") << "\n";
    return o.str();
}

// Horizontal pipeline diagram: stage boxes, arrows that show which instructions moved.
std::string PipelineViz::strip(const Frame& f, const Frame* prev) const {
    (void)prev;
    const Glyph& g = unicode_ ? UNI : ASC;
    const int N = cpu_.stages();
    const size_t W = (N == 4) ? 14 : 10;
    std::string l1, l2, l3, l4;
    for (int i = 0; i < N; i++) {
        const Cell& c = f.cells[i];
        std::string nm = cpu_.stageName(i);
        l1 += paint(center(nm, W), stageBg(nm));
        std::string idl, mn, tag;
        if (c.valid) {
            idl = "#" + std::to_string(c.seq) + " @" + std::to_string(c.pc);
            mn = (c.text == "<bad fetch>") ? "badfetch" : firstWord(c.text);
            if (c.flushed) tag = paint(center("FLUSHED", W), "97;41");
            else if (c.fault) tag = paint(center("EXCEPTION", W), "97;41");
            else if (c.stalled) tag = paint(center("STALL", W), "30;43");
            else if (c.taken) tag = paint(center("TAKEN", W), "97;45");
            else if (!c.fwd.empty()) tag = paint(center("FORWARD", W), "30;46");
            else if (c.halt) tag = paint(center("HALT", W), "1;91");
            else if (c.retire) tag = paint(center("RETIRE", W), "1;92");
            else if (c.held) tag = paint(center("held", W), "2;33");
            l2 += stageTag(nm, padR(center(clip(idl, W), W), W));
            l3 += stageTag(nm, padR(center(clip(mn, W), W), W));
        } else {
            static const char* why[] = {"", "fill", "stall", "flush", "drain", "idle"};
            idl = "bubble";
            mn = std::string("(") + why[(int)c.bubble] + ")";
            l2 += paint(center(idl, W), "2;90");
            l3 += paint(center(mn, W), "2;90");
        }
        l4 += tag.empty() ? std::string(W, ' ') : tag;
        if (i + 1 < N) {
            const Cell& nx = f.cells[i + 1];
            std::string a;
            if (nx.valid && nx.entered && nx.from == i) a = paint(g.move, "1;92");       // moved forward
            else if (c.valid && c.held) a = paint(g.block, "1;93");                      // blocked
            else a = paint(g.none, "2;90");                                              // nothing moved
            l1 += a; l2 += "   "; l3 += "   "; l4 += "   ";
        }
    }
    return " " + l1 + "\n " + l2 + "\n " + l3 + "\n " + l4 + "\n";
}

std::string PipelineViz::bubbleText(const Frame& f, int i) const {
    (void)f;
    switch (f.cells[i].bubble) {
        case Bubble::Fill:  return "bubble: pipeline filling (no instruction has reached this stage yet)";
        case Bubble::Stall: return "bubble: inserted behind the data-hazard stall in " + std::string(cpu_.stageName(i - 1));
        case Bubble::Flush: return "bubble: slot of a flushed instruction";
        case Bubble::Drain: return "bubble: pipeline draining (instruction fetch has stopped)";
        default:            return "bubble";
    }
}

std::string PipelineViz::statusText(const Frame& f, int i) const {
    const Cell& c = f.cells[i];
    if (!c.valid) return paint(bubbleText(f, i), "2;90");
    const Glyph& g = unicode_ ? UNI : ASC;
    std::vector<std::string> parts;
    if (c.entered) parts.push_back(c.from < 0 ? "fetched this cycle"
                                              : std::string(g.from) + " moved from " + cpu_.stageName(c.from));
    if (c.stalled) {
        int ps = stageOf(f, c.stallProducer);
        parts.push_back(paint("STALL RAW " + maskNames(c.stallMask) + " (waits for #" + std::to_string(c.stallProducer) +
                              (ps >= 0 ? std::string(" in ") + cpu_.stageName(ps) : std::string()) + ")", "30;43"));
    } else if (c.held) {
        // why did it not move last cycle?  (frames_[cycle-1] is the previous frame)
        bool ownStall = f.cycle >= 1 && f.cycle - 1 < frames_.size() && frames_[f.cycle - 1].cells[i].valid &&
                        frames_[f.cycle - 1].cells[i].seq == c.seq && frames_[f.cycle - 1].cells[i].stalled;
        parts.push_back(paint(ownStall ? "stayed here: it stalled last cycle (re-checking operands now)"
                                       : "blocked: an instruction ahead is stalled", "33"));
    }
    for (auto& fw : c.fwd) {
        int ps = stageOf(f, fw.second);
        parts.push_back(paint("FORWARD " + regName(fw.first) + " from #" + std::to_string(fw.second) +
                              (ps >= 0 ? std::string(" (") + cpu_.stageName(ps) + " result)" : std::string()), "30;46"));
    }
    if (c.taken) parts.push_back(paint("TAKEN -> PC=" + std::to_string(c.target) + ", younger instrs flushed", "97;45"));
    else if (i == stageIndex("EX") && isCondBranch(firstWord(c.text)) && !c.fault) parts.push_back("branch not taken (no penalty)");
    if (c.text == "<bad fetch>" && !c.flushed)
        parts.push_back(paint("fetched from a non-code address (faults if it reaches EX)", "33"));
    if (c.halt) parts.push_back(paint("HALT: fetch stopped", "1;91"));
    if (c.flushed) {
        bool fault = false, halt = false;
        for (auto& ev : f.events) { if (ev.kind == EvKind::Exception) fault = true; if (ev.kind == EvKind::Halt) halt = true; }
        parts.push_back(paint(std::string("FLUSHED (") + (fault ? "exception" : halt ? "after HALT" : "control hazard") + ")", "97;41"));
        if (c.text == "<bad fetch>") parts.push_back("wrong-path fetch, squashed before it could fault");
    }
    if (c.fault) parts.push_back(paint("EXCEPTION", "97;41"));
    if (c.retire) parts.push_back(paint("retires (commits)", "1;92"));
    std::string s;
    for (size_t k = 0; k < parts.size(); k++) s += (k ? "; " : "") + parts[k];
    return s;
}

std::string PipelineViz::table(const Frame& f) const {
    const Glyph& g = unicode_ ? UNI : ASC;
    std::ostringstream o;
    o << " " << padR("Stage", 5) << " " << g.v << " " << padR("Role", 34) << " " << g.v << " " << padR("Instruction", 24) << " " << g.v
      << " Status / movement\n";
    o << " " << rep(g.h, 6) << g.cross << rep(g.h, 36) << g.cross << rep(g.h, 26) << g.cross << rep(g.h, 40) << "\n";
    for (int i = 0; i < cpu_.stages(); i++) {
        const Cell& c = f.cells[i];
        std::string nm = cpu_.stageName(i);
        std::string ins = c.valid ? "#" + std::to_string(c.seq) + " [" + std::to_string(c.pc) + "] " + c.text : "-- bubble --";
        std::string insP = c.valid ? stageTag(nm, padR(clip(ins, 24), 24)) : paint(padR(ins, 24), "2;90");
        o << " " << paint(center(nm, 5), stageBg(nm)) << " " << g.v << " " << padR(stageRole(i), 34) << " " << g.v << " " << insP << " "
          << g.v << " " << statusText(f, i) << "\n";
    }
    return o.str();
}

std::string PipelineViz::eventList(const Frame& f) const {
    std::ostringstream o;
    int shown = 0;
    const Glyph& g = unicode_ ? UNI : ASC;
    for (const PipeEvent& ev : f.events) {
        std::string cons = label(ev.seq);
        switch (ev.kind) {
            case EvKind::Stall: {
                std::string pm = mnemonic(ev.other);
                std::string why = !fwd_ ? "forwarding is OFF: must wait until the producer writes back"
                                  : isLoadLike(pm) ? "load-use hazard: loaded value is only available after the memory stage"
                                                   : "producer result not ready yet";
                int ps = stageOf(f, ev.other);
                o << "  " << paint(" DATA HAZARD ", "30;43") << " RAW on " << maskNames(ev.regMask) << ": " << cons << " in "
                  << cpu_.stageName(ev.stage) << " needs a value from " << label(ev.other)
                  << (ps >= 0 ? std::string(" (now in ") + cpu_.stageName(ps) + ")" : std::string()) << "\n"
                  << "                " << "-> STALL: " << why << "; a bubble enters "
                  << (ev.stage + 1 < cpu_.stages() ? cpu_.stageName(ev.stage + 1) : "the next stage") << "\n";
                shown++; break;
            }
            case EvKind::Forward: {
                int ps = stageOf(f, ev.other);
                o << "  " << paint(" FORWARDING  ", "30;46") << " " << regName(ev.reg) << ": " << label(ev.other)
                  << (ps >= 0 ? std::string(" [") + cpu_.stageName(ps) + " result]" : std::string()) << " " << g.from << " bypassed to "
                  << label(ev.seq) << " in " << cpu_.stageName(ev.stage) << "  (stall avoided)\n";
                shown++; break;
            }
            case EvKind::Redirect:
                o << "  " << paint(" CTRL HAZARD ", "97;45") << " " << cons << " resolved in " << cpu_.stageName(ev.stage)
                  << ": taken, PC := " << ev.target << " (no branch prediction: wrong-path fetches are squashed)\n";
                shown++; break;
            case EvKind::Flush: {
                std::string l;
                for (uint64_t s : ev.seqs) {
                    int k = stageOf(f, s);
                    l += (l.empty() ? "" : ", ") + label(s) + (k >= 0 ? std::string(" (") + cpu_.stageName(k) + ")" : std::string());
                }
                o << "  " << paint(" PIPE FLUSH  ", "97;41") << " discarded " << ev.seqs.size() << " younger instr(s): " << l << "\n";
                shown++; break;
            }
            case EvKind::Halt:
                o << "  " << paint(" HALT        ", "1;91") << " " << cons << " reached " << cpu_.stageName(ev.stage)
                  << ": fetch stopped, pipeline drains\n";
                shown++; break;
            case EvKind::Exception:
                o << "  " << paint(" EXCEPTION   ", "97;41") << " " << cons << " in " << cpu_.stageName(ev.stage) << ": " << ev.text
                  << " (precise: older instrs finish, younger are discarded)\n";
                shown++; break;
            case EvKind::Retire: break;
        }
    }
    if (!shown) o << "  (no hazards this cycle)\n";
    return o.str();
}

std::string PipelineViz::counters(const Frame& f, const Frame* prev) const {
    std::ostringstream o;
    const Stats& s = f.stats;
    char b[160];
    double cpi = s.retired ? (double)s.cycles / (double)s.retired : 0.0;
    std::snprintf(b, sizeof b, " cycles=%llu  retired=%llu  CPI=%.2f  data-stall cycles=%llu  flushed=%llu  control bubbles=%llu  branches taken=%llu",
                  (unsigned long long)s.cycles, (unsigned long long)s.retired, cpi, (unsigned long long)s.dataStalls,
                  (unsigned long long)s.flushedInsts, (unsigned long long)s.controlBubbles, (unsigned long long)s.branchesTaken);
    o << b << "\n ";
    for (int r = 0; r < 8; r++) {
        std::snprintf(b, sizeof b, "%s=%04X", r == SP_REG ? "SP" : ("R" + std::to_string(r)).c_str(), f.regs[r]);
        bool ch = prev && prev->regs[r] != f.regs[r];
        o << (ch ? paint(std::string(b) + "*", "1;92") : std::string(b) + " ") << " ";
    }
    o << "  (* = written this cycle)\n";
    return o.str();
}

std::string PipelineViz::chartText(size_t idx) const {
    const Glyph& g = unicode_ ? UNI : ASC;
    const Frame& cur = frames_[idx];
    if (cur.cycle == 0) return "";
    uint64_t first = cur.cycle > (uint64_t)kChartCols ? cur.cycle - kChartCols + 1 : 1;
    std::map<uint64_t, std::map<uint64_t, std::string>> grid;     // seq -> cycle -> text
    for (uint64_t c = first > 1 ? first - 1 : 1; c <= cur.cycle; c++) {
        const Frame& fr = frames_[c];
        for (size_t i = 0; i < fr.cells.size(); i++) {
            const Cell& cl = fr.cells[i];
            if (!cl.valid) continue;
            if (c >= first) grid[cl.seq][c] = std::string(cpu_.stageName((int)i)) + (cl.stalled ? "*" : "");
            if (cl.flushed && c + 1 >= first && c + 1 <= cur.cycle) grid[cl.seq][c + 1] = "X";
        }
    }
    std::ostringstream o;
    o << " PIPELINE CHART  (cycles " << first << ".." << cur.cycle << ")   * = stalled in stage   X = flushed\n";
    o << " " << padR("instruction", 26);
    for (uint64_t c = first; c <= cur.cycle; c++) o << padR(std::to_string(c), 5);
    o << "\n " << rep(g.h, 26 + 5 * (int)(cur.cycle - first + 1)) << "\n";
    size_t skip = grid.size() > (size_t)kChartRows ? grid.size() - kChartRows : 0, n = 0;
    for (auto& kv : grid) {
        if (n++ < skip) continue;
        o << " " << padR(clip(label(kv.first), 25), 26);
        for (uint64_t c = first; c <= cur.cycle; c++) {
            auto it = kv.second.find(c);
            if (it == kv.second.end()) { o << std::string(5, ' '); continue; }
            const std::string& t = it->second;
            std::string base = t.substr(0, t.size() && t.back() == '*' ? t.size() - 1 : t.size());
            std::string cell = padR(t, 5);
            o << (t == "X" ? paint(cell, "97;41") : t.back() == '*' ? paint(cell, "30;43") : paint(cell, (std::string("1;") + stageFg(base)).c_str()));
        }
        o << "\n";
    }
    return o.str();
}

std::string PipelineViz::render(size_t idx) const {
    const Frame& f = frames_[idx];
    const Frame* prev = idx > 0 ? &frames_[idx - 1] : nullptr;
    std::ostringstream o;
    o << header(f);
    o << strip(f, prev) << "\n";
    o << table(f) << "\n";
    o << paint(" HAZARDS & EVENTS THIS CYCLE", "1") << "\n";
    if (f.cycle == 0) o << "  (nothing fetched yet -- type 'next' to clock the pipeline)\n";
    else o << eventList(f);
    o << "\n" << counters(f, prev);
    if (chart_ && f.cycle > 0) o << "\n" << chartText(idx);
    if (idx + 1 == frames_.size() && finished()) {
        o << "\n " << paint(f.faulted ? "PROGRAM STOPPED: EXCEPTION " + std::string(exName(cpu_.faultKind)) + " -- " + f.faultMsg
                                      : f.halted ? "PROGRAM FINISHED: HALT retired after " + std::to_string(f.cycle) + " cycles"
                                                 : "STOPPED: visualizer cycle cap (" + std::to_string(kMaxCycles) + ") reached",
                           f.halted ? "1;92" : "1;91")
          << "   (use 'prev' to review, 'reset' to restart, 'switch' to compare)\n";
    }
    return o.str();
}

// ======================================================================
//  commands
// ======================================================================
void PipelineViz::help() const {
    std::cout <<
"Pipeline visualizer commands\n"
"  next [n]             advance n clock cycles (default 1)\n"
"  prev [n]             go back n cycles (replays recorded history)\n"
"  run [ms] [cycles]    animate; ms = delay per cycle (default " << delayMs_ << "), cycles = stop after that many\n"
"                       (press ENTER or type 'pause' while it runs to pause)\n"
"  pause                pause the animation\n"
"  reset                restart the pipeline at cycle 0 (same program, same configuration)\n"
"  status               cycle, stage occupancy, hazard totals\n"
"  switch [yes]         swap 4-stage <-> 6-stage (restarts at cycle 0; needs 'yes' while instructions are in flight)\n"
"  exit                 back to the main RISC201 prompt\n"
"  extras:  fwd on|off (restarts)   chart on|off   regs   color on|off   unicode on|off   speed <ms>   help\n";
}

int PipelineViz::askMenu() {
    while (true) {
        std::cout << "\nSelect Pipeline Architecture:\n\n  1. 4-Stage Pipeline\n  2. 6-Stage Pipeline\n\n";
        std::string line;
        while (true) {
            if (interactive_) std::cout << "Enter choice [1/2] (or 'exit'): " << std::flush;
            if (!std::getline(in_, line)) return 0;
            if (!interactive_ && !line.empty() && line[0] != '#') std::cout << "choice> " << line << "\n";
            if (!line.empty() && line[0] == '#') continue;
            break;
        }
        std::string t = lower(split(line).empty() ? "" : split(line)[0]);
        if (t == "1" || t == "4") return 4;
        if (t == "2" || t == "6") return 6;
        if (t == "exit" || t == "q" || t == "quit" || t == "back") return 0;
        std::cout << "Invalid choice '" << line << "'. Please enter 1 or 2.\n";
    }
}

void PipelineViz::cmdNext(int n) {
    if (n < 1) n = 1;
    for (int k = 0; k < n; k++) {
        if (!forwardOne()) {
            if (k == 0) std::cout << "The program has already finished (cycle " << frames_.back().cycle
                                  << "). Use 'reset' to restart or 'prev' to review.\n";
            break;
        }
        if (k == n - 1 || (cur_ + 1 == frames_.size() && finished())) std::cout << render(cur_);
    }
}

void PipelineViz::cmdPrev(int n) {
    if (n < 1) n = 1;
    if (cur_ == 0) { std::cout << "Already at cycle 0 (start of the program).\n"; return; }
    cur_ = (size_t)n >= cur_ ? 0 : cur_ - n;
    std::cout << render(cur_);
}

bool PipelineViz::waitInput(int ms) {
    if (!interactive_) return false;
#ifdef _WIN32
    for (int waited = 0;; waited += 20) {          // poll the keyboard in 20 ms slices
        if (_kbhit()) return true;
        if (waited >= ms) return false;
        Sleep(20);
    }
#else
    struct pollfd p; p.fd = STDIN_FILENO; p.events = POLLIN; p.revents = 0;
    return poll(&p, 1, ms) > 0 && (p.revents & POLLIN);
#endif
}

void PipelineViz::cmdRun(const std::vector<std::string>& args) {
    int delay = interactive_ ? delayMs_ : 0, maxCycles = 1 << 30, v;
    if (args.size() > 0 && parseNum(args[0], v) && v >= 0) delay = v;
    if (args.size() > 1 && parseNum(args[1], v) && v > 0) maxCycles = v;
    if (atEnd() && finished()) {
        std::cout << "The program has already finished (cycle " << frames_.back().cycle << "). Use 'reset' to restart.\n";
        return;
    }
    const bool animate = interactive_ && isatty(STDOUT_FILENO);
    running_ = true;
    int n = 0;
    while (running_) {
        if (!forwardOne()) break;
        n++;
        if (animate) std::cout << "\033[2J\033[H";
        std::cout << render(cur_) << std::flush;
        if (atEnd() && finished()) break;
        if (n >= maxCycles) break;
        if (waitInput(delay)) {                          // user typed something: pause
            std::string l;
            if (std::getline(in_, l)) { std::string t = lower(l); if (!t.empty() && t != "pause" && t != "p") pending_ = l; }
            break;
        }
    }
    bool wasRunning = running_;
    running_ = false;
    if (wasRunning && !(atEnd() && finished())) std::cout << "-- paused at cycle " << frames_[cur_].cycle << " (type 'run' to resume) --\n";
}

void PipelineViz::cmdStatus() {
    const Frame& f = frames_[cur_];
    const Frame& last = frames_.back();
    uint64_t nfwd = 0;
    for (size_t k = 1; k <= cur_; k++) for (auto& ev : frames_[k].events) if (ev.kind == EvKind::Forward) nfwd++;
    std::cout << " PIPELINE STATUS\n";
    std::cout << "  configuration : " << cpu_.stages() << "-stage (";
    for (int i = 0; i < cpu_.stages(); i++) std::cout << (i ? " " : "") << cpu_.stageName(i);
    std::cout << "), forwarding " << (fwd_ ? "on" : "off") << ", " << (style_ == CtlStyle::Horizontal ? "horizontal" : "vertical") << " microcode\n";
    std::cout << "  program       : " << name_ << "\n";
    std::cout << "  state         : " << (running_ ? "RUNNING" : (atEnd() && finished()) ? (cpu_.faulted ? "FINISHED (exception)" : "FINISHED (halted)") : "PAUSED") << "\n";
    std::cout << "  clock cycle   : " << f.cycle << (cur_ + 1 == frames_.size() ? "  (latest)" : "  (viewing history; latest recorded = " + std::to_string(last.cycle) + ")") << "\n";
    std::cout << "  stage contents:\n";
    for (int i = 0; i < cpu_.stages(); i++) {
        const Cell& c = f.cells[i];
        std::string s = c.valid ? "#" + std::to_string(c.seq) + " [" + std::to_string(c.pc) + "] " + c.text : "-- bubble --";
        std::string note = c.valid ? (c.stalled ? "  (STALLED, RAW " + maskNames(c.stallMask) + ")" : c.flushed ? "  (FLUSHED)" : c.taken ? "  (branch TAKEN)" : "")
                                   : "  (" + bubbleText(f, i) + ")";
        std::cout << "    " << std::string(cpu_.stageName(i)) << std::string(4 - std::string(cpu_.stageName(i)).size(), ' ') << s << note << "\n";
    }
    std::cout << "  totals        : retired=" << f.stats.retired << "  data-stall cycles=" << f.stats.dataStalls << "  forwards=" << nfwd
              << "  flushed=" << f.stats.flushedInsts << "  control bubbles=" << f.stats.controlBubbles
              << "  branches taken=" << f.stats.branchesTaken << "\n";
}

void PipelineViz::cmdSwitch(const std::vector<std::string>& args) {
    bool confirm = !args.empty() && (lower(args[0]) == "yes" || lower(args[0]) == "y" || lower(args[0]) == "force");
    bool started = frames_.back().cycle > 0;
    bool safe = !started || (finished() && inFlight() == 0) || (cpu_.halted || cpu_.faulted);
    if (!safe && !confirm) {
        std::cout << "Not safe to switch yet: " << inFlight() << " instruction(s) are in flight in the " << cpu_.stages()
                  << "-stage pipeline and cannot be remapped onto a different set of stages.\n"
                     "Finish the program, 'reset', or type 'switch yes' to restart from cycle 0 in the "
                  << (stages_ == 4 ? 6 : 4) << "-stage pipeline.\n";
        return;
    }
    stages_ = (stages_ == 4) ? 6 : 4;
    configure();
    std::cout << "Switched to the " << stages_ << "-stage pipeline; restarted at cycle 0 (same program).\n";
    std::cout << render(0);
}

bool PipelineViz::command(const std::string& line) {
    std::vector<std::string> t = split(line);
    if (t.empty()) return true;
    std::string c = lower(t[0]);
    std::vector<std::string> a(t.begin() + 1, t.end());
    int v;
    if (c == "exit" || c == "back" || c == "quit" || c == "q") return false;
    else if (c == "next" || c == "n") cmdNext(!a.empty() && parseNum(a[0], v) ? v : 1);
    else if (c == "prev" || c == "p") cmdPrev(!a.empty() && parseNum(a[0], v) ? v : 1);
    else if (c == "run" || c == "r") cmdRun(a);
    else if (c == "pause") std::cout << (running_ ? "pausing...\n" : "The animation is not running (already paused). Use 'run' to animate or 'next' to step.\n");
    else if (c == "reset") { configure(); std::cout << "Pipeline restarted at cycle 0.\n" << render(0); }
    else if (c == "status") cmdStatus();
    else if (c == "switch") cmdSwitch(a);
    else if (c == "help" || c == "?") help();
    else if (c == "show") std::cout << render(cur_);
    else if (c == "chart") { chart_ = a.empty() ? !chart_ : lower(a[0]) == "on"; std::cout << "pipeline chart " << (chart_ ? "on" : "off") << "\n"; }
    else if (c == "color") { color_ = a.empty() ? !color_ : lower(a[0]) == "on"; std::cout << "colour " << (color_ ? "on" : "off") << "\n"; }
    else if (c == "unicode") { unicode_ = a.empty() ? !unicode_ : lower(a[0]) == "on";
#ifdef _WIN32
        if (unicode_) SetConsoleOutputCP(65001);   // switch the console to UTF-8 so the box glyphs display
#endif
        std::cout << "unicode box/arrow glyphs " << (unicode_ ? "on" : "off") << "\n"; }
    else if (c == "speed") {
        if (!a.empty() && parseNum(a[0], v) && v >= 0) { delayMs_ = v; std::cout << "animation delay " << v << " ms per cycle\n"; }
        else std::cout << "usage: speed <ms>   (currently " << delayMs_ << ")\n";
    }
    else if (c == "fwd") {
        if (a.empty()) std::cout << "forwarding is " << (fwd_ ? "on" : "off") << "\n";
        else { fwd_ = lower(a[0]) == "on"; configure(); std::cout << "forwarding " << (fwd_ ? "on" : "off") << "; pipeline restarted at cycle 0.\n" << render(0); }
    }
    else if (c == "regs") {
        const Frame& f = frames_[cur_];
        for (int r = 0; r < 8; r++) { char b[40]; std::snprintf(b, sizeof b, "  %-3s = 0x%04X (%6d)%s", regName(r).c_str(), f.regs[r], (int16_t)f.regs[r], r % 2 ? "\n" : "   "); std::cout << b; }
    }
    else std::cout << "unknown pipeline command '" << c << "' (type 'help')\n";
    return true;
}

void PipelineViz::session(int preselect) {
    int choice = preselect;
    if (choice != 4 && choice != 6) choice = askMenu();
    if (choice == 0) return;
    stages_ = choice;
    configure();
    std::cout << "\n" << stages_ << "-stage pipeline selected. Type 'help' for commands: next prev run pause reset status switch exit\n";
    std::cout << render(0);
    std::string line;
    while (true) {
        if (!pending_.empty()) { line = pending_; pending_.clear(); }
        else {
            if (interactive_) std::cout << "pipeline(" << stages_ << ")> " << std::flush;
            if (!std::getline(in_, line)) break;
            if (!line.empty() && line[0] == '#') continue;
            if (!interactive_ && !line.empty()) std::cout << "pipeline(" << stages_ << ")> " << line << "\n";
        }
        if (!command(line)) break;
    }
    std::cout << "Leaving pipeline visualizer (main CLI state unchanged).\n";
}
