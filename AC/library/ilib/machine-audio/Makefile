CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -fPIC -shared
TARGET   = libacmachinaaudio.so

# Detect optional deps and enable them if present
MPG123_OK := $(shell pkg-config --exists libmpg123 2>/dev/null && echo yes)
ALSA_OK   := $(shell pkg-config --exists alsa 2>/dev/null && echo yes)

CFLAGS_EXTRA =
LINK_EXTRA   =

ifeq ($(MPG123_OK),yes)
  CFLAGS_EXTRA += $(shell pkg-config --cflags libmpg123)
  LINK_EXTRA   += $(shell pkg-config --libs   libmpg123)
else
  CFLAGS_EXTRA += -DAC_NO_MPG123
endif

ifeq ($(ALSA_OK),yes)
  CFLAGS_EXTRA += $(shell pkg-config --cflags alsa)
  LINK_EXTRA   += $(shell pkg-config --libs   alsa)
else
  CFLAGS_EXTRA += -DAC_NO_ALSA
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): machine_audio.cpp machine_audio_c.h
	$(CXX) $(CXXFLAGS) $(CFLAGS_EXTRA) -o $@ $< $(LINK_EXTRA)
	@echo "Built: $@"
	@pkg-config --exists libmpg123 2>/dev/null && echo "  mpg123  : enabled" || echo "  mpg123  : disabled (install libmpg123-dev)"
	@pkg-config --exists alsa      2>/dev/null && echo "  ALSA    : enabled" || echo "  ALSA    : disabled (install libasound2-dev)"

clean:
	rm -f $(TARGET)
