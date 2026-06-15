#pragma once
#include "ast.hpp"
#include <string>

namespace aca {

// Lowers AC* AST into SAPL text.
// Always emits a valid SAPL program (even if some constructs lower to comments/placeholders).
std::string lowerToSapl(const Program& prog);

} // namespace aca

