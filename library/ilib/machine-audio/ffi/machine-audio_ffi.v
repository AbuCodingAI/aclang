module maudio

import os

// Spawn `cmd` via os.new_process with `args` as distinct argv elements — NO shell is
// ever invoked, so text containing '$(...)', backticks, ';', quotes etc. is inert.
// Returns true iff the process was found, started, and exited with status 0.
fn run_argv(cmd string, args []string) bool {
    path := os.find_abs_path_of_executable(cmd) or { return false }
    mut p := os.new_process(path)
    p.set_args(args)
    p.run()
    p.wait()
    return p.code == 0
}

// maudio_speak - Speak text aloud
// rate: 50-300 (default 175)
// returns: 1 if successful, 0 if no TTS available
pub fn speak(text string, rate int) bool {
    if text == '' {
        return false
    }

    mut final_rate := if rate > 0 { rate } else { 175 }
    final_rate = if final_rate < 50 { 50 } else if final_rate > 300 { 300 } else { final_rate }

    // Try espeak-ng
    if run_argv('espeak-ng', ['-s', final_rate.str(), text]) {
        return true
    }

    // Try say (macOS)
    if run_argv('say', [text]) {
        return true
    }

    // Try espeak
    if run_argv('espeak', ['-s', final_rate.str(), text]) {
        return true
    }

    // Try spd-say
    if run_argv('spd-say', ['-r', (final_rate - 175).str(), text]) {
        return true
    }

    // Fallback
    println('[machine-audio::speak] $text')
    return false
}

// maudio_speak_async - Speak text without blocking
pub fn speak_async(text string, rate int) {
    if text == '' {
        return
    }

    mut final_rate := if rate > 0 { rate } else { 175 }
    final_rate = if final_rate < 50 { 50 } else if final_rate > 300 { 300 } else { final_rate }

    spawn fn [final_rate, text] () {
        if run_argv('espeak-ng', ['-s', final_rate.str(), text]) {
            return
        }
        if run_argv('say', [text]) {
            return
        }
        if run_argv('espeak', ['-s', final_rate.str(), text]) {
            return
        }
        if run_argv('spd-say', ['-r', (final_rate - 175).str(), text]) {
            return
        }
        println('[machine-audio::speak_async] $text')
    }()
}

// maudio_listen - Record from microphone and transcribe
// timeout_ms: timeout in milliseconds
// returns: transcribed text
pub fn listen(timeout_ms int) string {
    final_timeout := if timeout_ms > 0 { timeout_ms } else { 5000 }
    secs := (final_timeout + 999) / 1000

    tmp_file := '/tmp/maudio_${os.getpid()}.wav'
    out_file := '${tmp_file}_out'

    // Record using arecord or ffmpeg
    record_ok := run_argv('arecord', ['-d', secs.str(), '-f', 'cd', '-t', 'wav', tmp_file])
        || run_argv('ffmpeg', ['-f', 'avfoundation', '-i', ':0', '-t', secs.str(), tmp_file, '-y'])

    if !record_ok {
        return ''
    }

    // Transcribe with whisper
    if run_argv('whisper', ['--model', 'tiny', tmp_file, '--output-txt', '--output-file', out_file]) {
        txt_path := '${out_file}.txt'
        if os.exists(txt_path) {
            text := os.read_file(txt_path) or { return '' }
            result := text.trim_space()

            // Cleanup
            os.rm(tmp_file) or {}
            os.rm(txt_path) or {}

            return result
        }
    }

    // Cleanup on failure
    os.rm(tmp_file) or {}
    return ''
}

// maudio_tts_ok - Check if Text-to-Speech is available
// returns: 1 if available, 0 otherwise
pub fn tts_ok() int {
    if command_exists('espeak-ng') {
        return 1
    }
    if command_exists('say') {
        return 1
    }
    if command_exists('espeak') {
        return 1
    }
    if command_exists('spd-say') {
        return 1
    }
    return 0
}

// maudio_stt_ok - Check if Speech-to-Text is available
// returns: 1 if available, 0 otherwise
pub fn stt_ok() int {
    return if command_exists('whisper') { 1 } else { 0 }
}

// Helper: Check if command exists
fn command_exists(cmd string) bool {
    if os.getenv('OS') == 'Windows_NT' {
        return run_argv('where', [cmd])
    }
    return run_argv('which', [cmd])
}
