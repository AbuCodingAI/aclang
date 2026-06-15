#ifndef AC_CAMERA_WRAPPER_HPP
#define AC_CAMERA_WRAPPER_HPP

// Wrapper for AC camera library to match AC syntax more closely
// This file provides a simpler interface that matches the AC camera.ac example

#include "camera.hpp"
#include <iostream>

using namespace AC;

// Global instances matching AC's global scope
// These are declared in camera.hpp and defined in camera.cpp

// Helper functions for AC-style syntax

/**
 * Initialize camera with default device
 */
inline void initCamera(Camera& cam) {
    if (!cam.init()) {
        std::cerr << "Failed to initialize camera" << std::endl;
    }
}

/**
 * Take a picture and save to file (AC-style: picture())
 */
inline bool picture(Camera& cam, const std::string& filename) {
    return cam.capture(filename);
}

/**
 * Display message in sidebar (AC-style: sidebar.display $message$)
 */
inline void display(SidebarConsole& console, const std::string& message) {
    console.display(message);
}

/**
 * Ask for user input (AC-style: sidebar.ask $prompt$)
 */
inline std::string ask(SidebarConsole& console, const std::string& prompt) {
    return console.ask(prompt);
}

/**
 * Get user input (AC-style: sidebar.getinput)
 */
inline std::string getInput(SidebarConsole& console) {
    return console.getInput();
}

/**
 * Configure sidebar (AC-style: sidebar.config item=console)
 */
inline void config(SidebarConsole& console, const std::string& key, const std::string& value) {
    console.config(key, value);
}

/**
 * Set sidebar region (AC-style: sidebar.region = left)
 */
inline void setRegion(SidebarConsole& console, const std::string& region) {
    console.setRegion(region);
}

/**
 * Set sidebar interactive (AC-style: sidebar.interactive = true)
 */
inline void setInteractive(SidebarConsole& console, bool value) {
    console.setInteractive(value);
}

/**
 * Update screen from camera (AC-style: WebCam.view)
 */
inline void updateScreen(Screen& screen, Camera& cam) {
    screen.updateFrame(cam);
}

/**
 * Set screen mode (AC-style: Background.config.mode=livefeed)
 */
inline void setScreenMode(Screen& screen, const std::string& mode) {
    screen.setMode(mode);
}

#endif // AC_CAMERA_WRAPPER_HPP
