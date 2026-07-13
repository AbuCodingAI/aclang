# AC Camera Library

A C++ library for camera capture, motion detection, and GUI sidebar console functionality.

## Features

- **Camera Capture**: Capture frames from webcam
- **Motion Detection**: Compare frames to detect movement
- **Sidebar Console**: Interactive GUI console for user input
- **Screen Display**: Display camera feed in OpenCV window

## Requirements

- C++17 or later
- OpenCV 4.x (or OpenCV 3.x with compatible API)

## Installation

### Ubuntu/Debian
```bash
sudo apt-get install libopencv-dev
```

### macOS
```bash
brew install opencv
```

### Windows
Download OpenCV from [opencv.org](https://opencv.org) and configure your build system.

## Usage

Include the header in your AC-generated C++ code:

```cpp
#include "library/camera/camera.hpp"
using namespace AC;
```

### Camera Usage

```cpp
// Initialize camera
Camera cam;
cam.init(0);  // 0 = default camera

// Capture frame
cv::Mat frame;
cam.capture(frame);

// Capture and save
cam.capture("output.jpg");
```

### Motion Detection

```cpp
MotionDetector detector;

// Set reference frame
detector.setReference("reference.jpg");

// Or set from Mat
detector.setReference(frame);

// Detect motion
if (detector.detectMotion(currentFrame)) {
    std::cout << "Motion detected!" << std::endl;
}

// Adjust sensitivity (0.0 to 1.0)
detector.setSensitivity(0.1);
```

### Sidebar Console

```cpp
SidebarConsole console;

// Configure
console.config("item", "console");
console.setRegion("left");
console.setInteractive(true);

// Display messages
console.display("Hello World");

// Get user input
std::string input = console.ask("Enter command:");
```

### Screen Display

```cpp
Screen screen;
screen.setMode("livefeed");

// Update from camera
screen.updateFrame(camera);

// Display in window
screen.display("Camera Feed");
```

## Example

See `example.cpp` for a complete motion detection example with sidebar console.

Build and run:
```bash
make example
./example
```

## AC Integration

In your AC code:

```ac
AC->C++

use ilib camera

<mainloop>
    <LOGIC>
        WebCam.init()
        firstFrame.picture()
        save as pic.png
        
        <StartHere>
            IF frame #= pic.png
                sidebar.display $Movement Detected$
                latestFrame.picture()
                save as pic.png
            sidebar.ask $Command?$
        <EndHere>
    <LOGIC>
<mainloop>
```

## API Reference

### Camera

- `bool init(int device_id = 0)` - Initialize camera
- `bool isInitialized()` - Check if initialized
- `bool capture(cv::Mat& frame)` - Capture frame to Mat
- `bool capture(const std::string& filename)` - Capture and save to file
- `void release()` - Release camera

### MotionDetector

- `void setReference(const cv::Mat& frame)` - Set reference frame
- `bool setReference(const std::string& filename)` - Load reference from file
- `bool detectMotion(const cv::Mat& currentFrame)` - Check for motion
- `void setSensitivity(double value)` - Set sensitivity (0.0-1.0)
- `bool hasReferenceFrame()` - Check if reference exists

### SidebarConsole

- `void config(const std::string& key, const std::string& value)` - Configure properties
- `void setRegion(const std::string& pos)` - Set position
- `void setInteractive(bool value)` - Enable/disable interaction
- `void display(const std::string& message)` - Show message
- `std::string ask(const std::string& prompt)` - Ask for input
- `std::string getInput()` - Get user input
- `void setInputCallback(std::function<std::string(const std::string&)>)` - Set input handler

### Screen

- `void setMode(const std::string& m)` - Set display mode
- `void display(const std::string& windowName)` - Show frame
- `bool updateFrame(Camera& camera)` - Update from camera
- `void setFrame(const cv::Mat& frame)` - Set frame directly
- `bool isLiveFeed()` - Check if live feed mode
- `void setFullScreen(bool value)` - Toggle full screen
