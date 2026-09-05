#ifndef AC_ACZIP_WRAPPER_HPP
#define AC_ACZIP_WRAPPER_HPP
#include "aczip_c.h"
#include <string>

// AC source calls aczip.compress(path, parallel, output_path) / aczip.decompress(...)
// / aczip.get_ratio(...) — aczip.hpp's own C++ API lives in `namespace aczip { class
// ACZip {...} }`, so a variable literally named `aczip` (needed to match AC's dotted
// call syntax) can't coexist with it in the same scope. This thin wrapper forwards to
// the extern "C" file-to-file convenience functions in aczip_c.h/.cpp instead — the
// same ones every other backend's FFI binding routes through (see that file's comment)
// — rather than pulling in the full byte-vector-based aczip::ACZip API directly.
struct _AcZipNS {
    long long compress(const std::string& path, bool parallel, const std::string& output_path) const {
        return ac_zip_compress_to_file(path.c_str(), parallel ? 1 : 0, output_path.c_str());
    }
    int decompress(const std::string& archive_path, const std::string& output_path) const {
        return ac_zip_decompress_from_file(archive_path.c_str(), output_path.c_str());
    }
    double get_ratio(size_t original, size_t compressed) const {
        return ac_get_compression_ratio(original, compressed);
    }
};
static _AcZipNS aczip;
#endif
