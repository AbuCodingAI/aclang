// Master include for the AC compiler — include this instead of individual headers.
// All compiler source files can use: #include "../include/ac.hpp"

#pragma once

#include "token.hpp"
#include "ast.hpp"
#include "error.hpp"
#include "type.hpp"
#include "ir.hpp"
#include "tags.hpp"
#include "backend_registry.hpp"
#include "exp_bny.hpp"
