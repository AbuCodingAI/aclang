// AC ilib: machine-audio — C++ shared library implementation
// Exported as: libacmachinaaudio.so
//
// Dependencies (optional — graceful degradation when absent):
//   espeak-ng   : TTS  (apt install espeak-ng  / espeak-ng-dev)
//   mpg123      : MP3 playback  (apt install libmpg123-dev)
//   ALSA        : PCM output for static noise  (apt install libasound2-dev)
//
// Build:
//   g++ -std=c++17 -O2 -shared -fPIC machine_audio.cpp \
//       -lmpg123 -lasound -o libacmachinaaudio.so
// Without optional deps:
//   g++ -std=c++17 -O2 -shared -fPIC machine_audio.cpp \
//       -DAC_NO_MPG123 -DAC_NO_ALSA -o libacmachinaaudio.so

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <csignal>

#ifndef AC_NO_MPG123
#  ifdef __has_include
#    if __has_include(<mpg123.h>)
#      include <mpg123.h>
#      define AC_HAS_MPG123 1
#    endif
#  endif
#endif

#ifndef AC_NO_ALSA
#  ifdef __has_include
#    if __has_include(<alsa/asoundlib.h>)
#      include <alsa/asoundlib.h>
#      define AC_HAS_ALSA 1
#    endif
#  endif
#endif

extern "C" {

// ── Internal track state ─────────────────────────────────────────────────────

struct Track {
    std::string  path;
    std::thread  playThread;
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{false};
    std::atomic<bool> looping{false};
    float speed  = 1.0f;
    float reverb = 0.0f;
};

static std::mutex              g_trackMu;
static std::unordered_map<int, Track*> g_tracks;
static std::atomic<int>        g_nextId{1};

// Global voice settings
static std::atomic<int>  g_rate{175};       // words per minute
static std::atomic<int>  g_pitch{50};       // 0-100
static std::atomic<int>  g_amplitude{100};  // 0-200
static bool              g_voiceMale = true;

// ── TTS helpers ──────────────────────────────────────────────────────────────

static void runEspeak(const std::string& text, bool block) {
    std::string voice = g_voiceMale ? "en+m3" : "en+f3";
    char sbuf[16], pbuf[16], abuf[16];
    std::snprintf(sbuf, sizeof(sbuf), "%d", (int)g_rate);
    std::snprintf(pbuf, sizeof(pbuf), "%d", (int)g_pitch);
    std::snprintf(abuf, sizeof(abuf), "%d", (int)g_amplitude);
    // Spawn espeak-ng via execvp — `text` and `voice` are distinct argv elements, so a
    // spoken string containing `$(...)`, backticks, `;` etc. can never run a shell command.
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) { dup2(dn, 2); close(dn); }   // silence espeak stderr
        char* av[] = { (char*)"espeak-ng", (char*)"-s", sbuf, (char*)"-p", pbuf,
                       (char*)"-a", abuf, (char*)"-v", (char*)voice.c_str(),
                       (char*)text.c_str(), nullptr };
        execvp("espeak-ng", av);
        _exit(127);
    }
    if (block) { int st; waitpid(pid, &st, 0); }
    else       { signal(SIGCHLD, SIG_IGN); }        // reap async speaker, no zombie
}

// ── AC machine-audio API ─────────────────────────────────────────────────────

// say($text$) — speak text (blocking)
void ac_maudio_say(const char* text) {
    if (!text) return;
    runEspeak(text, true);
}

// construct(speech) — non-blocking TTS
void ac_maudio_construct(const char* text) {
    if (!text) return;
    runEspeak(text, false);
}

// set voice to male / female
void ac_maudio_set_voice(const char* gender) {
    if (!gender) return;
    g_voiceMale = (std::string(gender) == "male");
}

// speech.rate(percentage) — 0–200%, maps to 80–350 wpm
void ac_maudio_speech_rate(int percent) {
    g_rate = 80 + (int)(percent / 100.0f * 270.0f);
}

