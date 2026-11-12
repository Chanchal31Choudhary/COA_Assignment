#include "assembler.h"
#include "disassembler.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static bool sameImage(const Program& a, const Program& b) {
    for (int i = 0; i < isa::MEM_WORDS; ++i) {
        if (a.used[i] != b.used[i] || a.isCode[i] != b.isCode[i]) return false;
        if (a.used[i] && a.mem[i] != b.mem[i]) return false;
    }
    return true;
}

int main(int argc, char** argv) {
    int passed = 0, failed = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string path = argv[i];
        std::ifstream input(path);
        if (!input) {
            std::cerr << "FAIL: cannot read " << path << '\n';
            ++failed;
            continue;
        }
        std::ostringstream source;
        source << input.rdbuf();
        Program original = assemble(source.str());
        if (!original.ok()) {
            // Syntax-error fixtures are checked by the CPU regression suite.
            if (path.find("e10_syntax_errors") != std::string::npos) {
                ++passed;
                continue;
            }
            std::cerr << "FAIL: unexpected assembly error in " << path << '\n';
            for (const auto& e : original.errors) std::cerr << "  " << e << '\n';
            ++failed;
            continue;
        }
        Program roundTrip = assemble(disassembleProgram(original, true));
        if (!roundTrip.ok() || !sameImage(original, roundTrip)) {
            std::cerr << "FAIL: assemble/disassemble round-trip: " << path << '\n';
            ++failed;
            continue;
        }
        ++passed;
    }
    std::cout << "Toolchain round-trip tests: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
