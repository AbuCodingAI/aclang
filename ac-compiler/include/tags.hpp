#pragma once
#include <string>
#include <unordered_set>
#include <algorithm>

namespace Tags {

inline const std::unordered_set<std::string>& core() {
    static const std::unordered_set<std::string> s = {
        "mainloop",
        "gui",
        "OBJECT",
        "SCREEN",
        "LOGIC",
        "Local",
        "StartHere",
        "EndHere",
        "Foreign"  // internal literal for raw passthrough blocks
    };
    return s;
}

inline bool isKnownTag(const std::string& tag) {
    return core().count(tag) > 0;
}

inline bool isMainLoop(const std::string& tag) {
    return tag == "mainloop";
}

inline bool isGUIBox(const std::string& tag) {
    return tag == "gui" || tag == "OBJECT" || tag == "SCREEN";
}

inline bool isLogicScope(const std::string& tag) {
    return tag == "LOGIC" || tag == "Local";
}

inline bool isStartHere(const std::string& tag) {
    return tag == "StartHere";
}

inline bool isEndHere(const std::string& tag) {
    return tag == "EndHere";
}

inline bool isSpawnable(const std::string& tag) {
    return !isMainLoop(tag) && !isGUIBox(tag) && !isLogicScope(tag) && !isStartHere(tag) && !isEndHere(tag);
}

inline std::string toLower(std::string tag) {
    std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
    return tag;
}

} // namespace Tags
