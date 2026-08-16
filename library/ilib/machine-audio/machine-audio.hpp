// AC ilib: machine-audio — Speech-to-Text and Text-to-Speech
// use ilib machine-audio
//
// Backend selection:
//   AC_MAUDIO_ESPEAK  — TTS via eSpeak-NG (Linux/macOS/Windows, open-source)
//   AC_MAUDIO_POCKETSPHINX — STT via PocketSphinx (offline, open-source)
//   AC_MAUDIO_STUB    — compile-only stub (default when no backend defined)
#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#if defined(AC_MAUDIO_ESPEAK)
#  include <espeak-ng/speak_lib.h>
#endif
#if defined(AC_MAUDIO_POCKETSPHINX)
#  include <pocketsphinx.h>
#endif

namespace ac_machine_audio {

// Spawn `path` via fork()/execvp() with `args` as distinct argv elements — NO shell,
// so text containing '$(...)', backticks, ';', quotes etc. is never interpreted as a
// command. Waits for completion; returns true iff the child exited with status 0.
inline bool run_argv(const char* path, const std::vector<std::string>& args, bool suppress_output = true) {
    pid_t pid = fork();
    if (pid == 0) {
        if (suppress_output) {
            std::freopen("/dev/null", "w", stdout);
            std::freopen("/dev/null", "w", stderr);
        }
        std::vector<char*> av;
        av.push_back(const_cast<char*>(path));
        for (const auto& a : args) av.push_back(const_cast<char*>(a.c_str()));
        av.push_back(nullptr);
        execvp(path, av.data());
        _exit(127);
    }
    if (pid < 0) return false;
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

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
    // Fallback: run TTS directly via fork()/execvp() — the text is a single argv
    // element, never concatenated into a shell command string.
    (void)rate; (void)voice;
    if (run_argv("espeak-ng", {text})) return true;
    if (run_argv("say", {text})) return true;
    std::printf("[TTS] %s\n", text.c_str());
    return true;
#endif
}

// Speak asynchronously (non-blocking, fire-and-forget).
inline void speak_async(const std::string& text, int rate = 175, const std::string& voice = "en") {
    (void)rate; (void)voice;
    pid_t pid = fork();
    if (pid == 0) {
        // Detach into our own session so the parent returning doesn't affect us.
        setsid();
        std::freopen("/dev/null", "w", stdout);
        std::freopen("/dev/null", "w", stderr);
        execlp("espeak-ng", "espeak-ng", text.c_str(), (char*)nullptr);
        execlp("say", "say", text.c_str(), (char*)nullptr);
        _exit(127);
    }
    // Fire-and-forget: don't wait for the child.
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
    // Fallback: try whisper.cpp CLI if present. Both steps run via fork()/execvp()
    // (argv arrays, no shell) instead of system()/popen() with a concatenated string.
    std::string tmp = "/tmp/ac_stt_capture.wav";
    std::string out_base = "/tmp/ac_stt_out";
    // Record for (timeout_ms/1000) seconds using arecord (Linux)
    int secs = std::max(1, timeout_ms / 1000);
    run_argv("arecord", {"-d", std::to_string(secs), "-f", "cd", "-t", "wav", tmp});
    // Transcribe with whisper.cpp CLI if available
    if (!run_argv("whisper", {"--model", "tiny", tmp, "--output-txt", "--output-file", out_base})) {
        return "";
    }
    FILE* f = std::fopen((out_base + ".txt").c_str(), "r");
    if (!f) return "";
    char buf[4096] = {};
    size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    return std::string(buf, n);
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
inline void   maudio_stop()                                           {}   // no active-track model here to stop

// AC's actual calling convention for this ilib is the DOTTED form (`maudio.speak(...)`,
// matching machine-audio.py's own `class maudio` staticmethod wrapper and every other native
// ilib implementation) — including the compiler's auto-injected `maudio.stop()` shutdown call
// (see ir.cpp's injectAutoShutoff, fires whenever `use ilib machine-audio` is imported).
inline bool _ac_maudio_speak1(const std::string& text) { return ac_machine_audio::speak(text); }
struct _ac_maudio_ns {
    bool (*speak)(const std::string&)       = _ac_maudio_speak1;
    std::string (*listen)(int)              = ac_machine_audio::listen;
    bool (*tts_ok)()                        = ac_machine_audio::tts_available;
    bool (*stt_ok)()                        = ac_machine_audio::stt_available;
    void (*stop)()                          = maudio_stop;
};
static _ac_maudio_ns maudio;
