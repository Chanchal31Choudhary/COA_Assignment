// =====================================================================
//  pipeviz.h  --  Interactive pipeline visualizer (CLI sub-mode "pipeline")
//  OWNER : Rani (Member C)  /  uses Kavya's CPU pipeline model
//  ROLE  : Steps the REAL cycle-accurate CPU (pipeline.cpp) one clock at a
//          time and draws what each stage holds, how instructions move,
//          bubbles, RAW data hazards, forwarding, control hazards and flushes.
//
//          It owns a private CPU instance, so the main CLI machine state
//          (registers, breakpoints, stats ...) is never touched.
//          Every displayed cycle is recorded as a Frame, which is how `prev`
//          works without making the simulator reversible.
//
//   Stage names are NOT defined here: they come from CPU::stageName()
//      4-stage : IF  ID  EX  WB
//      6-stage : IF  ID  RF  EX  MEM  WB
//
//   Commands (inside the sub-mode):
//      next [n]  prev [n]  run [delay_ms] [max_cycles]  pause  reset
//      status  switch [yes]  exit          (+ help, chart, color, unicode,
//                                            speed, fwd, regs)
// =====================================================================
#pragma once
#include <iosfwd>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "pipeline.h"

class PipelineViz {
public:
    PipelineViz(const Program& prog, const std::string& progName, CtlStyle style, bool forwarding,
                std::istream& in, bool interactive);

    // Shows the architecture menu (unless preselect is 4 or 6) and runs the
    // sub-mode command loop.  Returns when the user types `exit`.
    void session(int preselect = 0);

private:
    enum class Bubble { None, Fill, Stall, Flush, Drain, Idle };

    struct Cell {                              // one pipeline stage during one cycle
        bool valid = false;
        uint64_t seq = 0;
        uint16_t pc = 0;
        std::string text;
        int from = -1;                         // stage it came from (-1 = fetched this cycle)
        bool entered = false;                  // moved here at the start of this cycle
        bool held = false;                     // was already in this stage last cycle
        bool stalled = false;                  // own RAW stall
        bool flushed = false;                  // discarded this cycle
        bool taken = false;                    // branch/jump/CALL/RET redirected the PC
        uint16_t target = 0;
        bool halt = false, retire = false, fault = false;
        std::string faultText;
        uint64_t stallProducer = 0;
        unsigned stallMask = 0;
        std::vector<std::pair<int, uint64_t>> fwd;   // (register, producer seq)
        Bubble bubble = Bubble::None;          // why the stage is empty
    };

    struct Frame {                             // everything shown for one clock cycle
        uint64_t cycle = 0;                    // 0 = initial (nothing fetched yet)
        std::vector<Cell> cells;
        std::vector<PipeEvent> events;
        Stats stats;
        uint16_t regs[8] = {0};
        uint16_t pc = 0;
        bool halted = false, faulted = false;
        std::string faultMsg;
    };

    // ---- state ----
    Program prog_;
    std::string name_;
    CtlStyle style_;
    bool fwd_;
    std::istream& in_;
    bool interactive_;
    CPU cpu_;
    int stages_ = 4;
    std::vector<Frame> frames_;
    size_t cur_ = 0;                           // frame being displayed
    std::map<uint64_t, std::pair<uint16_t, std::string>> info_;   // seq -> (pc, disassembly)
    bool running_ = false;
    bool color_ = false, unicode_ = false, chart_ = true;
    int delayMs_ = 500;
    std::string pending_;                      // command typed while `run` was animating

    // ---- machine / history ----
    void configure();
    Frame initialFrame() const;
    void stepLive();
    bool forwardOne();                         // false => at the end of a finished program
    bool finished() const;
    bool atEnd() const { return cur_ + 1 == frames_.size(); }
    int inFlight() const;
    int stageIndex(const std::string& name) const;

    // ---- commands ----
    int askMenu();
    bool command(const std::string& line);
    void cmdNext(int n);
    void cmdPrev(int n);
    void cmdRun(const std::vector<std::string>& args);
    void cmdStatus();
    void cmdSwitch(const std::vector<std::string>& args);
    void help() const;
    bool waitInput(int ms);

    // ---- drawing ----
    std::string render(size_t idx) const;
    std::string header(const Frame& f) const;
    std::string strip(const Frame& f, const Frame* prev) const;
    std::string table(const Frame& f) const;
    std::string eventList(const Frame& f) const;
    std::string counters(const Frame& f, const Frame* prev) const;
    std::string chartText(size_t idx) const;
    std::string stageRole(int i) const;
    std::string statusText(const Frame& f, int i) const;
    std::string bubbleText(const Frame& f, int i) const;
    std::string label(uint64_t seq) const;
    std::string mnemonic(uint64_t seq) const;
    std::string paint(const std::string& s, const char* code) const;
    std::string stageTag(const std::string& name, const std::string& s) const;   // coloured stage text
    const char* stageBg(const std::string& name) const;
    const char* stageFg(const std::string& name) const;
    int stageOf(const Frame& f, uint64_t seq) const;
    std::string rule(char c = '=') const;
};
