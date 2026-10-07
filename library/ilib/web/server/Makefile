CXX = g++
CXXFLAGS = -std=c++17 -fPIC -O2 -Wall -DAC_WEBSERVER_DIR='"$(CURDIR)"'

TARGET_SO = libacserver.so
TARGET_A  = libacserver.a
SRC = web-server.cpp
OBJ = web-server.o

all: $(TARGET_SO) $(TARGET_A)

$(TARGET_SO): $(OBJ)
	$(CXX) -shared -o $@ $^

$(TARGET_A): $(OBJ)
	ar rcs $@ $^

$(OBJ): $(SRC) web-server_c.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET_SO) $(TARGET_A)

.PHONY: all clean
