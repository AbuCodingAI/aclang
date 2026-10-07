// AC ilib: os — Rust FFI (pure stdlib)
// Inlined by AC->RS compiler when "use ilib os" is declared.

use std::process::Command;
use std::fs;
use std::env;
use std::path::Path;

const _SBASH_BLOCKED: &[&str] = &["sudo", "su ", "function ", "nohup", "disown", "tmux", "screen", "&"];

fn _sbash_check(cmd: &str) -> bool {
    for pat in _SBASH_BLOCKED { if cmd.contains(pat) { return false; } }
    true
}

fn os_bash(cmd: &str) -> i64 {
    Command::new("sh").arg("-c").arg(cmd).status().map(|s| s.code().unwrap_or(-1) as i64).unwrap_or(-1)
}
fn os_sbash(cmd: &str) -> i64 {
    if !_sbash_check(cmd) { eprintln!("[os.sbash] The p in bash stands for protection"); return -1; }
    os_bash(cmd)
}
fn os_app_open(app: &str) -> i64 {
    let launcher = ["xdg-open", "open"].iter().find(|&&l| Command::new("which").arg(l).output().map(|o| o.status.success()).unwrap_or(false));
    let cmd = if let Some(l) = launcher { format!("{} {}", l, app) } else { app.to_string() };
    Command::new("sh").arg("-c").arg(&cmd).spawn().map(|_| 0).unwrap_or(-1)
}
fn os_mkfile(path: &str) -> i64 {
    fs::OpenOptions::new().create(true).append(true).open(path).map(|_| 0i64).unwrap_or_else(|e| { eprintln!("[os.mkfile] {}: {}", path, e); -1 })
}
fn os_rmfile(path: &str) -> i64 {
    fs::remove_file(path).map(|_| 0i64).unwrap_or_else(|e| { eprintln!("[os.rmfile] {}: {}", path, e); -1 })
}
fn os_mkdir(path: &str) -> i64 {
    fs::create_dir_all(path).map(|_| 0i64).unwrap_or_else(|e| { eprintln!("[os.mkdir] {}: {}", path, e); -1 })
}
fn os_rmdir(path: &str) -> i64 {
    fs::remove_dir_all(path).map(|_| 0i64).unwrap_or_else(|e| { eprintln!("[os.rmdir] {}: {}", path, e); -1 })
}
fn os_exists(path: &str) -> bool { Path::new(path).exists() }
fn os_cwd() -> String { env::current_dir().map(|p| p.to_string_lossy().into_owned()).unwrap_or_default() }
fn os_env(key: &str) -> String { env::var(key).unwrap_or_default() }
fn os_write_to(path: &str, content: &str) -> i64 {
    fs::write(path, content).map(|_| 0i64).unwrap_or_else(|e| { eprintln!("[os.write_to] {}: {}", path, e); -1 })
}
fn os_append_to(path: &str, content: &str) -> i64 {
    use std::io::Write;
    fs::OpenOptions::new().create(true).append(true).open(path)
        .and_then(|mut f| { f.write_all(content.as_bytes())?; if !content.ends_with('\n') { f.write_all(b"\n")?; } Ok(()) })
        .map(|_| 0i64).unwrap_or_else(|e| { eprintln!("[os.append_to] {}: {}", path, e); -1 })
}
fn os_read(path: &str) -> String {
    fs::read_to_string(path).unwrap_or_else(|e| { eprintln!("[os.read] {}: {}", path, e); String::new() })
}


