CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -MMD -MP

SRC = src/main.cpp src/lexer.cpp src/parser.cpp src/backend_registry.cpp src/ir.cpp src/ir_codegen.cpp src/exp_bny.cpp
OBJ = $(SRC:.cpp=.o)
DEP = $(SRC:.cpp=.d)
OUT = ac

all: $(OUT)

$(OUT): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(OUT)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEP)

install: all
ifeq ($(OS),Windows_NT)
	cp $(OUT) /usr/local/bin/ac
else
	sudo cp $(OUT) /usr/local/bin/ac
endif

windows:
	x86_64-w64-mingw32-g++ -std=c++17 -O2 -static $(SRC) -o ac.exe

clean:
ifeq ($(OS),Windows_NT)
	rm -f $(OUT).exe $(OBJ) $(DEP)
else
	rm -f $(OUT) ac.exe $(OBJ) $(DEP)
endif