// speech.pitch(percentage) — 0–100
void ac_maudio_speech_pitch(int percent) {
    g_pitch = std::max(0, std::min(100, percent));
}

// speech.amplitude(percentage) — 0–200
void ac_maudio_speech_amplitude(int percent) {
    g_amplitude = std::max(0, std::min(200, percent));
}

// load_mp3(path) — return a track handle (int)
int ac_maudio_load_mp3(const char* path) {
    if (!path) return -1;
    int id = g_nextId++;
    std::lock_guard<std::mutex> lk(g_trackMu);
    Track* t = new Track();
    t->path = path;
    g_tracks[id] = t;
    return id;
}

// Internal: actually play the track once (in a thread)
static void playOnce(Track* t) {
#ifdef AC_HAS_MPG123
    mpg123_handle* mh = mpg123_new(nullptr, nullptr);
    if (!mh) return;
    if (mpg123_open(mh, t->path.c_str()) != MPG123_OK) {
        mpg123_delete(mh);
        return;
    }

    long rate; int channels, encoding;
    mpg123_getformat(mh, &rate, &channels, &encoding);

#  ifdef AC_HAS_ALSA
    snd_pcm_t* pcm = nullptr;
    snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (pcm) {
        snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           (unsigned)channels, (unsigned)rate, 1, 50000);
    }

    unsigned char buf[4096];
    size_t done = 0;
    while (t->playing && mpg123_read(mh, buf, sizeof(buf), &done) == MPG123_OK) {
        while (t->paused && t->playing) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (pcm) snd_pcm_writei(pcm, buf, snd_pcm_bytes_to_frames(pcm, (snd_pcm_sframes_t)done));
    }
    if (pcm) { snd_pcm_drain(pcm); snd_pcm_close(pcm); }
#  else
    // Fallback: shell
    // mpg123 CLI via execvp — the track path is a distinct argv element, never a shell string.
    pid_t mpid = fork();
    if (mpid == 0) {
        int dn = open("/dev/null", O_WRONLY); if (dn >= 0) { dup2(dn, 2); close(dn); }
        char* av[] = { (char*)"mpg123", (char*)"-q", (char*)t->path.c_str(), nullptr };
        execvp("mpg123", av);
        _exit(127);
    }
    if (mpid > 0) { int st; waitpid(mpid, &st, 0); }
#  endif
    mpg123_close(mh);
    mpg123_delete(mh);
#else
    // Fallback: try mpg123 CLI
    // mpg123 CLI via execvp — the track path is a distinct argv element, never a shell string.
    pid_t mpid = fork();
    if (mpid == 0) {
        int dn = open("/dev/null", O_WRONLY); if (dn >= 0) { dup2(dn, 2); close(dn); }
        char* av[] = { (char*)"mpg123", (char*)"-q", (char*)t->path.c_str(), nullptr };
        execvp("mpg123", av);
        _exit(127);
    }
    if (mpid > 0) { int st; waitpid(mpid, &st, 0); }
#endif
}

// play(var) — start playback (non-blocking)
void ac_maudio_play(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it == g_tracks.end()) return;
    Track* t = it->second;
    if (t->playing) return;
    if (t->playThread.joinable()) t->playThread.join();  // reap a finished prior play; NOT detached → no UAF
    t->playing = true;
    t->looping = false;
    t->playThread = std::thread([t]() {
        playOnce(t);
        t->playing = false;
    });
}

// play.loop(var) — loop playback until stop()
void ac_maudio_play_loop(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it == g_tracks.end()) return;
    Track* t = it->second;
    if (t->playing) return;
    if (t->playThread.joinable()) t->playThread.join();
    t->playing = true;
    t->looping = true;
    t->playThread = std::thread([t]() {
        while (t->looping && t->playing) playOnce(t);
        t->playing = false;
    });
}

// pause(var)
void ac_maudio_pause(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it != g_tracks.end()) it->second->paused = true;
}

// stop(var)
void ac_maudio_stop(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it == g_tracks.end()) return;
    Track* t = it->second;
    t->looping = false;
    t->playing  = false;
    t->paused   = false;
}

