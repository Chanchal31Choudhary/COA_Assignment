// preprocessor.cpp -- OWNER: Jiya (Member A)
#include "preprocessor.h"
#include <cctype>
#include <map>
#include <sstream>

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
static std::string upper(std::string s) { for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; }

static std::string stripComment(const std::string& s) {
    size_t p = s.find(';');
    size_t q = s.find("//");
    size_t e = std::min(p, q);
    return (e == std::string::npos) ? s : s.substr(0, e);
}

// replace whole-word occurrences of #define'd names
static std::string substitute(const std::string& line, const std::map<std::string, std::string>& defs) {
    if (defs.empty()) return line;
    std::string out;
    size_t i = 0;
    while (i < line.size()) {
        if (std::isalpha((unsigned char)line[i]) || line[i] == '_') {
            size_t j = i;
            while (j < line.size() && (std::isalnum((unsigned char)line[j]) || line[j] == '_')) j++;
            std::string w = line.substr(i, j - i);
            auto it = defs.find(w);
            out += (it != defs.end()) ? it->second : w;
            i = j;
        } else out += line[i++];
    }
    return out;
}

PPResult preprocess(const std::string& source) {
    PPResult res;
    std::map<std::string, std::string> defs;
    std::istringstream in(source);
    std::string raw;
    int ln = 0;
    auto emit = [&](const std::string& t, int line, const std::string& from) {
        SrcLine s; s.text = t; s.line = line; s.from = from; res.lines.push_back(s);
    };
    while (std::getline(in, raw)) {
        ln++;
        std::string line = trim(stripComment(raw));
        if (line.empty()) continue;

        // #define NAME value
        if (line[0] == '#') {
            std::istringstream ls(line.substr(1));
            std::string kw, name;
            ls >> kw >> name;
            if (upper(kw) == "DEFINE" && !name.empty()) {
                std::string val;
                std::getline(ls, val);
                defs[name] = trim(val);
            } else res.errors.push_back("line " + std::to_string(ln) + ": unknown preprocessor directive '" + line + "'");
            continue;
        }

        line = substitute(line, defs);

        // split off a leading label so macros can follow it
        std::string label;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            label = trim(line.substr(0, colon + 1));
            line = trim(line.substr(colon + 1));
            emit(label, ln, "");
            if (line.empty()) continue;
        }

        std::istringstream ls(line);
        std::string mn;
        ls >> mn;
        std::string rest;
        std::getline(ls, rest);
        rest = trim(rest);
        std::string M = upper(mn);

        if (M == "PUSHM" || M == "POPM") {
            std::string r = upper(rest);
            if (r.empty() || r == "SP" || r == "R7") {
                res.errors.push_back("line " + std::to_string(ln) + ": " + M + " needs a register R0..R6");
                continue;
            }
            std::string tag = M + " " + r;
            if (M == "PUSHM") { emit("ADDI SP, -1", ln, tag); emit("STR " + r + ", [SP]", ln, tag); }
            else              { emit("LDR " + r + ", [SP]", ln, tag); emit("ADDI SP, 1", ln, tag); }
        } else if (M == "PUSHALL") {
            for (int i = 0; i <= 6; i++) {
                std::string r = "R" + std::to_string(i);
                emit("ADDI SP, -1", ln, "PUSHALL"); emit("STR " + r + ", [SP]", ln, "PUSHALL");
            }
        } else if (M == "POPALL") {
            for (int i = 6; i >= 0; i--) {
                std::string r = "R" + std::to_string(i);
                emit("LDR " + r + ", [SP]", ln, "POPALL"); emit("ADDI SP, 1", ln, "POPALL");
            }
        } else if (M == "ALLOC" || M == "FREE") {
            if (rest.empty()) { res.errors.push_back("line " + std::to_string(ln) + ": " + M + " needs a size"); continue; }
            emit(M == "ALLOC" ? "ADDI SP, 0-" + rest : "ADDI SP, " + rest, ln, M + " " + rest);
        } else {
            emit(line, ln, "");
        }
    }
    return res;
}

std::string renderExpanded(const PPResult& r) {
    std::ostringstream o;
    for (auto& l : r.lines) {
        o << "  " << l.text;
        if (!l.from.empty()) o << "        ; <- macro " << l.from;
        o << "\n";
    }
    for (auto& e : r.errors) o << "ERROR: " << e << "\n";
    return o.str();
}
