// AC ilib: machine-audio — Rust implementation (subprocess-based, no external deps)
// Inlined by AC->RS compiler when "use ilib machine-audio" is declared.

use std::process::{Command, Stdio};

fn _which(cmd: &str) -> bool {
    Command::new("which").arg(cmd).stdout(Stdio::null()).stderr(Stdio::null())
        .status().map(|s| s.success()).unwrap_or(false)
}

fn maudio_say(text: &str) -> bool { maudio_say_rate(text, 175) }
fn maudio_say_rate(text: &str, rate: i64) -> bool {
    if _which("espeak-ng") {
        Command::new("espeak-ng").args(["-s", &rate.to_string(), text]).status().is_ok()
    } else if _which("say") {
        Command::new("say").arg(text).status().is_ok()
    } else if _which("espeak") {
        Command::new("espeak").args(["-s", &rate.to_string(), text]).status().is_ok()
    } else {
        eprintln!("[machine-audio] {}", text); false
    }
}

fn maudio_say_async(text: &str) { maudio_say_async_rate(text, 175); }
fn maudio_say_async_rate(text: &str, rate: i64) {
    if _which("espeak-ng") {
        let _ = Command::new("espeak-ng").args(["-s", &rate.to_string(), text]).spawn();
    } else if _which("say") {
        let _ = Command::new("say").arg(text).spawn();
    } else {
        eprintln!("[machine-audio async] {}", text);
    }
}

fn maudio_stop() {
    let _ = Command::new("pkill").args(["-f", "espeak"]).status();
}

fn maudio_listen(timeout_ms: i64) -> String {
    // Try vosk-transcriber (matches the Python vosk path)
    if _which("vosk-transcriber") {
        let secs = std::cmp::max(1, timeout_ms / 1000);
        let out = Command::new("sh")
            .arg("-c")
            .arg(format!("arecord -d {} -f cd /tmp/_ac_audio.wav 2>/dev/null && vosk-transcriber /tmp/_ac_audio.wav 2>/dev/null", secs))
            .output();
        if let Ok(o) = out {
            let s = String::from_utf8_lossy(&o.stdout).trim().to_string();
            if !s.is_empty() { return s; }
        }
    }
    eprintln!("[machine-audio] listen not available (install vosk-transcriber)");
    String::new()
}

fn maudio_play(path: &str) {
    if _which("aplay") {
        let _ = Command::new("aplay").arg(path).spawn();
    } else if _which("paplay") {
        let _ = Command::new("paplay").arg(path).spawn();
    } else {
        eprintln!("[machine-audio] no audio player found");
    }
}

struct _MaudioNS;
impl _MaudioNS {
    fn say(&self, text: &str) -> bool           { maudio_say(text) }
    fn say_rate(&self, t: &str, r: i64) -> bool { maudio_say_rate(t, r) }
    fn say_async(&self, text: &str)             { maudio_say_async(text) }
    fn stop(&self)                               { maudio_stop() }
    fn listen(&self, timeout_ms: i64) -> String { maudio_listen(timeout_ms) }
    fn play(&self, path: &str)                  { maudio_play(path) }
}

static maudio: _MaudioNS = _MaudioNS;
