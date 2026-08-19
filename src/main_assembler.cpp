#include <iostream>
#include <string>

#include "../inc/Assembler.hpp"

using namespace std;


struct AssemblerOptions {
    string inputFile;
    string outputFile;
};


static string defaultOutputName(const string& inputFile) {
    size_t lastSlash = inputFile.find_last_of("/\\");
    string filename = (lastSlash == string::npos)
        ? inputFile
        : inputFile.substr(lastSlash + 1);

    size_t lastDot = filename.find_last_of('.');

    if (lastDot != string::npos) {
        filename = filename.substr(0, lastDot);
    }

    return filename + ".o";
}

static AssemblerOptions parseArguments(int argc, char** argv) {
    AssemblerOptions options;
    bool outputSpecified = false;

    for (int i = 1; i < argc; i++) {
        string argument = argv[i];
        if (argument == "-o") {
            if(outputSpecified) {
                throw runtime_error("option -o specified more than once");
            }

            if (i + 1 >= argc) {
                throw runtime_error("missing output filename after -o");
            }

            options.outputFile = argv[++i];
            outputSpecified = true;
        }
        else if (!argument.empty() && argument[0] == '-') {
            throw runtime_error("unknown option: " + argument);
        }
        else {
            if (!options.inputFile.empty()) {
                throw runtime_error("more than one input file specified: " + options.inputFile + " and " + argument);
            }

            options.inputFile = argument;
        }
    }

    if (options.inputFile.empty()) {
        throw runtime_error("no input file specified");
    }
    if (!outputSpecified) {
        options.outputFile = defaultOutputName(options.inputFile);
    }

    return options;
}




int main(int argc, char** argv) {
    try {
        AssemblerOptions options = parseArguments(argc, argv);


        Assembler assembler;
        assembler.assemble(options.inputFile);
        assembler.writeObjectFile(options.outputFile);
        return 0;
    }
    catch (const exception& e) {
        cerr << "Assembler error: " << e.what() << endl;
        cerr << "Usage: assembler [-o output.o] input.s\n";
        return 1;
    }
}