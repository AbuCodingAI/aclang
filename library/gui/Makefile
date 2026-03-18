CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2

# SDL2 + SDL2_image + SDL2_ttf flags
SDL_FLAGS  = $(shell pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf 2>/dev/null || echo "-lSDL2 -lSDL2_image -lSDL2_ttf")

# OpenCV (optional — enables webcam support)
OPENCV_FLAGS = $(shell pkg-config --cflags --libs opencv4 2>/dev/null)
ifneq ($(OPENCV_FLAGS),)
    CXXFLAGS += -DAC_USE_OPENCV $(OPENCV_FLAGS)
endif

# Install dependencies (Ubuntu 24.04)
install-deps:
	sudo apt install -y libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libopencv-dev

# Build example programs
motion: examples/motion_example.cpp
	$(CXX) $(CXXFLAGS) $< -o motion $(SDL_FLAGS)

geodeo: examples/geodeo_example.cpp
	$(CXX) $(CXXFLAGS) $< -o geodeo $(SDL_FLAGS)

clean:
	rm -f motion geodeo

.PHONY: install-deps clean
