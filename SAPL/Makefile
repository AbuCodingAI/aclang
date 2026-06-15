# SAPL - Semantic Algebra Programming Language
# Makefile for building the C++ interpreter

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = 

# Source files
SRCS = main.cpp
HEADERS = sapl.h

# Object files
OBJS = $(SRCS:.cpp=.o)

# Target executable
TARGET = sapl

# Default target
all: $(TARGET)

# Build the interpreter
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

# Compile source files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Debug build
debug: CXXFLAGS = -std=c++17 -Wall -Wextra -g -O0 -fsanitize=address
debug: clean $(TARGET)

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Install to /usr/local/bin
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Run tests
test: $(TARGET)
	@echo "Running SAPL tests..."
	@echo "Test 1: Sum reduction"
	./$(TARGET) -e "E I 1 10"
	@echo ""
	@echo "Test 2: Product reduction (factorial)"
	./$(TARGET) -e "PI I 1 5"
	@echo ""
	@echo "Test 3: Interval"
	./$(TARGET) -e "I 1 5"
	@echo ""
	@echo "Test 4: Array operations"
	./$(TARGET) -e "E [1 2 3 4 5]"
	@echo ""
	@echo "Test 5: Function definition"
	./$(TARGET) -e "factorial(x) = PI I 1 x; 0<- factorial(5)"
	@echo ""
	@echo "All tests completed!"

# Run benchmark
benchmark: $(TARGET)
	./$(TARGET) benchmark.sapl

# Help
help:
	@echo "Available targets:"
	@echo "  all       - Build the SAPL interpreter (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  debug     - Build with debug symbols and AddressSanitizer"
	@echo "  install   - Install to /usr/local/bin"
	@echo "  uninstall - Remove from /usr/local/bin"
	@echo "  test      - Run basic tests"
	@echo "  benchmark - Run the benchmark.sapl file"
	@echo "  help      - Show this help message"

.PHONY: all clean debug install uninstall test benchmark help