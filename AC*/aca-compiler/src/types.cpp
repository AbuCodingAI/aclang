#include "types.hpp"
#include <cctype>
#include <stdexcept>

namespace aca {

DecMap decimalLiteralToMap(const std::string& lit) {
    std::string s = lit;
    bool neg = false;
    if (!s.empty() && s[0] == '-') {
        neg = true;
        s = s.substr(1);
    }
    auto dot = s.find('.');
    if (dot == std::string::npos) throw std::runtime_error("Not a decimal literal: " + lit);
    std::string intPart = s.substr(0, dot);
    std::string fracPart = s.substr(dot + 1);
    if (intPart.empty()) intPart = "0";
    if (fracPart.empty()) fracPart = "0";

    DecMap m;
    for (size_t k = 0; k < intPart.size(); k++) {
        char c = intPart[k];
        if (!std::isdigit((unsigned char)c)) throw std::runtime_error("Bad decimal literal: " + lit);
        int exp = (int)intPart.size() - 1 - (int)k;
        int digit = c - '0';
        if (digit != 0) m[exp] = digit;
    }
    for (size_t k = 0; k < fracPart.size(); k++) {
        char c = fracPart[k];
        if (!std::isdigit((unsigned char)c)) throw std::runtime_error("Bad decimal literal: " + lit);
        int exp = -1 - (int)k;
        int digit = c - '0';
        if (digit != 0) m[exp] = digit;
    }

    if (neg) {
        if (!m.empty()) {
            auto it = m.begin();
            it->second = -it->second;
        } else {
            m[0] = 0;
        }
    }
    return m;
}

} // namespace aca

