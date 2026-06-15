// AC ilib: machine-audio — Speech-to-Text and Text-to-Speech
// use ilib machine-audio
//
// Backend selection:
//   AC_MAUDIO_ESPEAK  — TTS via eSpeak-NG (Linux/macOS/Windows, open-source)
//   AC_MAUDIO_POCKETSPHINX — STT via PocketSphinx (offline, open-source)
//   AC_MAUDIO_STUB    — compile-only stub (default when no backend defined)
#pragma once
#include <string>
#include <cstdio>
#include <cstdlib>

#if defined(AC_MAUDIO_ESPEAK)
#  include <espeak-ng/speak_lib.h>
#endif
#if defined(AC_MAUDIO_POCKETSPHINX)
#  include <pocketsphinx.h>
#endif

namespace ac_machine_audio {

// ── Text-to-Speech ───────────────────────────────────────────────────────────

// Speak text aloud (blocking until done).
// Returns true on success.
inline bool speak(const std::string& text, int rate = 175, const std::string& voice = "en") {
#if defined(AC_MAUDIO_ESPEAK)
    espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, nullptr, 0);
    espeak_SetVoiceByName(voice.c_str());
    espeak_SetParameter(espeakRATE, rate, 0);
    espeak_Synth(text.c_str(), text.size() + 1, 0, POS_CHARACTER, 0, espeakCHARS_AUTO, nullptr, nullptr);
    espeak_Synchronize();
    return true;
#else
    // Fallback: use system TTS via shell
    (void)rate; (void)voice;
    std::string cmd = "espeak-ng \"" + text + "\" 2>/dev/null || say \"" + text + "\" 2>/dev/null || echo '[TTS] " + text + "'";
    std::system(cmd.c_str());
    return true;
#endif
}

// Speak asynchronously (non-blocking, fire-and-forget).
inline void speak_async(const std::string& text, int rate = 175, const std::string& voice = "en") {
    (void)rate; (void)voice;
    std::string cmd = "(espeak-ng \"" + text + "\" 2>/dev/null || say \"" + text + "\" 2>/dev/null) &";
    std::system(cmd.c_str());
}

// ── Speech-to-Text ───────────────────────────────────────────────────────────

// Listen from microphone and return transcribed text (blocking).
// Requires a backend (PocketSphinx, whisper.cpp, or a cloud API wrapper).
// Returns empty string if STT is unavailable.
inline std::string listen(int timeout_ms = 5000) {
#if defined(AC_MAUDIO_POCKETSPHINX)
    (void)timeout_ms;
    ps_decoder_t* ps = ps_init(nullptr);
    if (!ps) return "";
    ps_start_utt(ps);
    // Note: real microphone capture requires PortAudio or ALSA integration.
    // This skeleton shows the PocketSphinx decode path; audio capture must be added.
    ps_end_utt(ps);
    int32_t score;
    const char* hyp = ps_get_hyp(ps, &score);
    std::string result = hyp ? hyp : "";
    ps_free(ps);
    return result;
#else
    (void)timeout_ms;
    // Fallback: try whisper.cpp CLI if present
    std::string tmp = "/tmp/ac_stt_capture.wav";
    // Record for (timeout_ms/1000) seconds using arecord (Linux) or rec (sox)
    int secs = std::max(1, timeout_ms / 1000);
    std::string rec_cmd = "arecord -d " + std::to_string(secs) + " -f cd -t wav " + tmp + " 2>/dev/null";
    std::system(rec_cmd.c_str());
    // Transcribe with whisper.cpp CLI if available
    FILE* pipe = popen(("whisper --model tiny " + tmp + " --output-txt --output-file /tmp/ac_stt_out 2>/dev/null && cat /tmp/ac_stt_out.txt 2>/dev/null").c_str(), "r");
    if (!pipe) return "";
    char buf[4096] = {};
    fread(buf, 1, sizeof(buf) - 1, pipe);
    pclose(pipe);
    return std::string(buf);
#endif
}

// ── Utilities ────────────────────────────────────────────────────────────────

// Check if TTS is available on this system.
inline bool tts_available() {
    return std::system("espeak-ng --version >/dev/null 2>&1") == 0 ||
           std::system("say --version >/dev/null 2>&1") == 0;
}

// Check if STT/whisper is available.
inline bool stt_available() {
    return std::system("whisper --help >/dev/null 2>&1") == 0;
}

} // namespace ac_machine_audio

// Flat API
inline bool   maudio_speak(const std::string& text, int rate = 175)  { return ac_machine_audio::speak(text, rate); }
inline void   maudio_speak_async(const std::string& text)             { ac_machine_audio::speak_async(text); }
inline std::string maudio_listen(int timeout_ms = 5000)               { return ac_machine_audio::listen(timeout_ms); }
inline bool   maudio_tts_ok()                                         { return ac_machine_audio::tts_available(); }
inline bool   maudio_stt_ok()                                         { return ac_machine_audio::stt_available(); }
