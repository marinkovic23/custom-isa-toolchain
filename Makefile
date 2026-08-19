CXX = g++

CXXFLAGS = -std=c++17 \
           -O2 \
           -Wall \
           -Wextra \
           -Wpedantic \
           -Wshadow \
           -Wconversion \
           -Wsign-conversion \
           -Wmaybe-uninitialized \
           -Iinc

CPP_DIR = src
COMMON = $(CPP_DIR)/Common.cpp
PARSER = $(CPP_DIR)/Parser.cpp

.PHONY: all clean sanitize test

all: assembler linker emulator

assembler: $(CPP_DIR)/main_assembler.cpp $(CPP_DIR)/Assembler.cpp $(PARSER) $(COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@

linker: $(CPP_DIR)/main_linker.cpp $(CPP_DIR)/Linker.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@

emulator: $(CPP_DIR)/main_emulator.cpp $(CPP_DIR)/Emulator.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: all
	./tests/run_all_tests.sh

sanitize:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="-std=c++17 -g -O1 -Wall -Wextra -Wpedantic -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinc" all

clean:
	rm -f assembler linker emulator
