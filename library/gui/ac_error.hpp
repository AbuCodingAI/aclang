#pragma once
#include <stdexcept>
#include <string>

// raise ERR($message$) — throws a runtime error displayed as "Preposterous: <msg>"
inline void ac_raise_err(const std::string& msg) {
    throw std::runtime_error("Preposterous: " + msg);
}

#define raise_ERR(msg) ac_raise_err(msg)
