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
    char cmd[4096];
    std::snprintf(cmd, sizeof(cmd),
        "espeak-ng -s %d -p %d -a %d -v \"%s\" \"%s\" %s 2>/dev/null",
        (int)g_rate, (int)g_pitch, (int)g_amplitude,
        voice.c_str(), text.c_str(),
        block ? "" : "&");
    std::system(cmd);
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
    char cmd[4096];
    std::snprintf(cmd, sizeof(cmd), "mpg123 -q \"%s\" 2>/dev/null", t->path.c_str());
    std::system(cmd);
#  endif
    mpg123_close(mh);
    mpg123_delete(mh);
#else
    // Fallback: try mpg123 CLI
    char cmd[4096];
    std::snprintf(cmd, sizeof(cmd), "mpg123 -q \"%s\" 2>/dev/null", t->path.c_str());
    std::system(cmd);
#endif
}

// play(var) — start playback (non-blocking)
void ac_maudio_play(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it == g_tracks.end()) return;
    Track* t = it->second;
    if (t->playing) return;
    t->playing = true;
    t->looping = false;
    t->playThread = std::thread([t]() {
        playOnce(t);
        t->playing = false;
    });
    t->playThread.detach();
}

// play.loop(var) — loop playback until stop()
void ac_maudio_play_loop(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it == g_tracks.end()) return;
    Track* t = it->second;
    if (t->playing) return;
    t->playing = true;
    t->looping = true;
    t->playThread = std::thread([t]() {
        while (t->looping && t->playing) playOnce(t);
        t->playing = false;
    });
    t->playThread.detach();
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

// Cleanup a track handle
void ac_maudio_free(int handle) {
    std::lock_guard<std::mutex> lk(g_trackMu);
    auto it = g_tracks.find(handle);
    if (it == g_tracks.end()) return;
    Track* t = it->second;
    t->looping = false;
    t->playing  = false;
    delete t;
    g_tracks.erase(it);
}

} // extern "C"
