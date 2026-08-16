# AC Camera Library Makefile
# Builds the camera library and example programs

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
OPENCV_FLAGS = $(shell pkg-config --cflags --libs opencv4)

# If opencv4 not found, try opencv
ifeq ($(OPENCV_FLAGS),)
    OPENCV_FLAGS = $(shell pkg-config --cflags --libs opencv)
endif

INCLUDES = -I.
LIBS = $(OPENCV_FLAGS)

# Targets
TARGET_SO = libaccamera.so
TARGET_A  = libaccamera.a

all: $(TARGET_SO) $(TARGET_A) motion_tracker_example

# The shared/static libs AC's compiled backends actually link against — camera.cpp
# owns the global Camera/SidebarConsole/Screen instances (WebCam/sidebar/etc.) that
# camera_c.cpp's extern "C" shim declares `extern` and wraps.
$(TARGET_SO): camera.o camera_c.o
	$(CXX) -shared -o $@ $^ $(LIBS)

$(TARGET_A): camera.o camera_c.o
	ar rcs $@ $^

camera.o: camera.cpp camera.hpp
	$(CXX) $(CXXFLAGS) -fPIC $(INCLUDES) -c camera.cpp -o $@ $(LIBS)

camera_c.o: camera_c.cpp camera_c.h camera.hpp
	$(CXX) $(CXXFLAGS) -fPIC $(INCLUDES) -c camera_c.cpp -o $@ $(LIBS)

motion_tracker_example: camera.cpp camera.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DAC_CAMERA_EXAMPLE camera.cpp -o motion_tracker_example $(LIBS)

example: example.cpp camera.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) example.cpp -o example $(LIBS)

clean:
	rm -f motion_tracker_example example *.o pic.png $(TARGET_SO) $(TARGET_A)

test: motion_tracker_example
	./motion_tracker_example

install:
	@echo "Camera library is header-only. Include camera.hpp in your AC-generated C++ code."
	@echo "Make sure OpenCV is installed:"
	@echo "  Ubuntu/Debian: sudo apt-get install libopencv-dev"
	@echo "  macOS: brew install opencv"
	@echo "  Windows: Download from opencv.org"

.PHONY: all clean test install
