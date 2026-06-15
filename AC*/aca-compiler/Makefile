CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

BIN_DIR := bin
SRC_DIR := src

SRCS := $(SRC_DIR)/main.cpp \
	$(SRC_DIR)/lexer.cpp \
	$(SRC_DIR)/parser.cpp \
	$(SRC_DIR)/types.cpp \
	$(SRC_DIR)/cache.cpp \
	$(SRC_DIR)/sapl_lower.cpp \
	$(SRC_DIR)/ir.cpp \
	$(SRC_DIR)/ir_cg.cpp \
	$(SRC_DIR)/bny_cg.cpp

$(BIN_DIR)/aca: $(SRCS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

.PHONY: build test clean
build: $(BIN_DIR)/aca

test: build
	./$(BIN_DIR)/aca ../benchmark.aca

clean:
	rm -rf $(BIN_DIR)
