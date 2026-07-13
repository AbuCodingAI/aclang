#ifndef AC_CAMERA_HPP
#define AC_CAMERA_HPP

// Auto-detect OpenCV if not explicitly set
#ifndef HAVE_OPENCV
#  ifdef __has_include
#    if __has_include(<opencv2/opencv.hpp>)
#      define HAVE_OPENCV 1
#    else
#      define HAVE_OPENCV 0
#    endif
#  else
#    define HAVE_OPENCV 0
#  endif
#endif

#if HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif
#include <string>
#include <functional>
#include <algorithm>
#include <vector>

namespace AC {

#if !HAVE_OPENCV
// Stub mode — OpenCV not available. All operations are no-ops that return safe defaults.
class Camera {
public:
    Camera() {}
    bool init(int = 0) { return false; }
    void release() {}
    bool capture(const std::string&) { return false; }
    bool captureLatest() { return false; }
    bool captureFirst() { return false; }
};

class SidebarConsole {
public:
    void config(const std::string&, const std::string&) {}
    void setRegion(const std::string&) {}
    void setInteractive(bool) {}
    void display(const std::string& msg) { fprintf(stderr, "[sidebar] %s\n", msg.c_str()); }
    std::string ask(const std::string& prompt) { fprintf(stderr, "[sidebar.ask] %s\n", prompt.c_str()); return ""; }
    std::string getInput() { return ""; }
    std::function<std::string(std::string)> inputCallback;
};

class Screen {
public:
    Screen() {}
    void setMode(const std::string&) {}
    void update() {}
};

#else // HAVE_OPENCV — full implementation below

/**
 * Camera class for capturing video frames from webcam
 */
class Camera {
private:
    cv::VideoCapture cap;
    bool initialized;

public:
    Camera() : initialized(false) {}
    
    ~Camera() {
        if (cap.isOpened()) {
            cap.release();
        }
    }
    
    /**
     * Initialize camera with specified device ID
     * @param device_id Camera device ID (default 0 for default camera)
     * @return true if initialization successful
     */
    bool init(int device_id = 0) {
        if (cap.isOpened()) {
            cap.release();
        }
        cap.open(device_id);
        initialized = cap.isOpened();
        return initialized;
    }
    
    /**
     * Check if camera is initialized
     * @return true if camera is ready
     */
    bool isInitialized() const {
        return initialized;
    }
    
    /**
     * Capture a single frame
     * @param frame Output frame
     * @return true if frame captured successfully
     */
    bool capture(cv::Mat& frame) {
        if (!initialized) return false;
        cap >> frame;
        return !frame.empty();
    }
    
    /**
     * Capture and save a frame to file
     * @param filename Output filename
     * @return true if saved successfully
     */
    bool capture(const std::string& filename) {
        if (!initialized) return false;
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) return false;
        return cv::imwrite(filename, frame);
    }
    
    /**
     * Get current frame (for motion detection)
     * @return Current frame Mat
     */
    cv::Mat& getFrame() {
        return currentFrame;
    }
    
    cv::Mat currentFrame;
    
    /**
     * Release camera resources
     */
    void release() {
        if (cap.isOpened()) {
            cap.release();
        }
        initialized = false;
    }
};

/**
 * Motion detection using frame comparison
 */
class MotionDetector {
private:
    cv::Mat referenceFrame;
    bool hasReference;
    double sensitivity;

public:
    MotionDetector() : hasReference(false), sensitivity(0.1) {}
    
    /**
     * Set reference frame for motion detection
     * @param frame Reference frame (background)
     */
    void setReference(const cv::Mat& frame) {
        if (!frame.empty()) {
            frame.copyTo(referenceFrame);
            hasReference = true;
        }
    }
    
    /**
     * Set reference frame from file
     * @param filename Path to reference image
     * @return true if reference loaded successfully
     */
    bool setReference(const std::string& filename) {
        referenceFrame = cv::imread(filename);
        hasReference = !referenceFrame.empty();
        return hasReference;
    }
    
    /**
     * Check if motion is detected between current frame and reference
     * @param currentFrame Current frame to check
     * @return true if motion detected
     */
    bool detectMotion(const cv::Mat& currentFrame) {
        if (!hasReference || currentFrame.empty()) return false;
        
        if (currentFrame.size() != referenceFrame.size()) {
            cv::Mat resized;
            cv::resize(currentFrame, resized, referenceFrame.size());
            return detectMotion(resized);
        }
        
        cv::Mat diff;
        cv::absdiff(referenceFrame, currentFrame, diff);
        
        cv::Mat gray;
        cv::cvtColor(diff, gray, cv::COLOR_BGR2GRAY);
        
        cv::Mat thresholded;
        cv::threshold(gray, thresholded, 30, 255, cv::THRESH_BINARY);
        
        int nonZeroCount = cv::countNonZero(thresholded);
        int totalPixels = thresholded.rows * thresholded.cols;
        double ratio = (double)nonZeroCount / totalPixels;
        
        return ratio > sensitivity;
    }
    