fn os_isdir(p: &str) -> i64 { if Path::new(p).is_dir() { 1 } else { 0 } }
fn os_isfile(p: &str) -> i64 { if Path::new(p).is_file() { 1 } else { 0 } }
fn os_size(p: &str) -> i64 { fs::metadata(p).map(|m| m.len() as i64).unwrap_or(-1) }
fn os_mtime(p: &str) -> i64 {
    fs::metadata(p).and_then(|m| m.modified()).ok()
        .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
        .map(|d| d.as_secs() as i64).unwrap_or(-1)
}
fn os_copy(src: &str, dst: &str) -> i64 {
    fs::copy(src, dst).map(|_| 0i64).unwrap_or_else(|e| { eprintln!("[os.copy] {}", e); -1 })
}
fn os_move(src: &str, dst: &str) -> i64 {
    if fs::rename(src, dst).is_ok() { return 0; }
    // rename() fails across filesystems: copy, then remove the original
    match fs::copy(src, dst).and_then(|_| fs::remove_file(src)) {
        Ok(()) => 0,
        Err(e) => { eprintln!("[os.move] {}", e); -1 }
    }
}
fn os_listdir(p: &str) -> Vec<String> {
    let mut names: Vec<String> = match fs::read_dir(p) {
        Ok(rd) => rd.filter_map(|e| e.ok()).map(|e| e.file_name().to_string_lossy().into_owned()).collect(),
        Err(e) => { eprintln!("[os.listdir] {}", e); Vec::new() }
    };
    names.sort();
    names
}
fn os_tmpdir() -> String {
    for var in ["TMPDIR", "TEMP"] {
        if let Ok(v) = env::var(var) { if !v.is_empty() { return v; } }
    }
    "/tmp".to_string()
}
// A unique name from the process id and the clock; create_new refuses to reuse an existing file.
fn os_unique_name(suffix: &str) -> String {
    let nanos = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_nanos()).unwrap_or(0);
    format!("{}/acos_{}_{}{}", os_tmpdir(), std::process::id(), nanos, suffix)
}
fn os_tmpfile(suffix: &str) -> String {
    for _ in 0..100 {
        let p = os_unique_name(suffix);
        if fs::OpenOptions::new().write(true).create_new(true).open(&p).is_ok() { return p; }
    }
    String::new()
}
fn os_mktmpdir() -> String {
    for _ in 0..100 {
        let p = os_unique_name("");
        if fs::create_dir(&p).is_ok() { return p; }
    }
    String::new()
}
fn os_chdir(p: &str) -> i64 {
    env::set_current_dir(p).map(|_| 0i64).unwrap_or_else(|e| { eprintln!("[os.chdir] {}", e); -1 })
}
fn os_join(a: &str, b: &str) -> String {
    if a.is_empty() { return b.to_string(); }
    if b.is_empty() { return a.to_string(); }
    if b.starts_with('/') { return b.to_string(); }
    if a.ends_with('/') { format!("{}{}", a, b) } else { format!("{}/{}", a, b) }
}
fn os_basename(p: &str) -> String { p.rsplit('/').next().unwrap_or("").to_string() }
fn os_dirname(p: &str) -> String {
    match p.rfind('/') {
        None => String::new(),
        Some(0) => "/".to_string(),
        Some(cut) => p[..cut].to_string(),
    }
}
fn os_homedir() -> String { env::var("HOME").unwrap_or_default() }

struct _OsNS;
impl _OsNS {
    fn bash(&self, cmd: &str) -> i64      { os_bash(cmd) }
    fn sbash(&self, cmd: &str) -> i64     { os_sbash(cmd) }
    fn app_open(&self, app: &str) -> i64  { os_app_open(app) }
    fn mkfile(&self, p: &str) -> i64      { os_mkfile(p) }
    fn rmfile(&self, p: &str) -> i64      { os_rmfile(p) }
    fn mkdir(&self, p: &str) -> i64       { os_mkdir(p) }
    fn rmdir(&self, p: &str) -> i64       { os_rmdir(p) }
    fn exists(&self, p: &str) -> bool     { os_exists(p) }
    fn cwd(&self) -> String               { os_cwd() }
    fn env(&self, k: &str) -> String      { os_env(k) }
    fn write_to(&self, p: &str, c: &str) -> i64   { os_write_to(p, c) }
    fn append_to(&self, p: &str, c: &str) -> i64  { os_append_to(p, c) }
    fn read(&self, p: &str) -> String     { os_read(p) }
    fn isdir(&self, p: &str) -> i64          { os_isdir(p) }
    fn isfile(&self, p: &str) -> i64         { os_isfile(p) }
    fn size(&self, p: &str) -> i64           { os_size(p) }
    fn mtime(&self, p: &str) -> i64          { os_mtime(p) }
    fn copy(&self, s: &str, d: &str) -> i64  { os_copy(s, d) }
    fn move_(&self, s: &str, d: &str) -> i64 { os_move(s, d) }
    fn listdir(&self, p: &str) -> Vec<String> { os_listdir(p) }
    fn tmpdir(&self) -> String               { os_tmpdir() }
    fn tmpfile(&self, suffix: &str) -> String { os_tmpfile(suffix) }
    fn mktmpdir(&self) -> String             { os_mktmpdir() }
    fn chdir(&self, p: &str) -> i64          { os_chdir(p) }
    fn join(&self, a: &str, b: &str) -> String { os_join(a, b) }
    fn basename(&self, p: &str) -> String    { os_basename(p) }
    fn dirname(&self, p: &str) -> String     { os_dirname(p) }
    fn homedir(&self) -> String              { os_homedir() }
}

static os: _OsNS = _OsNS;
