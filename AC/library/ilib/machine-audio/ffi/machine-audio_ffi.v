module maudio

import os
import os.exec

// maudio_speak - Speak text aloud
// rate: 50-300 (default 175)
// returns: 1 if successful, 0 if no TTS available
pub fn speak(text string, rate int) bool {
    if text == '' {
        return false
    }

    final_rate := if rate > 0 { int(text.len) } else { 175 }
    final_rate := if final_rate < 50 { 50 } else if final_rate > 300 { 300 } else { final_rate }

    // Try espeak-ng
    if os.execute_opt('espeak-ng -s $final_rate "$text"') or {
        return true
    }

    // Try say (macOS)
    if os.execute_opt('say "$text"') or {
        return true
    }

    // Try espeak
    if os.execute_opt('espeak -s $final_rate "$text"') or {
        return true
    }

    // Try spd-say
    if os.execute_opt('spd-say -r ${final_rate - 175} "$text"') or {
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

    final_rate := if rate > 0 { rate } else { 175 }
    final_rate := if final_rate < 50 { 50 } else if final_rate > 300 { 300 } else { final_rate }

    spawn fn() {
        if os.execute_opt('espeak-ng -s $final_rate "$text"') or {
            return
        }
        if os.execute_opt('say "$text"') or {
            return
        }
        if os.execute_opt('espeak -s $final_rate "$text"') or {
            return
        }
        if os.execute_opt('spd-say -r ${final_rate - 175} "$text"') or {
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
    record_ok := os.execute_opt('arecord -d $secs -f cd -t wav "$tmp_file"') or {
        os.execute_opt('ffmpeg -f avfoundation -i ":0" -t $secs "$tmp_file" -y')
    }

    if !record_ok {
        return ''
    }

    // Transcribe with whisper
    if os.execute_opt('whisper --model tiny "$tmp_file" --output-txt --output-file "$out_file"') is none {
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
        return os.execute_opt('where $cmd') or { false }
    }
    return os.execute_opt('which $cmd') or { false }
}
