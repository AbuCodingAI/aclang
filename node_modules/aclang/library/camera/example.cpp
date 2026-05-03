// AC Camera Library Example
// Demonstrates motion detection with sidebar console

#include "camera.hpp"
#include <iostream>
#include <string>

using namespace AC;

int main() {
    // Initialize camera
    if (!WebCam.init()) {
        std::cerr << "Failed to initialize camera" << std::endl;
        return 1;
    }
    
    // Take first frame as reference
    std::cout << "Capturing reference frame..." << std::endl;
    firstFrame.capture("pic.png");
    
    // Load reference for motion detection
    MotionDetector detector;
    detector.setReference("pic.png");
    
    // Configure sidebar console
    sidebar.config("item", "console");
    sidebar.setRegion("left");
    sidebar.setInteractive(true);
    
    // Main loop
    while (true) {
        // Capture current frame
        cv::Mat currentFrame;
        if (latestFrame.capture(currentFrame)) {
            // Check for motion
            if (detector.detectMotion(currentFrame)) {
                sidebar.display("Movement Detected");
                sidebar.display("Type 'reset' to update reference frame.");
                
                // Update reference
                latestFrame.capture("pic.png");
                detector.setReference("pic.png");
            }
            
            // Display in window
            Background.setFrame(currentFrame);
            Background.display("Camera Feed");
            
            // Check for user input
            std::string cmd = sidebar.ask("Command?");
            
            if (cmd == "reset") {
                sidebar.display("New reference frame saved.");
                latestFrame.capture("pic.png");
                detector.setReference("pic.png");
            } else if (cmd == "exit") {
                sidebar.display("Stopping AC Motion Tracker...");
                break;
            }
        }
        
        // Exit on ESC key
        if (cv::waitKey(30) == 27) {
            break;
        }
    }
    
    WebCam.release();
    return 0;
}
