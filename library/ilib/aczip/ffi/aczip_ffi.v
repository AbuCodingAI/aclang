// AC ilib: aczip — V C interop (libaczip.so / libaczip.dll)
module main

#flag -L @AC_LIBDIR@ -laczip
#flag -Wl,-rpath,@AC_LIBDIR@
#include "@AC_LIBDIR@/aczip_c.h"

fn C.ac_zip_compress_to_file(path &char, parallel int, output_path &char) i64
fn C.ac_zip_decompress_from_file(archive_path &char, output_path &char) int
fn C.ac_get_compression_ratio(original usize, compressed usize) f64

// AC has no raw byte-buffer type, so this binding calls the file-to-file convenience
// functions in aczip_c.h/.cpp (shared with every other backend's FFI — see that file's
// own comment) instead of marshaling ACZipByteArray by hand.
fn aczip_compress(path string, parallel i64, output_path string) i64 {
    par := if parallel != 0 { 1 } else { 0 }
    return C.ac_zip_compress_to_file(path.str, par, output_path.str)
}
fn aczip_decompress(archive_path string, output_path string) i64 {
    return i64(C.ac_zip_decompress_from_file(archive_path.str, output_path.str))
}
fn aczip_get_ratio(original i64, compressed i64) f64 {
    return C.ac_get_compression_ratio(usize(original), usize(compressed))
}

// Namespace struct — AC-generated V uses aczip.compress(...), aczip.decompress(...)
struct AcZipNS {}
fn (o AcZipNS) compress(path string, parallel i64, output_path string) i64 { return aczip_compress(path, parallel, output_path) }
fn (o AcZipNS) decompress(archive_path string, output_path string) i64     { return aczip_decompress(archive_path, output_path) }
fn (o AcZipNS) get_ratio(original i64, compressed i64) f64                 { return aczip_get_ratio(original, compressed) }

const aczip = AcZipNS{}
