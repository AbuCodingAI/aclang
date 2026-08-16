#pragma once
/* AC ilib: machine-audio — C API for libacmachinaaudio.so */
#ifdef __cplusplus
extern "C" {
#endif

/* TTS */
void        ac_maudio_say(const char* text);
void        ac_maudio_construct(const char* text);
void        ac_maudio_set_voice(const char* gender);   /* "male" | "female" */
void        ac_maudio_speech_rate(int percent);         /* 0-200 */
void        ac_maudio_speech_pitch(int percent);        /* 0-100 */
void        ac_maudio_speech_amplitude(int percent);    /* 0-200 */

/* MP3 playback */
int         ac_maudio_load_mp3(const char* path);      /* returns track handle */
void        ac_maudio_play(int handle);
void        ac_maudio_play_loop(int handle);
void        ac_maudio_pause(int handle);
void        ac_maudio_stop(int handle);
void        ac_maudio_reverb(int handle, int percent);
void        ac_maudio_speed(int handle, int percent);
const char* ac_maudio_decode(int handle);
void        ac_maudio_free(int handle);

/* White noise */
void        ac_maudio_static_noise(int duration_ms);

/* Simplified speak/listen/availability — used by examples/audio_test.ac, jarvis.ac */
int         ac_maudio_tts_ok(void);
void        ac_maudio_speak(const char* text);
const char* ac_maudio_listen(int timeout_ms);
void        ac_maudio_stop_all(void);   /* no-arg cleanup — see injectAutoShutoff's `maudio.stop()` */

#ifdef __cplusplus
}
#endif

/* ── Call-form shims ─────────────────────────────────────────────────────────
   AC's own correct calling convention for this ilib is the DOTTED form —
   `maudio.speak(...)`, `maudio.listen(...)`, `maudio.tts_ok()`, and the compiler's own
   auto-injected cleanup call `maudio.stop()` (see ir.cpp's injectAutoShutoff, fired whenever
   `use ilib machine-audio` is imported) — matching every other native ilib implementation
   (see machine-audio.py's own `class maudio` staticmethod wrapper). C++ needs an actual
   object for that dot syntax to resolve against; C-only code (no dotCallSyntax) instead gets
   the underscore form via #define, same as string-cheese_c.h's own such block. */
#define maudio_speak    ac_maudio_speak
#define maudio_listen   ac_maudio_listen
#define maudio_tts_ok   ac_maudio_tts_ok
#define maudio_stop     ac_maudio_stop_all
#ifdef __cplusplus
struct _ac_maudio_ns {
    void        (*speak)(const char*)  = ac_maudio_speak;
    const char* (*listen)(int)         = ac_maudio_listen;
    int         (*tts_ok)(void)        = ac_maudio_tts_ok;
    void        (*stop)(void)          = ac_maudio_stop_all;
};
static _ac_maudio_ns maudio;
#endif
