#include "aczip.hpp"
#include <filesystem>
#include <fstream>
#include <zlib.h>
#include <sstream>
#include <queue>

namespace fs = std::filesystem;
using namespace aczip;

// ============================================================================
// ACZIP IMPLEMENTATION
// ============================================================================

std::vector<uint8_t> ACZip::compress(const std::string& path, bool parallel) {
    Archive archive = build_archive(path);

    std::string serialized;
    serialized += "ACZP";  // Magic header

    // Tag each file with 4-bit protocol
    int file_counter = 0;
    for (auto& entry : archive.files) {
        file_counter++;
        entry.tag = generate_tag(file_counter);
        serialized += entry.tag;
        serialized += std::to_string(entry.data.size()) + ":";
        serialized.append((char*)entry.data.data(), entry.data.size());
    }

    // Gzip compress the archive
    return ACGzip::compress(
        std::vector<uint8_t>(serialized.begin(), serialized.end()), 6);
}

void ACZip::decompress(const std::vector<uint8_t>& data, const std::string& output_path) {
    // Decompress gzip first
    auto decompressed = ACGzip::decompress(data);
    std::string serialized(decompressed.begin(), decompressed.end());

    // Skip magic header
    if (serialized.substr(0, 4) != "ACZP") {
        throw std::runtime_error("Invalid ACZip file");
    }

    // Parse files and extract
    size_t pos = 4;
    while (pos < serialized.length()) {
        std::string tag;
        for (int i = 0; i < 4 && pos < serialized.length(); i++) {
            tag += serialized[pos++];
        }

        size_t colon = serialized.find(':', pos);
        size_t size = std::stoll(serialized.substr(pos, colon - pos));
        pos = colon + 1;

        std::vector<uint8_t> file_data;
        file_data.insert(file_data.end(),
                        serialized.begin() + pos,
                        serialized.begin() + pos + size);
        pos += size;

        // Reconstruct file from tag
        std::string file_path = output_path + "/" + tag;
        fs::create_directories(fs::path(file_path).parent_path());

        std::ofstream out(file_path, std::ios::binary);
        out.write((char*)file_data.data(), file_data.size());
    }
}

std::vector<uint8_t> ACZip::compress_hdd(const std::string& path) {
    // Sequential compression, less random access
    return compress(path, false);
}

std::vector<uint8_t> ACZip::compress_sata(const std::string& path) {
    // Balanced compression
    return compress(path, true);
}

double ACZip::get_ratio(size_t original, size_t compressed) {
    if (original == 0) return 0.0;
    return (compressed * 100.0) / original;
}

Archive ACZip::build_archive(const std::string& path) {
    Archive archive;

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());

            FileEntry fe;
            fe.path = entry.path().relative_path().string();
            fe.data = data;

            archive.files.push_back(fe);
        }
    }

    return archive;
}

std::string ACZip::generate_tag(int index) {
    if (index <= 15) {
        char buf[3];
        snprintf(buf, sizeof(buf), "0x%X", index);
        return std::string(buf);
    }
    // Folder tags
    int folder = (index / 15) + 1;
    int file = (index % 15) + 1;
    char buf[10];
    snprintf(buf, sizeof(buf), "1x%02d.0x%02X", folder, file);
    return std::string(buf);
}

// ============================================================================
// TAR IMPLEMENTATION
// ============================================================================

std::vector<uint8_t> ACTar::create(const std::string& path) {
    std::string cmd = "tar -cf - \"" + path + "\"";
    FILE* proc = popen(cmd.c_str(), "r");
    if (!proc) throw std::runtime_error("Failed to create tar");

    std::vector<uint8_t> tar_data;
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), proc)) > 0) {
        tar_data.insert(tar_data.end(), buffer, buffer + bytes);
    }

    pclose(proc);
    return tar_data;
}

void ACTar::extract(const std::vector<uint8_t>& data, const std::string& output_path) {
    std::string cmd = "tar -xf - -C \"" + output_path + "\"";
    FILE* proc = popen(cmd.c_str(), "w");
    if (!proc) throw std::runtime_error("Failed to extract tar");

    fwrite(data.data(), 1, data.size(), proc);
    pclose(proc);
}

// ============================================================================
// GZIP IMPLEMENTATION
// ============================================================================

std::vector<uint8_t> ACGzip::compress(const std::vector<uint8_t>& data, int level) {
    std::vector<uint8_t> compressed;
    compressed.resize(compressBound(data.size()));

    uLongf compressed_size = compressed.size();
    int result = compress2(compressed.data(), &compressed_size,
                          data.data(), data.size(), level);

    if (result != Z_OK) {
        throw std::runtime_error("Gzip compression failed");
    }

    compressed.resize(compressed_size);
    return compressed;
}

std::vector<uint8_t> ACGzip::decompress(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> decompressed(data.size() * 4);

    uLongf decompressed_size = decompressed.size();
    int result = uncompress(decompressed.data(), &decompressed_size,
                           data.data(), data.size());

    if (result == Z_BUF_ERROR) {
        decompressed.resize(decompressed_size * 2);
        decompressed_size = decompressed.size();
        result = uncompress(decompressed.data(), &decompressed_size,
                           data.data(), data.size());
    }

    if (result != Z_OK) {
        throw std::runtime_error("Gzip decompression failed");
    }

    decompressed.resize(decompressed_size);
    return decompressed;
}
