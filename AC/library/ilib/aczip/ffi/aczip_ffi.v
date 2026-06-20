module aczip

#flag -laczip
#include "aczip.h"

pub struct ByteArray {
	data &u8
	size usize
}

fn C.ac_zip_compress(path &char, parallel int) ByteArray
fn C.ac_zip_decompress(data &u8, size usize, output_path &char) int
fn C.ac_zip_compress_hdd(path &char) ByteArray
fn C.ac_zip_compress_sata(path &char) ByteArray
fn C.ac_get_compression_ratio(original usize, compressed usize) f64
fn C.ac_free_bytes(arr ByteArray)

pub fn compress(path string, parallel bool) ![]u8 {
	c_path := path.str
	c_parallel := if parallel { 1 } else { 0 }

	result := C.ac_zip_compress(c_path, c_parallel)
	bytes := result.data[0..result.size].clone()
	C.ac_free_bytes(result)

	return bytes
}

pub fn decompress(data []u8, output_path string) !void {
	c_output := output_path.str
	ret := C.ac_zip_decompress(data.data, data.len, c_output)

	if ret != 0 {
		return error('decompression failed')
	}
}

pub fn compress_hdd(path string) ![]u8 {
	c_path := path.str

	result := C.ac_zip_compress_hdd(c_path)
	bytes := result.data[0..result.size].clone()
	C.ac_free_bytes(result)

	return bytes
}

pub fn compress_sata(path string) ![]u8 {
	c_path := path.str

	result := C.ac_zip_compress_sata(c_path)
	bytes := result.data[0..result.size].clone()
	C.ac_free_bytes(result)

	return bytes
}

pub fn get_ratio(original_size usize, compressed_size usize) f64 {
	return C.ac_get_compression_ratio(original_size, compressed_size)
}
