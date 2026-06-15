#include "camera.hpp"
#include <cstring>
#include <string>

namespace AC {
    extern Camera WebCam;
    extern Camera latestFrame;
    extern Camera firstFrame;
    extern SidebarConsole sidebar;
    extern Screen Background;
}

static std::string g_lastInput;

extern "C" {

// Camera functions
int ac_camera_init() {
    return AC::WebCam.init() ? 1 : 0;
}

int ac_camera_capture(const char* filename) {
    return AC::WebCam.capture(filename) ? 1 : 0;
}

int ac_camera_capture_latest(const char* filename) {
    return AC::latestFrame.capture(filename) ? 1 : 0;
}

int ac_camera_capture_first(const char* filename) {
    return AC::firstFrame.capture(filename) ? 1 : 0;
}

void ac_camera_release() {
    AC::WebCam.release();
}

// Sidebar functions
void ac_sidebar_config(const char* key, const char* value) {
    AC::sidebar.config(key, value);
}

void ac_sidebar_setregion(const char* region) {
    AC::sidebar.setRegion(region);
}

void ac_sidebar_setinteractive(int value) {
    AC::sidebar.setInteractive(value != 0);
}

void ac_sidebar_display(const char* message) {
    AC::sidebar.display(message);
}

const char* ac_sidebar_ask(const char* prompt) {
    g_lastInput = AC::sidebar.ask(prompt);
    return g_lastInput.c_str();
}

const char* ac_sidebar_getinput() {
    g_lastInput = AC::sidebar.getInput();
    return g_lastInput.c_str();
}

// Screen functions
void ac_screen_setmode(const char* mode) {
    AC::Background.setMode(mode);
}

void ac_screen_update() {
#if HAVE_OPENCV
    AC::Background.updateFrame(AC::WebCam);
#endif
}

} // extern "C"
