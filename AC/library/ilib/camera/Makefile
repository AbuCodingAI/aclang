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
all: motion_tracker_example

motion_tracker_example: camera.cpp camera.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DAC_CAMERA_EXAMPLE camera.cpp -o motion_tracker_example $(LIBS)

example: example.cpp camera.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) example.cpp -o example $(LIBS)

clean:
	rm -f motion_tracker_example example *.o pic.png

test: motion_tracker_example
	./motion_tracker_example

install:
	@echo "Camera library is header-only. Include camera.hpp in your AC-generated C++ code."
	@echo "Make sure OpenCV is installed:"
	@echo "  Ubuntu/Debian: sudo apt-get install libopencv-dev"
	@echo "  macOS: brew install opencv"
	@echo "  Windows: Download from opencv.org"

.PHONY: all clean test install
