// AC ilib: aczip — Go CGO FFI (libaczip.so / libaczip.dll)
package main

/*
#cgo CFLAGS: -I${SRCDIR}/library/ilib/aczip
#cgo LDFLAGS: -L${SRCDIR}/library/ilib/aczip -laczip -Wl,-rpath,${SRCDIR}/library/ilib/aczip
#include <stdlib.h>
#include "aczip_c.h"
*/
import "C"
import "unsafe"

// AC has no raw byte-buffer type, so this binding calls the file-to-file convenience
// functions in aczip_c.h/.cpp (shared with every other backend's FFI — see that file's
// own comment) instead of marshaling ACZipByteArray by hand.
func aczip_compress(path string, parallel int64, outputPath string) int64 {
    cp := C.CString(path); defer C.free(unsafe.Pointer(cp))
    op := C.CString(outputPath); defer C.free(unsafe.Pointer(op))
    par := C.int(0); if parallel != 0 { par = 1 }
    return int64(C.ac_zip_compress_to_file(cp, par, op))
}
func aczip_decompress(archivePath string, outputPath string) int64 {
    ap := C.CString(archivePath); defer C.free(unsafe.Pointer(ap))
    op := C.CString(outputPath); defer C.free(unsafe.Pointer(op))
    return int64(C.ac_zip_decompress_from_file(ap, op))
}
func aczip_get_ratio(original int64, compressed int64) float64 {
    return float64(C.ac_get_compression_ratio(C.size_t(original), C.size_t(compressed)))
}

// Namespace struct — AC-generated Go uses aczip.compress(...), aczip.decompress(...)
type acZipNS struct{}
func (acZipNS) compress(path string, parallel int64, outputPath string) int64 { return aczip_compress(path, parallel, outputPath) }
func (acZipNS) decompress(archivePath string, outputPath string) int64        { return aczip_decompress(archivePath, outputPath) }
func (acZipNS) get_ratio(original int64, compressed int64) float64            { return aczip_get_ratio(original, compressed) }

var aczip = acZipNS{}
