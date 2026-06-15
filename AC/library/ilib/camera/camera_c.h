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
#endif
