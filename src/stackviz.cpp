// stackviz.cpp -- OWNER: Rani (Member C)
#include "stackviz.h"
#include <cstdio>
#include <sstream>

using namespace isa;

std::string renderStack(const Memory& mem, uint16_t sp, int maxRows) {
    std::ostringstream o;
    char b[160];
    int live = (int)STACK_TOP - (int)sp;
    int cap = (int)STACK_TOP - (int)STACK_LIMIT;
    std::snprintf(b, sizeof b, " STACK (full-descending)   SP=%d (0x%02X)   depth=%d/%d words   peak=%d\n",
                  sp, sp, live, cap, (int)STACK_TOP - (int)mem.stackLow());
    o << b;
    o << " +--------+--------+---------+------------------------------+\n";
    o << " |  addr  |  hex   |   dec   |  status                      |\n";
    o << " +--------+--------+---------+------------------------------+\n";

    auto row = [&](int a, const char* status) {
        uint16_t v = mem.peek(a);
        std::snprintf(b, sizeof b, " |  %3d   |  %04X  | %6d  | %-28s |\n", a, v, (int16_t)v, status);
        o << b;
    };
    if (live <= 0) {
        o << " |        (stack is empty)                                   |\n";
    } else {
        int top = STACK_TOP - 1;                       // stack base (oldest word)
        int show = live <= maxRows ? live : maxRows;
        int headRows = live <= maxRows ? live : 3;     // rows near the base when collapsing
        for (int i = 0; i < live; i++) {
            int a = top - i;
            bool inHead = i < headRows;
            bool inTail = i >= live - (show - headRows);
            if (!inHead && !inTail) {
                if (i == headRows) {
                    std::snprintf(b, sizeof b, " |  ...    ...      ...   | %d words hidden %-11s |\n", live - show, "");
                    o << b;
                }
                continue;
            }
            std::string st = "##### live";
            if (a == top) st = "##### live  (base)";
            if (a == sp) st = "##### live  <== SP (top)";
            row(a, st.c_str());
        }
    }
    int freeShown = 0;
    for (int a = (int)sp - 1; a >= (int)STACK_LIMIT && freeShown < 2; a--, freeShown++) row(a, ".....  free");
    if ((int)sp - 1 - 2 >= (int)STACK_LIMIT) o << " |  ...    ...      ...   | (more free space)            |\n";
    std::snprintf(b, sizeof b, " +==== GUARD: STACK_LIMIT = %d (push below this -> StackOverflow) ====+\n", (int)STACK_LIMIT);
    o << b;
    return o.str();
}
