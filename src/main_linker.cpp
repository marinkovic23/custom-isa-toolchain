#include <iostream>
#include <vector>
#include <string>

#include "../inc/Linker.hpp"
#include <optional>
#include <unordered_map>
#include <stdexcept>

using namespace std;


enum class LinkerMode {
    HEX,
    RELOCATABLE
};

struct LinkerOptions {
    optional<LinkerMode> mode;
    string outputFile;
    vector<string> inputFiles;
    unordered_map<string, uint32_t> placements;
};


static pair<string, uint32_t> parsePlaceOption(const string& argument) {
    const string prefix = "-place=";

    if (argument.rfind(prefix, 0) != 0) {
        throw runtime_error("Invalid -place option: " + argument);
    }

    string value = argument.substr(prefix.size());

    size_t at = value.find('@');

    if (at == string::npos || 
        at == 0 ||
        at == value.size() - 1 ||
        value.find('@', at + 1) != string::npos) {
        throw runtime_error("Invalid -place option: " + argument);
    }


    string section = value.substr(0, at);
    string addressText = value.substr(at + 1);

    size_t processed = 0;
    unsigned long long address = 0;

    try {
        address = stoull(addressText, &processed, 0);
    } catch (const exception&) {
        throw runtime_error("Invalid address in option: " + argument);
    }

    if (processed != addressText.size() || address > UINT32_MAX) {
        throw runtime_error("address is not a valid 32-bit value: " + addressText);
    }

    return {
        section,
        static_cast<uint32_t>(address)
    };
}

static void setLinkerMode(LinkerOptions& options, LinkerMode requestedMode, const string& optionName) {
    if (options.mode.has_value()) {
        throw runtime_error(
            "exactly one of -hex and -relocatable must be specified; "
            "conflicting or repeated option: " + 
            optionName
        );
    }


    options.mode = requestedMode;
}



static LinkerOptions parseArguments(int argc, char** argv) {
    LinkerOptions options;
    bool outputSpecified = false;
    for (int i = 1; i < argc; i++) {
        string argument = argv[i];

        if (argument == "-o") {
            if (outputSpecified) {
                throw runtime_error("option -o specified more than once");
            }


            if (i + 1 >= argc) {
                throw runtime_error("missing output filename after -o");
            }

            options.outputFile = argv[++i];
            outputSpecified = true;
        }
        else if (argument == "-hex") {
            setLinkerMode(options, LinkerMode::HEX, argument);
        }
        else if (argument == "-relocatable" ) {
            setLinkerMode(options, LinkerMode::RELOCATABLE, argument);
        }
        else if (argument.rfind("-place", 0) == 0) {
            auto[section, address] = parsePlaceOption(argument);

            auto [iterator, inserted] = options.placements.emplace(section, address);

            if (!inserted) {
                throw runtime_error("section placement specified more than once: " + section);
            }
        }
        else if (!argument.empty() &&  argument[0] == '-') {
            throw runtime_error("unknown option: " + argument);
        }
        else {
            options.inputFiles.push_back(argument);
        }
    }

    if (!options.mode.has_value()) {
        throw runtime_error("exactly one of -hex and -relocatable must be specified");
    }

    if (options.inputFiles.empty()) {
        throw runtime_error("no input files provided");
    }

    if (!outputSpecified) {
        if (*options.mode == LinkerMode::HEX) {
            options.outputFile = "out.hex";
        }
        else {
            options.outputFile = "out.o";
        }
    }

    return options;
}

static void printUsage() {
    cerr
    << "Usage:\n"
    << " linker -hex "
    << "[-place=section@address ...] "
    << "[-o output.hex] input.o ...\n"
    << '\n'
    << " linker -relocatable "
    << "[-o output.o] input.o ...\n";
}




int main(int argc, char** argv) {
    try {
        LinkerOptions options = parseArguments(argc, argv);

        Linker linker;

        for (const string& inputFile : options.inputFiles) {
            linker.loadObjectFile(inputFile);
        }

        if (*options.mode == LinkerMode::HEX) {
            //add this to linker.hpp

            //use of placements will be implemented later

            linker.setPlaceOptions(options.placements);

            linker.writeHexFile(options.outputFile);
        }
        else {
            //this also must be added to linker hpp, I have not made this yet.

            linker.writeRelocatableFile(options.outputFile);
        }

        return 0;

    } catch (const exception& e) {
        cerr << "Linker error: " << e.what() << endl;


        printUsage();
        return 1;
    }

}