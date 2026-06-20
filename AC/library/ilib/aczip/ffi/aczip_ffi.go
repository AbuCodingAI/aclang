package aczip

import (
	"unsafe"
)

/*
#cgo LDFLAGS: -laczip
#include <stdlib.h>

typedef struct {
	char* data;
	size_t size;
} ByteArray;

ByteArray ac_zip_compress(const char* path, int parallel);
int ac_zip_decompress(const unsigned char* data, size_t size, const char* output_path);
ByteArray ac_zip_compress_hdd(const char* path);
ByteArray ac_zip_compress_sata(const char* path);
double ac_get_compression_ratio(size_t original, size_t compressed);
void ac_free_bytes(ByteArray arr);
*/
import "C"

// Compress directory using ACZip protocol
func Compress(path string, parallel bool) ([]byte, error) {
	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	cParallel := 0
	if parallel {
		cParallel = 1
	}

	result := C.ac_zip_compress(cPath, C.int(cParallel))
	defer C.ac_free_bytes(result)

	return C.GoBytes(unsafe.Pointer(result.data), C.int(result.size)), nil
}

// Decompress ACZip archive
func Decompress(data []byte, outputPath string) error {
	cOutputPath := C.CString(outputPath)
	defer C.free(unsafe.Pointer(cOutputPath))

	ret := C.ac_zip_decompress((*C.uchar)(unsafe.Pointer(&data[0])), C.size_t(len(data)), cOutputPath)
	if ret != 0 {
		return ErrDecompressionFailed
	}
	return nil
}

// Compress optimized for HDD
func CompressHDD(path string) ([]byte, error) {
	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	result := C.ac_zip_compress_hdd(cPath)
	defer C.ac_free_bytes(result)

	return C.GoBytes(unsafe.Pointer(result.data), C.int(result.size)), nil
}

// Compress optimized for SATA
func CompressSATA(path string) ([]byte, error) {
	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	result := C.ac_zip_compress_sata(cPath)
	defer C.ac_free_bytes(result)

	return C.GoBytes(unsafe.Pointer(result.data), C.int(result.size)), nil
}

// Get compression ratio
func GetRatio(originalSize, compressedSize int64) float64 {
	return float64(C.ac_get_compression_ratio(C.size_t(originalSize), C.size_t(compressedSize)))
}

var ErrDecompressionFailed = error(nil)
