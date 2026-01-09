#ifndef EVALUATE_H
#define EVALUATE_H

#include "position.h"
#include <string>

namespace Eval {

// Initialize static tables (PSQT, masks, etc.)
void init();

// Main evaluation function
int evaluate(const Position& pos);

// Debug/Tracing
std::string trace(const Position& pos);

} // namespace Eval

#endif // EVALUATE_H