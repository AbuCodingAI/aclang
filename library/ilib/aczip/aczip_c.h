#ifndef AC_ZIP_C_H
#define AC_ZIP_C_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Plain-C ABI view of a heap-allocated byte buffer returned across the FFI
 * boundary. Layout (pointer, then size_t) matches what every FFI binding
 * (Go cgo, Rust, V, Java/JNA, the raw ASM {rax,rdx} struct-return convention)
 * independently declares for a two-field { data, size } struct-by-value
 * return — so the exact tag name doesn't matter, only field order/widths. */
typedef struct {
    char*  data;
    size_t size;
} ACZipByteArray;

/* Compress a directory into the ACZip v2 archive format (see aczip.cpp).
 * Returns a heap-allocated buffer the caller MUST release with
 * ac_free_bytes(). On failure returns {NULL, 0}. */
ACZipByteArray ac_zip_compress(const char* path, int parallel);

/* Decompress an ACZip v2 archive (data/size) into output_path.
 * Returns 0 on success, non-zero on failure. */
int ac_zip_decompress(const unsigned char* data, size_t size, const char* output_path);

/* Compress optimized for HDD (sequential, less random access). */
ACZipByteArray ac_zip_compress_hdd(const char* path);

/* Compress optimized for SATA (balanced, threaded). */
ACZipByteArray ac_zip_compress_sata(const char* path);

/* Compression ratio as a percentage (compressed / original * 100). */
double ac_get_compression_ratio(size_t original, size_t compressed);

/* Release a buffer returned by ac_zip_compress / ac_zip_compress_hdd /
 * ac_zip_compress_sata. Safe to call on a {NULL, 0} result. */
void ac_free_bytes(ACZipByteArray arr);

/* File-to-file convenience wrappers — AC's `aczip.compress(path, parallel,
 * output_path)` / `aczip.decompress(archive_path, output_path)` call these (every
 * language's FFI binding routes through them) instead of individually marshaling
 * ACZipByteArray across each backend's own FFI. compress_to_file returns the
 * compressed byte count, or -1 on failure. decompress_from_file returns 0 on
 * success, non-zero on failure. */
long long ac_zip_compress_to_file(const char* path, int parallel, const char* output_path);
int ac_zip_decompress_from_file(const char* archive_path, const char* output_path);

#ifdef __cplusplus
}
#endif

/* Call-form shims: the AC compiler emits aczip_x(...) on the C backend (the C++
 * backend gets a real aczip:: namespace via aczip_wrapper — see camera_c.h's
 * identical pattern for the retired-symbol reasoning). */
#ifndef __cplusplus
#define aczip_compress    ac_zip_compress_to_file
#define aczip_decompress  ac_zip_decompress_from_file
#define aczip_get_ratio   ac_get_compression_ratio
#endif

#endif
