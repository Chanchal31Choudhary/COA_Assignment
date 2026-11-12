// =====================================================================
//  stackviz.h  --  ASCII stack-memory frame renderer
//  OWNER : Rani (Member C)
//  ROLE  : Draws the full-descending stack (high addresses on top, stack
//          grows downwards), marks SP, live words, free words and the guard.
// =====================================================================
#pragma once
#include <string>
#include "memory.h"

std::string renderStack(const Memory& mem, uint16_t sp, int maxRows = 16);
