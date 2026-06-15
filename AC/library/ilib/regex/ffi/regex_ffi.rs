// AC ilib: regex — Rust FFI (libacregex.so / acregex.dll)
use std::ffi::{CString, CStr};
use std::os::raw::{c_char, c_int};

#[link(name = "acregex")]
extern "C" {
    fn ac_regex_match(str: *const c_char, pat: *const c_char) -> c_int;
    fn ac_regex_test(str: *const c_char, pat: *const c_char) -> c_int;
    fn ac_regex_search(str: *const c_char, pat: *const c_char) -> *const c_char;
    fn ac_regex_replace(str: *const c_char, pat: *const c_char, repl: *const c_char) -> *const c_char;
    fn ac_regex_replace_all(str: *const c_char, pat: *const c_char, repl: *const c_char) -> *const c_char;
    fn ac_regex_count(str: *const c_char, pat: *const c_char) -> c_int;
    fn ac_regex_escape(str: *const c_char) -> *const c_char;
    fn ac_regex_find_all(str: *const c_char, pat: *const c_char, out_count: *mut c_int) -> *mut *mut c_char;
    fn ac_regex_split(str: *const c_char, pat: *const c_char, out_count: *mut c_int) -> *mut *mut c_char;
    fn ac_regex_groups(str: *const c_char, pat: *const c_char, out_count: *mut c_int) -> *mut *mut c_char;
    fn ac_regex_free_list(list: *mut *mut c_char, count: c_int);
}

fn _cs(s: &str) -> CString { CString::new(s).unwrap_or_default() }
fn _gs(p: *const c_char) -> String {
    if p.is_null() { return String::new(); }
    unsafe { CStr::from_ptr(p).to_string_lossy().into_owned() }
}
fn _list(arr: *mut *mut c_char, n: c_int) -> Vec<String> {
    if arr.is_null() { return vec![]; }
    let count = n as usize;
    let result = unsafe {
        std::slice::from_raw_parts(arr as *const *mut c_char, count)
            .iter()
            .map(|&p| _gs(p as *const c_char))
            .collect()
    };
    unsafe { ac_regex_free_list(arr, n); }
    result
}

pub fn regex_match(s: &str, p: &str) -> bool {
    let cs, cp = _cs(s), _cs(p);
    unsafe { ac_regex_match(cs.as_ptr(), cp.as_ptr()) != 0 }
}
pub fn regex_test(s: &str, p: &str) -> bool {
    let cs, cp = _cs(s), _cs(p);
    unsafe { ac_regex_test(cs.as_ptr(), cp.as_ptr()) != 0 }
}
pub fn regex_search(s: &str, p: &str) -> String {
    let cs, cp = _cs(s), _cs(p);
    _gs(unsafe { ac_regex_search(cs.as_ptr(), cp.as_ptr()) })
}
pub fn regex_replace(s: &str, p: &str, r: &str) -> String {
    let cs, cp, cr = _cs(s), _cs(p), _cs(r);
    _gs(unsafe { ac_regex_replace(cs.as_ptr(), cp.as_ptr(), cr.as_ptr()) })
}
pub fn regex_replace_all(s: &str, p: &str, r: &str) -> String {
    let cs, cp, cr = _cs(s), _cs(p), _cs(r);
    _gs(unsafe { ac_regex_replace_all(cs.as_ptr(), cp.as_ptr(), cr.as_ptr()) })
}
pub fn regex_count(s: &str, p: &str) -> i32 {
    let cs, cp = _cs(s), _cs(p);
    unsafe { ac_regex_count(cs.as_ptr(), cp.as_ptr()) }
}
pub fn regex_escape(s: &str) -> String {
    let cs = _cs(s);
    _gs(unsafe { ac_regex_escape(cs.as_ptr()) })
}
pub fn regex_find_all(s: &str, p: &str) -> Vec<String> {
    let cs, cp = _cs(s), _cs(p); let mut n: c_int = 0;
    _list(unsafe { ac_regex_find_all(cs.as_ptr(), cp.as_ptr(), &mut n) }, n)
}
pub fn regex_split(s: &str, p: &str) -> Vec<String> {
    let cs, cp = _cs(s), _cs(p); let mut n: c_int = 0;
    _list(unsafe { ac_regex_split(cs.as_ptr(), cp.as_ptr(), &mut n) }, n)
}
pub fn regex_groups(s: &str, p: &str) -> Vec<String> {
    let cs, cp = _cs(s), _cs(p); let mut n: c_int = 0;
    _list(unsafe { ac_regex_groups(cs.as_ptr(), cp.as_ptr(), &mut n) }, n)
}

// regex namespace object — AC-generated Rust uses regex.match(s, p), etc.
pub struct AcRegex;
impl AcRegex {
    pub fn match_(&self, s: &str, p: &str) -> bool      { regex_match(s, p)         }
    pub fn test(&self, s: &str, p: &str) -> bool         { regex_test(s, p)          }
    pub fn search(&self, s: &str, p: &str) -> String     { regex_search(s, p)        }
    pub fn replace(&self, s: &str, p: &str, r: &str) -> String { regex_replace(s,p,r) }
    pub fn replace_all(&self, s: &str, p: &str, r: &str) -> String { regex_replace_all(s,p,r) }
    pub fn count(&self, s: &str, p: &str) -> i32         { regex_count(s, p)         }
    pub fn escape(&self, s: &str) -> String              { regex_escape(s)           }
    pub fn find_all(&self, s: &str, p: &str) -> Vec<String> { regex_find_all(s, p)  }
    pub fn split(&self, s: &str, p: &str) -> Vec<String> { regex_split(s, p)        }
    pub fn groups(&self, s: &str, p: &str) -> Vec<String> { regex_groups(s, p)      }
}
pub static regex: AcRegex = AcRegex;
