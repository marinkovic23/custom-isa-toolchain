#include <iostream>
#include <string>

#include "../inc/Emulator.hpp"

using namespace std;

int main(int argc, char** argv) {
    if (argc != 2) {
        cerr << "Usage:\n";
        cerr << "  ./emulator input.hex\n";
        return 1;
    }

    string inputFile = argv[1];

    Emulator emulator;

    try {
        emulator.loadHexFile(inputFile);
        emulator.run();
    } catch (const exception& e) {
        cerr << "Emulator error: " << e.what() << endl;
        return 1;
    }

    return 0;
}