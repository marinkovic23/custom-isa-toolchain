#include "../hpp/Assembler.hpp"
#include "../hpp/Linker.hpp"


using namespace std;

int main() {
    Assembler assembler;
    assembler.assemble("test.s");

    assembler.writeObjectFile("test.o");


    Linker linker;
    linker.loadObjectFile("test.o");


    return 0;
}