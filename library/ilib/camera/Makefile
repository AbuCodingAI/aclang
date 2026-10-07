# AC Camera Library Makefile
# Builds the camera library and example programs

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
# --cflags alone (not --libs) — `pkg-config --libs opencv5` blanket-links EVERY installed
# OpenCV module, including opencv_viz, which needs VTK. VTK isn't a dependency this
# library has any business requiring (camera.hpp only ever calls into VideoCapture/Mat/
# imread/imwrite/imshow/cvtColor/resize/threshold/absdiff/countNonZero — core, imgproc,
# imgcodecs, videoio, highgui, nothing else), but the blanket link made it one anyway —
# verified real failure: `ld: undefined reference to vtkAlgorithm::...` the moment OpenCV
# was rebuilt against a VTK-less system. Link only the modules actually used instead.
OPENCV_CFLAGS = $(shell pkg-config --cflags opencv4 2>/dev/null)
OPENCV_MODULES = core imgproc imgcodecs videoio highgui

# If opencv4 not found, try opencv5 (the pkg-config name OpenCV 5.x ships under), then
# the unversioned "opencv" name some distro packages still use.
ifeq ($(OPENCV_CFLAGS),)
    OPENCV_CFLAGS = $(shell pkg-config --cflags opencv5 2>/dev/null)
endif
ifeq ($(OPENCV_CFLAGS),)
    OPENCV_CFLAGS = $(shell pkg-config --cflags opencv 2>/dev/null)
endif

OPENCV_LIBS = $(addprefix -lopencv_,$(OPENCV_MODULES))
OPENCV_FLAGS = $(OPENCV_CFLAGS) $(OPENCV_LIBS)

INCLUDES = -I.
LIBS = $(OPENCV_FLAGS)

# Targets
TARGET_SO = libaccamera.so
TARGET_A  = libaccamera.a

all: $(TARGET_SO) $(TARGET_A)

# The shared/static libs AC's compiled backends actually link against — camera.cpp
# owns the global Camera/SidebarConsole/Screen instances (camera/sidebar/screen) that
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
