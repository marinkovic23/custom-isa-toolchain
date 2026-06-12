#include "../hpp/Linker.hpp"
#include <fstream>

using namespace std;

void Linker::loadObjectFile(const string& filename) {
    ObjectFile obj;
    obj.filename = filename;

    ifstream in(filename);

    if (!in) throw runtime_error("Cannot open object file: " + filename);

    string token;

    while (in >> token) {
        if (token == "SECTIONS") {
            readSections(in, obj);
        }
        else if (token == "SYMBOLS") {
            readSymbols(in, obj);
        }
        else if (token == "RELOCATIONS") {
            readRelocations(in, obj);
        }
        else if (token == "END") {
            break;
        }
        else throw runtime_error("Unknown object file token: " + token);
    }

    objectFiles.push_back(obj);

}

void Linker::readSections(ifstream& in, ObjectFile& obj) {
    int count;
    in >> count;

    for (int i = 0; i < count; i++) {
        string name;
        int size;

        in >> name >> size;

        Section section;
        section.name = name;

        for (int j = 0; j < size; j++) {
            string byteHex;
            in >> byteHex;

            uint8_t value = static_cast<uint8_t>(stoi(byteHex, nullptr, 16));

            section.bytes.push_back(value);
        }

        obj.sections[name] = section;
    }
}

//helper
SymbolBind Linker::stringToSymbolBind(const string& s) {
    if (s == "LOCAL") return SymbolBind::LOCAL;
    if (s == "GLOBAL") return SymbolBind::GLOBAL;
    if (s == "EXTERN") return SymbolBind::EXTERN;

    throw runtime_error("Invalid symbol bind: " + s);
}



void Linker::readSymbols(ifstream& in, ObjectFile& obj) {
    int count;
    in >> count;

    for (int i = 0; i < count; i++) {
        Symbol sym;

        string bindStr;
        string defStr;

        in >> sym.name  
           >> sym.section
           >> sym.offset
           >> bindStr
           >> defStr;

        sym.bind = stringToSymbolBind(bindStr);
        sym.defined = (defStr == "DEF");

        obj.symbols[sym.name] = sym;
    }
}

void Linker::readRelocations(ifstream& in, ObjectFile& obj) {
    int count;
    in >> count;

    for (int i = 0; i < count; i++) {
        Relocation rel;

        in >> rel.section
           >> rel.offset
           >> rel.symbol;

        obj.relocations.push_back(rel);
    }
}


void Linker::mergeSections() {
    for (size_t objIndex = 0; objIndex < objectFiles.size(); objIndex++) {
        ObjectFile& obj = objectFiles[objIndex];
        for (auto& pair : obj.sections) {
            Section& inputSec = pair.second;
            std::string name = inputSec.name;

            if (outputSections.find(name) == outputSections.end()) {
                OutputSection outSec;
                outSec.name = name;

                outputSections[name] = outSec;
                outputSectionOrder.push_back(name);
            }

            OutputSection& outSec = outputSections[name];

            inputSec.outputOffset = outSec.bytes.size();

            outSec.bytes.insert(
                outSec.bytes.end(),
                inputSec.bytes.begin(),
                inputSec.bytes.end()
            );

            outSec.size = outSec.bytes.size();
        }

    }
}


void Linker::resolveSymbols() {

    //prvi prolaz, definisemo sve simbole

    for (ObjectFile& obj : objectFiles) {
        for (const auto& pair : obj.symbols) {
            const Symbol& sym = pair.second;

            if (!sym.defined) continue;

            const Section& sec = obj.sections.at(sym.section);

            uint32_t finalAddress = sec.baseAddress + sym.offset;

            obj.localResolvedSymbols[sym.name] = finalAddress;

            if (sym.bind == SymbolBind::GLOBAL) {
                if (resolvedSymbols.find(sym.name) != resolvedSymbols.end()) throw runtime_error("Multiple definition of symbol: " + sym.name);
        
                resolvedSymbols[sym.name] = {sym.name, finalAddress};        
            }



        
        }
    }
    //drugi prolaz, gledamo eksterne simbole

    for (ObjectFile& obj : objectFiles) {
        for (const auto& pair : obj.symbols) {
            const Symbol& sym = pair.second;

            if (sym.bind == SymbolBind::EXTERN) {
                if (resolvedSymbols.find(sym.name) == resolvedSymbols.end()) {
                    throw runtime_error("Unresolved external symbol: " + sym.name);
                }
            }
        }
    }

}



uint32_t Linker::resolveSymbolForObject(const ObjectFile& obj, const std::string& symbol) {
    auto localIt = obj.localResolvedSymbols.find(symbol);

    if (localIt != obj.localResolvedSymbols.end()) return localIt->second;

    auto globalIt = resolvedSymbols.find(symbol);

    if (globalIt != resolvedSymbols.end()) return globalIt->second.address;

    throw runtime_error("Unresolved symbol in relocation: " + symbol);
}






