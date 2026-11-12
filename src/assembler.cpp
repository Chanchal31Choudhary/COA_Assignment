// assembler.cpp -- OWNER: Jiya (Member A)
#include "assembler.h"
#include "preprocessor.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace isa;

Program::Program() {
    for (int i = 0; i < MEM_WORDS; i++) { mem[i] = 0; isCode[i] = false; used[i] = false; }
}

// ------------------------------------------------------------------ helpers
static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
static std::string upper(std::string s) { for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; }

static bool parseNumber(const std::string& t, int& v) {
    if (t.empty()) return false;
    char* end = nullptr;
    long x;
    if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) x = std::strtol(t.c_str() + 2, &end, 16);
    else if (t.size() > 2 && t[0] == '0' && (t[1] == 'b' || t[1] == 'B')) x = std::strtol(t.c_str() + 2, &end, 2);
    else if (std::isdigit((unsigned char)t[0])) x = std::strtol(t.c_str(), &end, 10);
    else return false;
    if (*end != '\0') return false;
    v = (int)x;
    return true;
}

static bool evalExpr(const std::string& s0, const std::map<std::string, int>& sym, int& out, std::string& err) {
    std::string s = trim(s0);
    if (s.empty()) { err = "missing value"; return false; }
    int total = 0, sign = 1;
    bool first = true;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            sign = (s[i] == '-') ? -1 : 1;
            i++;
        } else if (!first) { err = "expected + or - in expression '" + s + "'"; return false; }
        while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
        size_t j = i;
        while (j < s.size() && s[j] != '+' && s[j] != '-') j++;
        std::string term = trim(s.substr(i, j - i));
        if (term.empty()) { err = "bad expression '" + s + "'"; return false; }
        int v;
        if (!parseNumber(term, v)) {
            auto it = sym.find(term);
            if (it == sym.end()) { err = "undefined symbol '" + term + "'"; return false; }
            v = it->second;
        }
        total += sign * v;
        sign = 1;
        first = false;
        i = j;
    }
    out = total;
    return true;
}

static int parseReg(const std::string& t0) {
    std::string t = upper(trim(t0));
    if (t == "SP") return SP_REG;
    if (t.size() == 2 && t[0] == 'R' && t[1] >= '0' && t[1] <= '7') return t[1] - '0';
    return -1;
}

static std::string stripBrackets(const std::string& s0) {
    std::string s = trim(s0);
    if (s.size() >= 2 && s.front() == '[' && s.back() == ']') return trim(s.substr(1, s.size() - 2));
    return s;
}

struct Parsed {
    std::string mn;                 // uppercase mnemonic / directive ("" if label-only)
    std::vector<std::string> ops;
    std::string label;
    std::string text;
    int line = 0;
    std::string from;
};

static Parsed parseLine(const SrcLine& sl) {
    Parsed p;
    p.line = sl.line;
    p.from = sl.from;
    p.text = sl.text;
    std::string s = trim(sl.text);
    size_t colon = s.find(':');
    if (colon != std::string::npos) {
        std::string lab = trim(s.substr(0, colon));
        bool ident = !lab.empty();
        for (char c : lab) if (!(std::isalnum((unsigned char)c) || c == '_')) ident = false;
        if (ident) { p.label = lab; s = trim(s.substr(colon + 1)); }
    }
    if (s.empty()) return p;
    size_t sp = 0;
    while (sp < s.size() && !std::isspace((unsigned char)s[sp])) sp++;
    p.mn = upper(s.substr(0, sp));
    std::string rest = trim(s.substr(sp));
    if (!rest.empty()) {
        std::string cur;
        for (char c : rest) {
            if (c == ',') { p.ops.push_back(trim(cur)); cur.clear(); }
            else cur += c;
        }
        p.ops.push_back(trim(cur));
    }
    return p;
}

static int findOpcode(const std::string& mn) {
    for (int i = 0; i < OPCODE_COUNT; i++) if (mn == OP_TABLE[i].name) return i;
    return -1;
}

