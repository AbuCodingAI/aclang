// AC ilib: string-cheese — Rust implementation (pure stdlib, no external deps)
// Inlined by AC->RS compiler when "use ilib string-cheese" is declared.

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
fn stringm_join(sep: &str, parts: &[&str]) -> String { parts.join(sep) }
fn stringm_format(t: &str) -> String { t.to_string() }

struct _StringmNS;
impl _StringmNS {
    fn lower(&self, s: &str) -> String      { stringm_lower(s) }
    fn upper(&self, s: &str) -> String      { stringm_upper(s) }
    fn trim(&self, s: &str) -> String       { stringm_trim(s) }
    fn strip(&self, s: &str, c: &str) -> String { stringm_strip(s, c) }
    fn find(&self, s: &str, p: &str) -> i64 { stringm_find(s, p) }
    fn replace(&self, s: &str, o: &str, n: &str) -> String { stringm_replace(s, o, n) }
    fn split(&self, s: &str, sep: &str) -> Vec<String> { stringm_split(s, sep) }
    fn length(&self, s: &str) -> i64        { stringm_len(s) }
    fn startswith(&self, s: &str, p: &str) -> bool { stringm_startswith(s, p) }
    fn endswith(&self, s: &str, p: &str) -> bool   { stringm_endswith(s, p) }
    fn count(&self, s: &str, sub: &str) -> i64 { stringm_count(s, sub) }
    fn format(&self, t: &str) -> String     { stringm_format(t) }
}

static stringm: _StringmNS = _StringmNS;
