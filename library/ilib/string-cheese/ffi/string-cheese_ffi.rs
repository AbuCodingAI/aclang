// AC ilib: string-cheese — Rust implementation (pure stdlib, no external deps)
// Inlined by AC->RS compiler when "use ilib string-cheese" is declared.
// No Cargo/external crates: this is dropped verbatim into a file compiled with plain
// `rustc`, so everything below must build with `rustc --crate-type bin` off std alone.
use std::io::BufRead;

const _WS: &str = " \t\n\r";

fn _is_ws(s: &str) -> bool { s == _WS }

fn stringm_lower(s: &str) -> String { s.to_lowercase() }
fn stringm_upper(s: &str) -> String { s.to_uppercase() }
fn stringm_trim(s: &str) -> String  { s.trim().to_string() }
fn stringm_strip(s: &str, chars: &str) -> String {
    if _is_ws(chars) { return s.trim().to_string(); }
    let c: Vec<char> = chars.chars().collect();
    s.trim_matches(|ch| c.contains(&ch)).to_string()
}
fn stringm_find(s: &str, pattern: &str) -> i64 {
    if _is_ws(pattern) {
        return s.char_indices()
            .find(|(_, c)| c.is_whitespace())
            .map(|(i, _)| i as i64)
            .unwrap_or(-1);
    }
    s.find(pattern).map(|i| i as i64).unwrap_or(-1)
}
fn stringm_replace(s: &str, old: &str, new: &str) -> String {
    if _is_ws(old) {
        return s.split_whitespace().collect::<Vec<_>>().join(new);
    }
    s.replace(old, new)
}
fn stringm_split(s: &str, sep: &str) -> Vec<String> {
    if _is_ws(sep) { return s.split_whitespace().map(|p| p.to_string()).collect(); }
    s.split(sep).map(|p| p.to_string()).collect()
}
fn stringm_split_nth(s: &str, sep: &str, n: usize) -> String {
    let parts = stringm_split(s, sep);
    parts.get(n).cloned().unwrap_or_default()
}
fn stringm_len(s: &str) -> i64 { s.chars().count() as i64 }
fn stringm_startswith(s: &str, prefix: &str) -> bool { s.starts_with(prefix) }
fn stringm_endswith(s: &str, suffix: &str) -> bool   { s.ends_with(suffix) }
fn stringm_count(s: &str, sub: &str) -> i64 {
    if _is_ws(sub) { return s.chars().filter(|c| c.is_whitespace()).count() as i64; }
    let mut n = 0i64; let mut pos = 0;
    while let Some(i) = s[pos..].find(sub) { n += 1; pos += i + sub.len(); }
    n
}
// join takes parts by value as Vec<String> (not &[&str]) because the AC compiler passes
// split()'s Vec<String> result straight through by value with no auto-ref/borrow — so
// `stringm.join(sep, stringm.split(s, sep2))` needs to type-check with zero casts inserted.
fn stringm_join(sep: &str, parts: Vec<String>) -> String { parts.join(sep) }
fn stringm_format(t: &str) -> String { t.to_string() }

// b / f / t — string-prefix constructors (bytes / f-string / t-string). f/t interpolate
// at the compiler/IR level; the runtime is a passthrough (mirrors ac_stringm_b/f/t in
// string_cheese_c.h — identity for a Rust `&str`/`String`).
fn stringm_b(s: &str) -> String { s.to_string() }
fn stringm_f(s: &str) -> String { s.to_string() }
fn stringm_t(s: &str) -> String { s.to_string() }

fn stringm_ischar(s: &str) -> i64 {
    if s.is_empty() { return 0; }
    if s.chars().all(|c| c.is_alphabetic()) { 1 } else { 0 }
}
fn stringm_isws(s: &str) -> i64 {
    if s.is_empty() { return 1; }
    if s.chars().all(|c| c.is_whitespace()) { 1 } else { 0 }
}

// getline/scan read from the process-wide buffered stdin (std::io::stdin() shares one
// global lock/buffer across calls, so successive reads correctly advance through the
// stream — no hand-rolled persistent reader needed, unlike languages without that guarantee).
fn stringm_getline() -> String {
    let mut line = String::new();
    match std::io::stdin().lock().read_line(&mut line) {
        Ok(0) | Err(_) => String::new(),
        Ok(_) => line.trim_end_matches(['\r', '\n']).to_string(),
    }
}
fn stringm_scan(needle: &str) -> i64 {
    let mut line = String::new();
    match std::io::stdin().lock().read_line(&mut line) {
        Ok(0) | Err(_) => 0,
        Ok(_) => if line.contains(needle) { 1 } else { 0 },
    }
}

struct _StringmNS;
impl _StringmNS {
    fn lower(&self, s: &str) -> String      { stringm_lower(s) }
    fn upper(&self, s: &str) -> String      { stringm_upper(s) }
    fn trim(&self, s: &str) -> String       { stringm_trim(s) }
    // AC strip = 1-arg trim, same convention as the C/C++/V bindings in this ilib
    // (string_cheese_c.h: "AC strip = 1-arg trim") — the 2-arg stringm_strip helper
    // above is still available for anything that wants a real custom-charset strip.
    fn strip(&self, s: &str) -> String      { stringm_trim(s) }
    fn find(&self, s: &str, p: &str) -> i64 { stringm_find(s, p) }
    fn replace(&self, s: &str, o: &str, n: &str) -> String { stringm_replace(s, o, n) }
    fn split(&self, s: &str, sep: &str) -> Vec<String> { stringm_split(s, sep) }
    fn split_nth(&self, s: &str, sep: &str, n: usize) -> String { stringm_split_nth(s, sep, n) }
    fn join(&self, sep: &str, parts: Vec<String>) -> String { stringm_join(sep, parts) }
    fn length(&self, s: &str) -> i64        { stringm_len(s) }
    // startswith/endswith/ischar/isws/scan return i64 (0/1), not bool: the AC compiler's
    // Rust backend declares any ilib call result it doesn't specifically recognize as a
    // String/Vec<String> as `i64` (see BackendStrategy::isAcStrFunc/isAcStrListFunc in
    // ir_codegen.cpp and RustStrategy::emitCall) — a Rust `bool` return here is a hard
    // type mismatch at the call site.
    fn startswith(&self, s: &str, p: &str) -> i64 { if stringm_startswith(s, p) { 1 } else { 0 } }
    fn endswith(&self, s: &str, p: &str) -> i64   { if stringm_endswith(s, p) { 1 } else { 0 } }
    fn count(&self, s: &str, sub: &str) -> i64 { stringm_count(s, sub) }
    fn format(&self, t: &str) -> String     { stringm_format(t) }
    fn b(&self, s: &str) -> String { stringm_b(s) }
    fn f(&self, s: &str) -> String { stringm_f(s) }
    fn t(&self, s: &str) -> String { stringm_t(s) }
    fn ischar(&self, s: &str) -> i64 { stringm_ischar(s) }
    fn isws(&self, s: &str) -> i64   { stringm_isws(s) }
    fn getline(&self) -> String { stringm_getline() }
    fn scan(&self, needle: &str) -> i64 { stringm_scan(needle) }
}

static stringm: _StringmNS = _StringmNS;