// ------------------------------------------------------------------ assembler
Program assemble(const std::string& source) {
    Program prog;
    PPResult pp = preprocess(source);
    for (auto& e : pp.errors) prog.errors.push_back(e);

    std::vector<Parsed> lines;
    for (auto& sl : pp.lines) lines.push_back(parseLine(sl));

    auto err = [&](int ln, const std::string& m) { prog.errors.push_back("line " + std::to_string(ln) + ": " + m); };

    // ---------------- PASS 1 : symbol table + address assignment ----------------
    int addr = 0;
    for (auto& p : lines) {
        if (!p.label.empty()) {
            if (prog.symbols.count(p.label)) err(p.line, "duplicate label '" + p.label + "'");
            else prog.symbols[p.label] = addr;
        }
        if (p.mn.empty()) continue;
        std::string e;
        if (p.mn[0] == '.') {
            if (p.mn == ".EQU") {
                std::string name, val;
                if (p.ops.size() == 2) { name = p.ops[0]; val = p.ops[1]; }
                else if (p.ops.size() == 1) {
                    std::istringstream is(p.ops[0]);
                    is >> name;
                    std::getline(is, val);
                }
                int v;
                if (name.empty() || !evalExpr(val, prog.symbols, v, e)) err(p.line, ".equ needs NAME, value" + (e.empty() ? "" : " (" + e + ")"));
                else if (prog.symbols.count(name)) err(p.line, "duplicate symbol '" + name + "'");
                else prog.symbols[name] = v;
            } else if (p.mn == ".ORG") {
                int v;
                if (p.ops.size() != 1 || !evalExpr(p.ops[0], prog.symbols, v, e)) err(p.line, ".org needs an address" + (e.empty() ? "" : " (" + e + ")"));
                else if (v < 0 || v >= MEM_WORDS) err(p.line, ".org address out of range");
                else addr = v;
            } else if (p.mn == ".WORD") addr += (int)p.ops.size();
            else if (p.mn == ".SPACE") {
                int v;
                if (p.ops.size() != 1 || !evalExpr(p.ops[0], prog.symbols, v, e) || v < 0) err(p.line, ".space needs a size");
                else addr += v;
            } else err(p.line, "unknown directive '" + p.mn + "'");
        } else {
            if (findOpcode(p.mn) < 0) err(p.line, "unknown mnemonic '" + p.mn + "'");
            addr++;
        }
        if (addr > MEM_WORDS) { err(p.line, "program does not fit in " + std::to_string(MEM_WORDS) + " words"); break; }
    }
    // ---------------- PASS 2 : encode ----------------
    addr = 0;
    auto place = [&](const Parsed& p, uint16_t w, bool code) {
        if (addr < 0 || addr >= MEM_WORDS) { err(p.line, "address out of range"); return; }
        if (prog.used[addr]) err(p.line, "address " + std::to_string(addr) + " already used (overlap)");
        prog.mem[addr] = w;
        prog.used[addr] = true;
        prog.isCode[addr] = code;
        if (addr > prog.lastAddr) prog.lastAddr = addr;
        ListLine l; l.addr = addr; l.word = w; l.isCode = code; l.src = p.text; l.line = p.line; l.from = p.from;
        prog.listing.push_back(l);
        addr++;
    };

    for (auto& p : lines) {
        if (p.mn.empty()) continue;
        std::string e;
        if (p.mn[0] == '.') {
            if (p.mn == ".ORG") { int v; if (p.ops.size() == 1 && evalExpr(p.ops[0], prog.symbols, v, e) && v >= 0 && v < MEM_WORDS) addr = v; }
            else if (p.mn == ".WORD") {
                for (auto& o : p.ops) {
                    int v;
                    if (!evalExpr(o, prog.symbols, v, e)) { err(p.line, e); continue; }
                    if (v < -32768 || v > 65535) err(p.line, ".word value out of 16-bit range");
                    place(p, (uint16_t)v, false);
                }
            } else if (p.mn == ".SPACE") {
                int v;
                if (p.ops.size() == 1 && evalExpr(p.ops[0], prog.symbols, v, e))
                    for (int i = 0; i < v && addr < MEM_WORDS; i++) place(p, 0, false);
            }
            continue;
        }
        int op = findOpcode(p.mn);
        if (op < 0) continue;                 // already reported in pass 1
        const OpInfo& oi = OP_TABLE[op];
        auto need = [&](size_t n) {
            if (p.ops.size() != n) { err(p.line, p.mn + " expects " + std::to_string(n) + " operand(s)"); return false; }
            return true;
        };
        auto reg = [&](const std::string& s, int& r) {
            r = parseReg(s);
            if (r < 0) { err(p.line, "bad register '" + s + "' (use R0..R7 or SP)"); return false; }
            return true;
        };
        int rd = 0, rs1 = 0, rs2 = 0, imm = 0;
        uint16_t w = 0;
        bool good = true;
        switch (oi.fmt) {
            case F_NONE: good = need(0); w = encR(op, 0, 0, 0); break;
            case F_R3:   good = need(3) && reg(p.ops[0], rd) && reg(p.ops[1], rs1) && reg(p.ops[2], rs2); if (good) w = encR(op, rd, rs1, rs2); break;
            case F_R2:   good = need(2) && reg(p.ops[0], rd) && reg(p.ops[1], rs1); if (good) w = encR(op, rd, rs1, 0); break;
            case F_MEM:  good = need(2) && reg(p.ops[0], rd) && reg(stripBrackets(p.ops[1]), rs1); if (good) w = encR(op, rd, rs1, 0); break;
            case F_R1:   good = need(1) && reg(p.ops[0], rd); if (good) w = encR(op, rd, 0, 0); break;
            case F_RIMM:
                good = need(2) && reg(p.ops[0], rd);
                if (good) {
                    if (!evalExpr(p.ops[1], prog.symbols, imm, e)) { err(p.line, e); good = false; }
                    else if (imm < -128 || imm > 127) { err(p.line, "immediate " + std::to_string(imm) + " out of range -128..127"); good = false; }
                    else w = encI(op, rd, imm);
                }
                break;
            case F_RADDR: case F_RBR:
                good = need(2) && reg(p.ops[0], rd);
                if (good) {
                    if (!evalExpr(stripBrackets(p.ops[1]), prog.symbols, imm, e)) { err(p.line, e); good = false; }
                    else if (imm < 0 || imm >= MEM_WORDS) { err(p.line, "address " + std::to_string(imm) + " out of range 0..255"); good = false; }
                    else w = encI(op, rd, imm);
                }
                break;
            case F_TGT:
                good = need(1);
                if (good) {
                    if (!evalExpr(p.ops[0], prog.symbols, imm, e)) { err(p.line, e); good = false; }
                    else if (imm < 0 || imm >= MEM_WORDS) { err(p.line, "target " + std::to_string(imm) + " out of range 0..255"); good = false; }
                    else w = encI(op, 0, imm);
                }
                break;
        }
        if (good) place(p, w, true);
    }
    // report errors in source-line order
    std::stable_sort(prog.errors.begin(), prog.errors.end(), [](const std::string& a, const std::string& b) {
        auto num = [](const std::string& x) { return x.compare(0, 5, "line ") == 0 ? std::atoi(x.c_str() + 5) : 0; };
        return num(a) < num(b);
    });
    return prog;
}

