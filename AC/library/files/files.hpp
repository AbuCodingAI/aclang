// AC ilib: files — File I/O library
// use ilib files
#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

namespace ac_files {

// Read entire file into a string. Returns empty string if file not found.
inline std::string read(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

// Read file as lines (strips trailing newlines per line).
inline std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream f(path);
    if (!f.is_open()) return lines;
    std::string line;
    while (std::getline(f, line)) lines.push_back(line);
    return lines;
}

// Write string to file (overwrites). Returns true on success.
inline bool write(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return f.good();
}

// Append string to file. Returns true on success.
inline bool append(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::app);
    if (!f.is_open()) return false;
    f << content;
    return f.good();
}

// Check if file exists.
inline bool exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// Delete a file. Returns true on success.
inline bool remove(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

} // namespace ac_files

// Flat API (matches AC ilib call style)
inline std::string  files_read(const std::string& p)               { return ac_files::read(p); }
inline bool         files_write(const std::string& p, const std::string& c) { return ac_files::write(p, c); }
inline bool         files_append(const std::string& p, const std::string& c){ return ac_files::append(p, c); }
inline bool         files_exists(const std::string& p)             { return ac_files::exists(p); }
inline bool         files_remove(const std::string& p)             { return ac_files::remove(p); }