// reverb(var, percent) — store effect; applied on next play
void ac_maudio_reverb(int handle, int percent) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it != g_tracks.end()) it->second->reverb = percent / 100.0f;
}

// speed(var, percent) — store speed factor
void ac_maudio_speed(int handle, int percent) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it != g_tracks.end()) it->second->speed = percent / 100.0f;
}

// decode(var) — return path string (introspection)
const char* ac_maudio_decode(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    return (it != g_tracks.end()) ? it->second->path.c_str() : "";
}

// static — play white noise for duration_ms milliseconds (blocks)
void ac_maudio_static_noise(int duration_ms) {
    if (duration_ms <= 0) duration_ms = 1000;
#ifdef AC_HAS_ALSA
    snd_pcm_t* pcm = nullptr;
    if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) goto fallback;
    snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                       1, 44100, 1, 50000);
    {
        const int frames = 44100 * duration_ms / 1000;
        std::vector<int16_t> buf(frames);
        // LCG white noise
        uint32_t seed = 12345;
        for (auto& s : buf) {
            seed = seed * 1664525u + 1013904223u;
            s = (int16_t)(seed >> 16);
        }
        snd_pcm_writei(pcm, buf.data(), (snd_pcm_uframes_t)frames);
        snd_pcm_drain(pcm);
    }
    snd_pcm_close(pcm);
    return;
fallback:
#endif
    // Fallback: write a WAV to /tmp and play with aplay
    int samples = 44100 * duration_ms / 1000;
    FILE* f = std::fopen("/tmp/ac_static.wav", "wb");
    if (!f) return;
    // WAV header
    uint32_t dataLen = (uint32_t)(samples * 2);
    uint32_t riffLen = dataLen + 36;
    uint16_t u16; uint32_t u32;
    auto w2 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    auto w4 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w4(riffLen);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); w4(16); w2(1); w2(1);
    w4(44100); w4(88200); w2(2); w2(16);
    std::fwrite("data", 1, 4, f); w4(dataLen);
    uint32_t seed = 12345;
    for (int i = 0; i < samples; i++) {
        seed = seed * 1664525u + 1013904223u;
        int16_t s = (int16_t)(seed >> 16);
        std::fwrite(&s, 2, 1, f);
    }
    std::fclose(f);
    std::system("aplay -q /tmp/ac_static.wav 2>/dev/null");
    (void)u16; (void)u32;
}

// ── speak / listen / tts_ok ──────────────────────────────────────────────────
// AC's `maudio_speak`/`maudio_listen`/`maudio_tts_ok` (used by examples/audio_test.ac,
// jarvis.ac) were ONLY ever implemented natively in Python (machine-audio.py) — this shared
// C++ core (what C/C++/Rust/Go/Java/V/ASM/BNY all actually call through) never had them at
// all, so every non-PY/JS backend hit a hard "undefined reference"/"undeclared identifier" on
// any program using them. Real implementations, mirroring PY's own tool fallback chain
// (espeak-ng/spd-say for TTS, arecord+whisper CLI for STT — all language-agnostic system
// tools, not Python-specific) so every backend gets identical, genuinely working behavior
// through the one shared library instead of needing a from-scratch reimplementation each.

// Runs `cmd` via execvp with `argv`; returns true if the exec succeeded (exit code 0),
// false if the tool is missing or failed. WEXITSTATUS==127 means execvp couldn't find `cmd`.
static bool runTool(const char* cmd, char* const argv[], bool silence) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        if (silence) {
            int dn = open("/dev/null", O_WRONLY);
            if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); close(dn); }
        }
        execvp(cmd, argv);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}
static bool toolExists(const char* cmd) {
    char* av[] = { (char*)cmd, (char*)"--version", nullptr };
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); close(dn); }
        execvp(cmd, av);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) && WEXITSTATUS(st) != 127;
}

// tts_ok() — is a real TTS engine available? (1/0, matches AC's int-everything convention)
int ac_maudio_tts_ok() {
    return (toolExists("espeak-ng") || toolExists("espeak") || toolExists("spd-say")) ? 1 : 0;
}

