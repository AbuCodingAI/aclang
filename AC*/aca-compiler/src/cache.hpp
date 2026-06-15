#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace aca {

uint64_t fnv64(const std::string& data);

struct CacheEntry {
    uint64_t hash = 0;
    std::string sapl;
};

// Cache format: magic(4) "ACIR" + version(u8) + hash(u64) + sapl_len(u32) + sapl bytes
std::optional<CacheEntry> loadIrc(const std::string& path, uint64_t expectedHash);
bool saveIrc(const std::string& path, uint64_t hash, const std::string& sapl);

} // namespace aca

