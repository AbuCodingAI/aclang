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

#ifdef __cplusplus
}
#endif