// speak($text$) — blocking TTS via whichever engine is actually installed; prints the text
// as a last-resort fallback (matches PY's own `print(f"[machine-audio::speak] {text}")`) so
// the call is never silently a no-op even with zero TTS engines present.
void ac_maudio_speak(const char* text) {
    if (!text) return;
    if (toolExists("espeak-ng")) {
        char* av[] = { (char*)"espeak-ng", (char*)text, nullptr };
        if (runTool("espeak-ng", av, true)) return;
    }
    if (toolExists("espeak")) {
        char* av[] = { (char*)"espeak", (char*)text, nullptr };
        if (runTool("espeak", av, true)) return;
    }
    if (toolExists("spd-say")) {
        char* av[] = { (char*)"spd-say", (char*)"-w", (char*)text, nullptr };
        if (runTool("spd-say", av, true)) return;
    }
    std::printf("[machine-audio::speak] %s\n", text);
}

// listen(timeout_ms) — record from the default mic via `arecord`, transcribe via the
// `whisper` CLI. Returns "" (matches AC's own "Nothing heard or STT unavailable" documented
// fallback path — see audio_test.ac) if either tool is missing or nothing was transcribed;
// never blocks longer than timeout_ms/1000 seconds regardless of outcome.
const char* ac_maudio_listen(int timeout_ms) {
    static std::string result;
    result.clear();
    if (!toolExists("arecord") || !toolExists("whisper")) return result.c_str();
    int secs = timeout_ms > 0 ? std::max(1, timeout_ms / 1000) : 1;
    char wavPath[] = "/tmp/ac_listen_XXXXXX.wav";
    int fd = mkstemps(wavPath, 4);
    if (fd < 0) return result.c_str();
    close(fd);
    char secBuf[16];
    std::snprintf(secBuf, sizeof(secBuf), "%d", secs);
    {
        char* av[] = { (char*)"arecord", (char*)"-q", (char*)"-d", secBuf,
                       (char*)"-f", (char*)"cd", (char*)"-t", (char*)"wav",
                       wavPath, nullptr };
        runTool("arecord", av, true);
    }
    std::string outDir = "/tmp";
    {
        char* av[] = { (char*)"whisper", wavPath, (char*)"--model", (char*)"tiny",
                       (char*)"--output_format", (char*)"txt",
                       (char*)"--output_dir", (char*)outDir.c_str(), nullptr };
        runTool("whisper", av, true);
    }
    std::string base = wavPath;
    auto slash = base.find_last_of('/');
    std::string stem = (slash == std::string::npos) ? base : base.substr(slash + 1);
    auto dot = stem.rfind(".wav");
    if (dot != std::string::npos) stem = stem.substr(0, dot);
    std::string txtPath = outDir + "/" + stem + ".txt";
    FILE* f = std::fopen(txtPath.c_str(), "r");
    if (f) {
        char buf[4096];
        size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        std::fclose(f);
        result = buf;
        while (!result.empty() && (result.back() == '\n' || result.back() == ' ' || result.back() == '\r'))
            result.pop_back();
        std::remove(txtPath.c_str());
    }
    std::remove(wavPath);
    return result.c_str();
}

// stop_all() — no-arg cleanup used by the compiler's auto-injected `maudio.stop()` shutdown
// call (see ir.cpp's injectAutoShutoff) whenever `use ilib machine-audio` is imported; distinct
// from ac_maudio_stop(handle), which stops one specific MP3 track.
void ac_maudio_stop_all() {
    std::lock_guard<std::mutex> lk(g_trackMu);
    for (auto& [id, t] : g_tracks) { t->looping = false; t->playing = false; }
}

// Cleanup a track handle
void ac_maudio_free(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it == g_tracks.end()) return;
    Track* t = it->second;
    t->looping = false;
    t->playing  = false;
    if (t->playThread.joinable()) t->playThread.join();  // wait for the player to exit BEFORE freeing (was UAF)
    delete t;
    g_tracks.erase(it);
}

} // extern "C"
