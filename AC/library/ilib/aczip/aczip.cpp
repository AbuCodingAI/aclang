#include "aczip.hpp"
#include <filesystem>
#include <fstream>
#include <zlib.h>
#include <sstream>
#include <thread>
#include <mutex>

namespace fs = std::filesystem;
using namespace aczip;

// ============================================================================
// ACZIP IMPLEMENTATION - OPTIMIZED FOR SPEED
// ============================================================================

std::vector<uint8_t> ACZip::compress(const std::string& path, bool parallel) {
    Archive archive = build_archive(path);

    std::vector<uint8_t> result;
    result.insert(result.end(), {'A', 'C', 'Z', '2'});  // Magic header v2

    // Compress each file individually + in parallel if enabled
    std::vector<std::pair<std::string, std::vector<uint8_t>>> compressed_files;
    std::mutex result_mutex;

    auto compress_file = [&](FileEntry& entry) {
        entry.tag = generate_tag(compressed_files.size());
        auto compressed = ACGzip::compress(entry.data, 6);

        std::lock_guard<std::mutex> lock(result_mutex);
        compressed_files.push_back({entry.tag, compressed});
    };

    if (parallel && archive.files.size() > 1) {
        std::vector<std::thread> threads;
        int num_threads = std::thread::hardware_concurrency();
        size_t files_per_thread = archive.files.size() / num_threads;

        for (int i = 0; i < num_threads && i * files_per_thread < archive.files.size(); i++) {
            size_t start = i * files_per_thread;
            size_t end = (i == num_threads - 1) ? archive.files.size() : (i + 1) * files_per_thread;

            threads.push_back(std::thread([&, start, end]() {
                for (size_t j = start; j < end; j++) {
                    compress_file(archive.files[j]);
                }
            }));
        }

        for (auto& t : threads) {
            t.join();
        }
    } else {
        for (auto& entry : archive.files) {
            compress_file(entry);
        }
    }

    // Write compressed files: [tag(4)][orig_size(4)][comp_size(4)][compressed_data]
    uint32_t file_count = compressed_files.size();
    result.insert(result.end(), (uint8_t*)&file_count, (uint8_t*)&file_count + 4);

    for (auto& [tag, compressed] : compressed_files) {
        // Tag (4 bytes)
        result.insert(result.end(), tag.begin(), tag.end());

        // Original size (4 bytes)
        uint32_t orig_size = compressed.size();  // We don't track original, use this
        result.insert(result.end(), (uint8_t*)&orig_size, (uint8_t*)&orig_size + 4);

        // Compressed size (4 bytes)
        uint32_t comp_size = compressed.size();
        result.insert(result.end(), (uint8_t*)&comp_size, (uint8_t*)&comp_size + 4);

        // Compressed data
        result.insert(result.end(), compressed.begin(), compressed.end());
    }

    return result;
}

void ACZip::decompress(const std::vector<uint8_t>& data, const std::string& output_path) {
    // Check magic header
    if (data.size() < 8 || data[0] != 'A' || data[1] != 'C' || data[2] != 'Z' || data[3] != '2') {
        throw std::runtime_error("Invalid ACZip file");
    }

    size_t pos = 4;

    // Read file count
    uint32_t file_count = *(uint32_t*)(data.data() + pos);
    pos += 4;

    // Read and decompress each file
    for (uint32_t i = 0; i < file_count; i++) {
        if (pos + 12 > data.size()) {
            throw std::runtime_error("Corrupt archive: truncated header");
        }

        // Tag
        std::string tag(data.begin() + pos, data.begin() + pos + 4);
        pos += 4;

        // Sizes
        uint32_t orig_size = *(uint32_t*)(data.data() + pos);
        pos += 4;

        uint32_t comp_size = *(uint32_t*)(data.data() + pos);
        pos += 4;

        if (pos + comp_size > data.size()) {
            throw std::runtime_error("Corrupt archive: truncated data");
        }

        // Compressed data
        std::vector<uint8_t> compressed_data(data.begin() + pos, data.begin() + pos + comp_size);
        pos += comp_size;

        // Decompress
        auto decompressed = ACGzip::decompress(compressed_data);

        // Write file
        std::string file_path = output_path + "/" + tag;
        fs::create_directories(fs::path(file_path).parent_path());

        std::ofstream out(file_path, std::ios::binary);
        out.write((char*)decompressed.data(), decompressed.size());
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

// ============================================================================
// XZ COMPRESSION (LZMA2)
// ============================================================================

std::vector<uint8_t> ACXz::compress(const std::vector<uint8_t>& data, int preset) {
    std::string cmd = "xz -" + std::to_string(preset) + " -c";
    FILE* proc = popen(cmd.c_str(), "w");
    if (!proc) throw std::runtime_error("Failed to start xz");

    fwrite(data.data(), 1, data.size(), proc);
    fclose(proc);

    // Read compressed output
    std::string read_cmd = "xz -" + std::to_string(preset) + " -c";
    FILE* read_proc = popen(read_cmd.c_str(), "r");
    if (!read_proc) throw std::runtime_error("Failed to read xz output");

    std::vector<uint8_t> compressed;
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), read_proc)) > 0) {
        compressed.insert(compressed.end(), buffer, buffer + bytes);
    }
    pclose(read_proc);

    return compressed;
}

