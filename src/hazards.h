// Static hazard analysis for assembled RISC201 programs.
#pragma once
#include <string>
#include "assembler.h"

// Returns a read-only report for one pipeline configuration.
std::string analyzeHazards(const Program& program, int stages);
std::string analyzeHazards(const Program& program, int stages, bool forwarding);

// Convenience report covering 4/6 stages with forwarding on/off.
std::string analyzeAllHazards(const Program& program);
