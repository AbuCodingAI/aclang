#include "cache.hpp"
#include <fstream>

namespace aca {

static const char IRC_MAGIC[4] = {'A', 'C', 'I', 'R'};
static const uint8_t IRC_VERSION = 1;

uint64_t fnv64(const std::string& data) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 0x00000100000001B3ULL;
    }
    return h;
}

std::optional<CacheEntry> loadIrc(const std::string& path, uint64_t expectedHash) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;

    char magic[4];
    f.read(magic, 4);
    if (!f || std::string(magic, 4) != std::string(IRC_MAGIC, 4)) return std::nullopt;

    uint8_t ver = 0;
    f.read((char*)&ver, 1);
    if (!f || ver != IRC_VERSION) return std::nullopt;

    uint64_t hash = 0;
    f.read((char*)&hash, 8);
    if (!f || hash != expectedHash) return std::nullopt;

    uint32_t len = 0;
    f.read((char*)&len, 4);
    if (!f) return std::nullopt;

    std::string sapl(len, '\0');
    f.read(&sapl[0], len);
    if (!f) return std::nullopt;

    return CacheEntry{hash, std::move(sapl)};
}

bool saveIrc(const std::string& path, uint64_t hash, const std::string& sapl) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(IRC_MAGIC, 4);
    f.write((char*)&IRC_VERSION, 1);
    f.write((char*)&hash, 8);
    uint32_t len = (uint32_t)sapl.size();
    f.write((char*)&len, 4);
    f.write(sapl.data(), (std::streamsize)sapl.size());
    return (bool)f;
}

} // namespace aca