// ------------------------------------------------------------------ output
std::string toHex(const Program& p) {
    std::string s;
    char b[32];
    for (int a = 0; a <= p.lastAddr; a++) {
        std::snprintf(b, sizeof b, "%04X", p.mem[a]);
        s += b;
        if (!p.isCode[a]) s += " ;D";
        s += "\n";
    }
    return s;
}

std::string toBin(const Program& p) {
    std::string s;
    for (int a = 0; a <= p.lastAddr; a++) {
        for (int i = 15; i >= 0; i--) s += ((p.mem[a] >> i) & 1) ? '1' : '0';
        if (!p.isCode[a]) s += " ;D";
        s += "\n";
    }
    return s;
}

std::string toListing(const Program& p) {
    std::ostringstream o;
    o << "ADDR  WORD  BINARY            SOURCE\n";
    char b[128];
    for (auto& l : p.listing) {
        std::string bin;
        for (int i = 15; i >= 0; i--) bin += ((l.word >> i) & 1) ? '1' : '0';
        std::snprintf(b, sizeof b, "%04d  %04X  %s  ", l.addr, l.word, bin.c_str());
        o << b << l.src;
        if (!l.from.empty()) o << "    ; macro " << l.from;
        o << "\n";
    }
    o << "\nSYMBOL TABLE\n";
    for (auto& kv : p.symbols) o << "  " << kv.first << " = " << kv.second << "\n";
    return o.str();
}

Program parseImage(const std::string& text, std::vector<std::string>& errors) {
    Program p;
    std::istringstream in(text);
    std::string raw;
    int ln = 0, addr = 0;
    while (std::getline(in, raw)) {
        ln++;
        bool data = false;
        size_t sc = raw.find(';');
        if (sc != std::string::npos) {
            std::string c = trim(raw.substr(sc + 1));
            data = !c.empty() && (c[0] == 'D' || c[0] == 'd');
            raw = raw.substr(0, sc);
        }
        std::string t = trim(raw);
        if (t.empty()) continue;
        bool bin = (t.size() == 16);
        for (char c : t) if (c != '0' && c != '1') bin = false;
        char* end = nullptr;
        unsigned long v = std::strtoul(t.c_str(), &end, bin ? 2 : 16);
        if (*end != '\0' || t.size() > 16 || (!bin && t.size() > 4)) {
            errors.push_back("line " + std::to_string(ln) + ": bad word '" + t + "'");
            continue;
        }
        if (addr >= MEM_WORDS) { errors.push_back("image larger than memory"); break; }
        p.mem[addr] = (uint16_t)v;
        p.used[addr] = true;
        p.isCode[addr] = !data;
        p.lastAddr = addr;
        addr++;
    }
    return p;
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    std::ofstream f(path);
    if (!f) return false;
    f << data;
    return true;
}
