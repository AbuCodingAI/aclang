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
    if !_sbash_check(cmd) { eprintln!("[os.sbash] blocked"); return -1; }
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
}

static os: _OsNS = _OsNS;