    /**
     * Set motion detection sensitivity (0.0 to 1.0)
     * @param value Sensitivity threshold
     */
    void setSensitivity(double value) {
        sensitivity = std::max(0.0, std::min(1.0, value));
    }
    
    /**
     * Check if reference frame is set
     * @return true if reference exists
     */
    bool hasReferenceFrame() const {
        return hasReference;
    }
};

/**
 * GUI Sidebar Console component
 */
class SidebarConsole {
private:
    std::string position;  // "left", "right", "top", "bottom"
    bool interactive;
    bool visible;
    std::string title;
    std::vector<std::string> messages;
    std::function<void(const std::string&)> inputCallback;
    
public:
    SidebarConsole() : interactive(false), visible(true), position("left") {}
    
    /**
     * Configure sidebar properties
     * @param key Property key (e.g., "item", "region", "interactive")
     * @param value Property value
     */
    void config(const std::string& key, const std::string& value) {
        if (key == "item" && value == "console") {
            // Configure as console item
        } else if (key == "region") {
            position = value;
        } else if (key == "interactive") {
            interactive = (value == "true" || value == "1");
        }
    }
    
    /**
     * Set sidebar position
     * @param pos Position ("left", "right", "top", "bottom")
     */
    void setRegion(const std::string& pos) {
        position = pos;
    }
    
    /**
     * Set interactive mode
     * @param value true for interactive
     */
    void setInteractive(bool value) {
        interactive = value;
    }
    
    /**
     * Display a message in the sidebar
     * @param message Message text
     */
    void display(const std::string& message) {
        messages.push_back(message);
        // In a real implementation, this would update the GUI
    }
    
    /**
     * Ask for user input (blocks until input received)
     * @param prompt Prompt text
     * @return User input
     */
    std::string ask(const std::string& prompt) {
        if (interactive && inputCallback) {
            inputCallback(prompt);
        }
        return "";
    }

    /**
     * Get user input from sidebar
     * @return User input string
     */
    std::string getInput() {
        if (interactive && inputCallback) {
            inputCallback("Enter command:");
        }
        return "";
    }
    
    /**
     * Set input callback function
     * @param callback Function to call when input is requested
     */
    void setInputCallback(std::function<std::string(const std::string&)> callback) {
        inputCallback = callback;
    }
    
    /**
     * Get all messages
     * @return Vector of messages
     */
    std::vector<std::string> getMessages() const {
        return messages;
    }
    
    /**
     * Clear all messages
     */
    void clearMessages() {
        messages.clear();
    }
    
    /**
     * Check if sidebar is visible
     * @return true if visible
     */
    bool isVisible() const {
        return visible;
    }
    
    /**
     * Set visibility
     * @param value true to show, false to hide
     */
    void setVisible(bool value) {
        visible = value;
    }
};

/**
 * Screen/Display class for showing camera feed
 */
class Screen {
private:
    std::string mode;  // "livefeed", "static", etc.
    cv::Mat currentFrame;
    bool fullScreen;
    
public:
    Screen() : mode("livefeed"), fullScreen(false) {}
    
    /**
     * Set display mode
     * @param m Mode string
     */
    void setMode(const std::string& m) {
        mode = m;
    }
    
    /**
     * Display current frame
     * @param windowName Name of display window
     */
    void display(const std::string& windowName = "Camera") {
        if (!currentFrame.empty()) {
            cv::imshow(windowName, currentFrame);
        }
    }
    
    /**
     * Update frame from camera
     * @param camera Camera instance
     * @return true if frame updated
     */
    bool updateFrame(Camera& camera) {
        return camera.capture(currentFrame);
    }
    
    /**
     * Set frame directly
     * @param frame Frame to display
     */
    void setFrame(const cv::Mat& frame) {
        frame.copyTo(currentFrame);
    }
    
    /**
     * Check if in livefeed mode
     * @return true if livefeed
     */
    bool isLiveFeed() const {
        return mode == "livefeed";
    }
    
    /**
     * Set full screen mode
     * @param value true for full screen
     */
    void setFullScreen(bool value) {
        fullScreen = value;
    }
};

// Global instances (matching AC's global scope)
extern Camera WebCam;
extern Camera latestFrame;
extern Camera firstFrame;
extern SidebarConsole sidebar;
extern Screen Background;

#endif // HAVE_OPENCV

} // namespace AC

#endif // AC_CAMERA_HPP