std::vector<uint8_t> ACXz::decompress(const std::vector<uint8_t>& data) {
    // Use xz command-line tool
    FILE* proc = popen("xz -d -c", "w");
    if (!proc) throw std::runtime_error("Failed to start xz decompression");

    fwrite(data.data(), 1, data.size(), proc);
    fclose(proc);

    // Read decompressed output
    FILE* read_proc = popen("xz -d -c", "r");
    if (!read_proc) throw std::runtime_error("Failed to read xz output");

    std::vector<uint8_t> decompressed;
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), read_proc)) > 0) {
        decompressed.insert(decompressed.end(), buffer, buffer + bytes);
    }
    pclose(read_proc);

    return decompressed;
}

void ACXz::compress_file(const std::string& input, const std::string& output, int preset) {
    std::string cmd = "xz -" + std::to_string(preset) + " -c \"" + input + "\" > \"" + output + "\"";
    if (system(cmd.c_str()) != 0) {
        throw std::runtime_error("XZ file compression failed");
    }
}

void ACXz::decompress_file(const std::string& input, const std::string& output) {
    std::string cmd = "xz -d -c \"" + input + "\" > \"" + output + "\"";
    if (system(cmd.c_str()) != 0) {
        throw std::runtime_error("XZ file decompression failed");
    }
}

// ============================================================================
// ZSTD COMPRESSION
// ============================================================================

std::vector<uint8_t> ACZstd::compress(const std::vector<uint8_t>& data, int level) {
    std::string cmd = "zstd -" + std::to_string(level) + " -c";
    FILE* proc = popen(cmd.c_str(), "w");
    if (!proc) throw std::runtime_error("Failed to start zstd");

    fwrite(data.data(), 1, data.size(), proc);
    fclose(proc);

    // Read compressed output
    FILE* read_proc = popen(cmd.c_str(), "r");
    if (!read_proc) throw std::runtime_error("Failed to read zstd output");

    std::vector<uint8_t> compressed;
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), read_proc)) > 0) {
        compressed.insert(compressed.end(), buffer, buffer + bytes);
    }
    pclose(read_proc);

    return compressed;
}

std::vector<uint8_t> ACZstd::decompress(const std::vector<uint8_t>& data) {
    FILE* proc = popen("zstd -d -c", "w");
    if (!proc) throw std::runtime_error("Failed to start zstd decompression");

    fwrite(data.data(), 1, data.size(), proc);
    fclose(proc);

    // Read decompressed output
    FILE* read_proc = popen("zstd -d -c", "r");
    if (!read_proc) throw std::runtime_error("Failed to read zstd output");

    std::vector<uint8_t> decompressed;
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), read_proc)) > 0) {
        decompressed.insert(decompressed.end(), buffer, buffer + bytes);
    }
    pclose(read_proc);

    return decompressed;
}

void ACZstd::compress_file(const std::string& input, const std::string& output, int level) {
    std::string cmd = "zstd -" + std::to_string(level) + " -c \"" + input + "\" > \"" + output + "\"";
    if (system(cmd.c_str()) != 0) {
        throw std::runtime_error("Zstd file compression failed");
    }
}

void ACZstd::decompress_file(const std::string& input, const std::string& output) {
    std::string cmd = "zstd -d -c \"" + input + "\" > \"" + output + "\"";
    if (system(cmd.c_str()) != 0) {
        throw std::runtime_error("Zstd file decompression failed");
    }
}
