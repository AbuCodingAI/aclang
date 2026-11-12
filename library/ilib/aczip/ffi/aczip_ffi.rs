// AC ilib: aczip — Rust FFI (libaczip.so / libaczip.dll)
//
// AC has no raw byte-buffer type, so this binding calls the file-to-file convenience
// functions in aczip_c.h/.cpp (shared with every other backend's FFI — see that file's
// own comment) instead of marshaling ACZipByteArray by hand.
// (No #![allow(non_upper_case_globals)] here — this file is inlined mid-program, not
// at the top, so an inner `#![...]` attribute is a hard syntax error there; the
// resulting "static variable `aczip` should have an upper case name" is just a
// warning, same as camera_ffi.rs's own `pub static camera` leaves unaddressed.)

use std::os::raw::{c_char, c_int, c_longlong};
use std::ffi::CString;

#[link(name = "aczip")]
extern "C" {
    fn ac_zip_compress_to_file(path: *const c_char, parallel: c_int, output_path: *const c_char) -> c_longlong;
    fn ac_zip_decompress_from_file(archive_path: *const c_char, output_path: *const c_char) -> c_int;
    fn ac_get_compression_ratio(original: usize, compressed: usize) -> f64;
    fn ac_zip_iso(srcdir: *const c_char, out_iso: *const c_char, label: *const c_char) -> c_int;
    fn ac_zip_package(srcdir: *const c_char, out_path: *const c_char) -> c_int;
}

fn _cs(s: &str) -> CString { CString::new(s).unwrap_or_default() }

pub struct AcZip;
impl AcZip {
    pub fn compress(&self, path: &str, parallel: i64, output_path: &str) -> i64 {
        let p = _cs(path); let o = _cs(output_path);
        unsafe { ac_zip_compress_to_file(p.as_ptr(), (parallel != 0) as c_int, o.as_ptr()) as i64 }
    }
    pub fn decompress(&self, archive_path: &str, output_path: &str) -> i64 {
        let a = _cs(archive_path); let o = _cs(output_path);
        unsafe { ac_zip_decompress_from_file(a.as_ptr(), o.as_ptr()) as i64 }
    }
    pub fn get_ratio(&self, original: i64, compressed: i64) -> f64 {
        unsafe { ac_get_compression_ratio(original as usize, compressed as usize) }
    }
    pub fn iso(&self, srcdir: &str, out_iso: &str, label: &str) -> i64 {
        let s = _cs(srcdir); let o = _cs(out_iso); let l = _cs(label);
        unsafe { ac_zip_iso(s.as_ptr(), o.as_ptr(), l.as_ptr()) as i64 }
    }
    pub fn pack(&self, srcdir: &str, out_path: &str) -> i64 {
        let s = _cs(srcdir); let o = _cs(out_path);
        unsafe { ac_zip_package(s.as_ptr(), o.as_ptr()) as i64 }
    }
}

pub static aczip: AcZip = AcZip;
