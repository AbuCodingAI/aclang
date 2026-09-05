#ifndef AC_CAMERA_C_H
#define AC_CAMERA_C_H
#ifdef __cplusplus
extern "C" {
#endif
int ac_camera_init(void);
int ac_camera_capture(const char* filename);
int ac_camera_capture_latest(const char* filename);
int ac_camera_capture_first(const char* filename);
void ac_camera_release(void);
void ac_sidebar_config(const char* key, const char* value);
void ac_sidebar_setregion(const char* region);
void ac_sidebar_setinteractive(int value);
void ac_sidebar_display(const char* message);
const char* ac_sidebar_ask(const char* prompt);
const char* ac_sidebar_getinput(void);
void ac_screen_setmode(const char* mode);
void ac_screen_update(void);
#ifdef __cplusplus
}
#endif

/* Call-form shims: the AC compiler emits camera_x(...)/sidebar_x(...)/screen_x(...) on
   the C backend (the C++ backend instead gets a real AC::Camera/SidebarConsole/Screen
   object via camera_wrapper.hpp) — same pattern as regex_c.h's own shim block. */
#ifndef __cplusplus
#define camera_init             ac_camera_init
#define camera_capture          ac_camera_capture
#define camera_capture_latest   ac_camera_capture_latest
#define camera_capture_first    ac_camera_capture_first
#define camera_release          ac_camera_release
#define sidebar_config          ac_sidebar_config
#define sidebar_setregion       ac_sidebar_setregion
#define sidebar_setinteractive  ac_sidebar_setinteractive
#define sidebar_display         ac_sidebar_display
#define sidebar_ask             ac_sidebar_ask
#define sidebar_getinput        ac_sidebar_getinput
#define screen_setmode          ac_screen_setmode
#define screen_update           ac_screen_update
#endif

#endif
