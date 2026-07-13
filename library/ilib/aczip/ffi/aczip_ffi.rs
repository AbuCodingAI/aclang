#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use std::os::raw::{c_char, c_int, c_double};
use std::ffi::{CString, CStr};
use std::ptr;

#[repr(C)]
pub struct ByteArray {
    pub data: *mut u8,
    pub size: usize,
}

extern "C" {
    pub fn ac_zip_compress(path: *const c_char, parallel: c_int) -> ByteArray;
    pub fn ac_zip_decompress(data: *const u8, size: usize, output_path: *const c_char) -> c_int;
    pub fn ac_zip_compress_hdd(path: *const c_char) -> ByteArray;
    pub fn ac_zip_compress_sata(path: *const c_char) -> ByteArray;
    pub fn ac_get_compression_ratio(original: usize, compressed: usize) -> c_double;
    pub fn ac_free_bytes(arr: ByteArray);
}

pub fn compress(path: &str, parallel: bool) -> Result<Vec<u8>, &'static str> {
    let c_path = CString::new(path).map_err(|_| "Invalid path")?;
    let parallel_flag = if parallel { 1 } else { 0 };

    unsafe {
        let result = ac_zip_compress(c_path.as_ptr(), parallel_flag);
        let bytes = std::slice::from_raw_parts(result.data, result.size).to_vec();
        ac_free_bytes(result);
        Ok(bytes)
    }
}

pub fn decompress(data: &[u8], output_path: &str) -> Result<(), &'static str> {
    let c_output_path = CString::new(output_path).map_err(|_| "Invalid path")?;

    unsafe {
        let ret = ac_zip_decompress(data.as_ptr(), data.len(), c_output_path.as_ptr());
        if ret != 0 {
            return Err("Decompression failed");
        }
        Ok(())
    }
}

pub fn compress_hdd(path: &str) -> Result<Vec<u8>, &'static str> {
    let c_path = CString::new(path).map_err(|_| "Invalid path")?;

    unsafe {
        let result = ac_zip_compress_hdd(c_path.as_ptr());
        let bytes = std::slice::from_raw_parts(result.data, result.size).to_vec();
        ac_free_bytes(result);
        Ok(bytes)
    }
}

pub fn compress_sata(path: &str) -> Result<Vec<u8>, &'static str> {
    let c_path = CString::new(path).map_err(|_| "Invalid path")?;

    unsafe {
        let result = ac_zip_compress_sata(c_path.as_ptr());
        let bytes = std::slice::from_raw_parts(result.data, result.size).to_vec();
        ac_free_bytes(result);
        Ok(bytes)
    }
}

pub fn get_ratio(original_size: usize, compressed_size: usize) -> f64 {
    unsafe { ac_get_compression_ratio(original_size, compressed_size) as f64 }
}
