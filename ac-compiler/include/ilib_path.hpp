#pragma once
#include <string>

// Maps an ilib's import name to its directory under library/ilib/.
// web-server is a sub-library of web (`from ilib web use web-server`), so its sources
// live at library/ilib/web/server/ rather than a top-level library/ilib/web-server/.
inline std::string ilibSubdir(const std::string& name) {
    if (name == "web-server") return "web/server";
    return name;
}
