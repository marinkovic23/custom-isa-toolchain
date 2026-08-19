#include "../inc/Linker.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <unordered_set>

using namespace std;


static uint32_t checkedUint32Size(size_t size, const string& description) {
    if (size > static_cast<size_t>(numeric_limits<uint32_t>::max())) {
        throw runtime_error(description + " exceeds 32-bit range");
    }

    return static_cast<uint32_t>(size);
}

static uint32_t checkedUint32Value(uint64_t value, const string& description) {
    if (value > static_cast<uint64_t>(numeric_limits<uint32_t>::max())) {
        throw runtime_error(description + " exceeds 32-bit range");
    }

    return static_cast<uint32_t>(value);
}

static uint32_t checkedOffsetSum(uint32_t first, uint32_t second, const string& description) {
    return checkedUint32Value(
        static_cast<uint64_t>(first) + static_cast<uint64_t>(second),
        description
    );
}

static uint32_t addModulo32(uint32_t symbolValue, int32_t addend) {
    return symbolValue + static_cast<uint32_t>(addend);
}

static int64_t addSigned(uint32_t symbolValue, int32_t addend) {
    return static_cast<int64_t>(symbolValue) + static_cast<int64_t>(addend);
}

static string symbolBindToString(SymbolBind bind) {
    switch (bind) {
        case SymbolBind::LOCAL: return "LOCAL";
        case SymbolBind::GLOBAL: return "GLOBAL";
        case SymbolBind::EXTERN: return "EXTERN";
    }

    throw runtime_error("Invalid symbol binding value");
}

static string relocationTypeToString(RelocationType type) {
    switch (type) {
        case RelocationType::ABS32: return "ABS32";
        case RelocationType::DISP12: return "DISP12";
    }

    throw runtime_error("Invalid relocation type value");
}


void Linker::setPlaceOptions(const unordered_map<string, uint32_t>& placements) {
    this->placeOptions = placements;
}


static void expectToken(ifstream& input, const string& expected, const string& filename) {

    string actual;

    if (!(input >> actual)) {
        throw runtime_error("Unexpected end of object file " + filename + "; expected token: " + expected);
    }

    if (actual != expected) {
        throw runtime_error("Invalid token in object file " + filename + "; expected:  " + expected + ", found: " + actual);
    }
}

void Linker::loadObjectFile(const string& filename) {
    ObjectFile obj;
    obj.filename = filename;

    ifstream in(filename);

    if (!in) throw runtime_error("Cannot open object file: " + filename);

    string magic;
    unsigned version = 0;

    if (!(in >> magic >> version)) {
        throw runtime_error("Invalid or empty object file: " + filename);
    }

    if (magic != "MOJOBJ") {
        throw runtime_error("Invalid object-file signature in " + filename + ": " + magic);
    }

    if (version != 1) {
        throw runtime_error("Unsupported MOJOBJ version in " + filename + ": " + to_string(version));
    }



    expectToken(in, "SECTIONS", filename);
    readSections(in, obj);

    expectToken(in, "SYMBOLS", filename);
    readSymbols(in, obj);


    expectToken(in, "RELOCATIONS", filename);
    readRelocations(in, obj);
    
    expectToken(in, "END", filename);
  
    string trailingToken;
    
    if (in >> trailingToken) {
        throw runtime_error("Unexpected content after END in " + filename + ": " + trailingToken);
    }

    validateObjectFile(obj);
    objectFiles.push_back(std::move(obj));

}


static uint8_t parseHexByte(const string& text, const string& filename) {
    if (text.size() != 2) {
        throw runtime_error("Invalid byte in " + filename + ": " + text);
    }

    size_t processed = 0;
    unsigned long value = 0;

    try {
        value = stoul(text, &processed, 16);
    }
    catch (const exception&) {
        throw runtime_error("Invalid byte in " + filename + ": " + text);
    }


    if (processed != text.size() || value > 0xFF) {
        throw runtime_error("Invalid byte in " + filename + ": " + text);
    }

    return static_cast<uint8_t>(value);
}



void Linker::readSections(ifstream& in, ObjectFile& obj) {
    size_t count = 0;

    if (!(in >> count)) {
        throw runtime_error("Invalid selection count in " + obj.filename);
    }


    for (size_t sectionIndex = 0; sectionIndex < count; sectionIndex++) {
       
        expectToken(in, "SECTION", obj.filename);

        Section section;
        size_t sectionSize = 0;

        if (!(in >> section.name >> sectionSize)) {
            throw runtime_error("Malformed section record in " + obj.filename);
        }

        if (obj.sections.find(section.name) != obj.sections.end()) {
            throw runtime_error("Duplicate section in object file: " + section.name);
        }

        section.bytes.reserve(sectionSize);

        for (size_t byteIndex = 0; byteIndex < sectionSize; byteIndex++) {

            string byteText;

            if (!(in >> byteText)) {
                throw runtime_error("Unexpected end of section " + section.name);
            }
            

            section.bytes.push_back(parseHexByte(byteText, obj.filename));
        }

        obj.sectionOrder.push_back(section.name);

        obj.sections.emplace(section.name, std::move(section));
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
    size_t count = 0;
    if (!(in >> count)) {
        throw runtime_error("Invalid symbol count in " + obj.filename);
    }

    for (size_t index = 0; index < count; index++) {
        string recordType;

        if (!(in >> recordType)) {
            throw runtime_error("Unexpected end of file while reading symbols in " + obj.filename);
        }

        if (recordType != "SYMBOL") {
            throw runtime_error("Expected SYMBOL record in " + obj.filename + ", found: " + recordType);
        }

        Symbol sym;

        string bindStr;
        string defStr;

        if (!(in >> sym.name  
           >> sym.section
           >> sym.offset
           >> bindStr
           >> defStr)) {
            throw runtime_error("Malformed SYMBOL record in " + obj.filename);
        }

        sym.bind = stringToSymbolBind(bindStr);

        if (defStr == "DEF") {
            sym.defined = true;
        }
        else if (defStr == "UND") {
            sym.defined = false;
        } else {
            throw runtime_error("Invalid symbol definition state: " + defStr);
        }

        auto [iterator, inserted] = obj.symbols.emplace(sym.name, sym);
        (void) iterator;

        if (!inserted) {
            throw runtime_error("Duplicate symbol in " + obj.filename + ": " + sym.name);
        }
    }
}


static RelocationType relocationTypeFromString(const string& text) {
    if (text == "ABS32") {
        return RelocationType::ABS32;
    }
    if (text == "DISP12") {
        return RelocationType::DISP12;
    }

    throw runtime_error("Unknown relocation type: " + text);
}



void Linker::readRelocations(ifstream& in, ObjectFile& obj) {
    size_t count = 0;
   
    if (!(in >> count)) {
        throw runtime_error("Invalid relocation count in " + obj.filename);
    }

    for (size_t index = 0; index < count; index++) {
        expectToken(in, "RELOCATION", obj.filename);

        Relocation relocation;

        string typeText;

        if (!(in
            >> relocation.section
            >> relocation.offset
            >> relocation.symbol
            >> typeText
            >> relocation.addend))
        {
            throw runtime_error("Malformed relocation record in " + obj.filename);
        }

        relocation.type = relocationTypeFromString(typeText);
        obj.relocations.push_back(relocation);
    }
}


void Linker::validateObjectFile(const ObjectFile& obj) const {
    if (obj.sectionOrder.size() != obj.sections.size()) {
        throw runtime_error("Section order does not match section table in " + obj.filename);
    }

    unordered_set<string> seenSections;

    for (const string& sectionName : obj.sectionOrder) {
        if (sectionName.empty() || sectionName == "UND" || sectionName == "ABS") {
            throw runtime_error("Invalid section name in " + obj.filename + ": " + sectionName);
        }

        if (!seenSections.insert(sectionName).second) {
            throw runtime_error("Duplicate section order entry in " + obj.filename + ": " + sectionName);
        }

        if (obj.sections.find(sectionName) == obj.sections.end()) {
            throw runtime_error("Section order references unknown section in " + obj.filename + ": " + sectionName);
        }
    }

    for (const auto& [tableName, symbol] : obj.symbols) {
        if (symbol.name.empty() || tableName != symbol.name) {
            throw runtime_error("Invalid symbol-table entry in " + obj.filename + ": " + tableName);
        }

        if (symbol.defined) {
            if (symbol.bind == SymbolBind::EXTERN) {
                throw runtime_error("External symbol is also defined in " + obj.filename + ": " + symbol.name);
            }

            if (symbol.section != "ABS") {
                auto sectionIt = obj.sections.find(symbol.section);
                if (sectionIt == obj.sections.end()) {
                    throw runtime_error("Defined symbol references unknown section in " + obj.filename + ": " + symbol.name);
                }

                if (static_cast<size_t>(symbol.offset) > sectionIt->second.bytes.size()) {
                    throw runtime_error("Defined symbol lies outside its section in " + obj.filename + ": " + symbol.name);
                }
            }
        }
        else {
            if (symbol.section != "UND") {
                throw runtime_error("Undefined symbol does not belong to UND in " + obj.filename + ": " + symbol.name);
            }

            if (symbol.bind == SymbolBind::LOCAL) {
                throw runtime_error("Unresolved local symbol in " + obj.filename + ": " + symbol.name);
            }
        }
    }

    for (const Relocation& relocation : obj.relocations) {
        auto sectionIt = obj.sections.find(relocation.section);
        if (sectionIt == obj.sections.end()) {
            throw runtime_error("Relocation references unknown section in " + obj.filename + ": " + relocation.section);
        }

        if (obj.symbols.find(relocation.symbol) == obj.symbols.end()) {
            throw runtime_error("Relocation references unknown symbol in " + obj.filename + ": " + relocation.symbol);
        }

        const size_t offset = static_cast<size_t>(relocation.offset);
        if (offset > sectionIt->second.bytes.size() || sectionIt->second.bytes.size() - offset < 4) {
            throw runtime_error("Relocation exceeds section bounds in " + obj.filename + ": " + relocation.section);
        }
    }
}


void Linker::resetOutputState() {
    outputSections.clear();
    outputSectionOrder.clear();
    resolvedSymbols.clear();

    for (ObjectFile& obj : objectFiles) {
        obj.localResolvedSymbols.clear();

        for (auto& [sectionName, section] : obj.sections) {
            (void) sectionName;
            section.baseAddress = 0;
            section.outputOffset = 0;
        }
    }
}


void Linker::mergeSections() {
    for (ObjectFile& obj : objectFiles) {
        for (const string& sectionName : obj.sectionOrder) {
            Section& inputSec = obj.sections.at(sectionName);

            if (outputSections.find(sectionName) == outputSections.end()) {
                OutputSection outSec;
                outSec.name = sectionName;

                outputSections.emplace(sectionName, std::move(outSec));
                outputSectionOrder.push_back(sectionName);
            }

            OutputSection& outSec = outputSections.at(sectionName);

            inputSec.outputOffset = checkedUint32Size(
                outSec.bytes.size(),
                "Output offset for section " + sectionName
            );

            outSec.bytes.insert(
                outSec.bytes.end(),
                inputSec.bytes.begin(),
                inputSec.bytes.end()
            );

            outSec.size = checkedUint32Size(
                outSec.bytes.size(),
                "Merged size of section " + sectionName
            );
        }
    }
}


void Linker::placeSections() {
    struct ExplicitRange {
        string name;
        uint64_t start = 0;
        uint64_t end = 0;
    };

    vector<ExplicitRange> ranges;
    uint64_t nextFreeAddress = 0;

    for (const auto& [sectionName, address] : placeOptions) {
        auto outputIt = outputSections.find(sectionName);

        if (outputIt == outputSections.end()) {
            throw runtime_error("-place references unknown section: " + sectionName);
        }

        OutputSection& section = outputIt->second;
        section.baseAddress = address;

        const uint64_t end = static_cast<uint64_t>(address) + static_cast<uint64_t>(section.size);
        if (end > (static_cast<uint64_t>(numeric_limits<uint32_t>::max()) + 1u)) {
            throw runtime_error("Placed section exceeds the 32-bit address space: " + sectionName);
        }

        ranges.push_back({sectionName, address, end});
        nextFreeAddress = max(nextFreeAddress, end);
    }

    sort(ranges.begin(), ranges.end(), [](const ExplicitRange& first, const ExplicitRange& second) {
        if (first.start != second.start) return first.start < second.start;
        return first.name < second.name;
    });

    uint64_t activeEnd = 0;
    string activeName;

    for (const ExplicitRange& range : ranges) {
        if (range.end == range.start) continue;

        if (!activeName.empty() && range.start < activeEnd) {
            throw runtime_error(
                "Placed sections overlap: " + activeName + " and " + range.name
            );
        }

        if (range.end > activeEnd) {
            activeEnd = range.end;
            activeName = range.name;
        }
    }

    for (const string& sectionName : outputSectionOrder) {
        if (placeOptions.find(sectionName) != placeOptions.end()) continue;

        OutputSection& section = outputSections.at(sectionName);

        if (nextFreeAddress > static_cast<uint64_t>(numeric_limits<uint32_t>::max())) {
            throw runtime_error("No address remains for section: " + sectionName);
        }

        section.baseAddress = static_cast<uint32_t>(nextFreeAddress);

        const uint64_t end = nextFreeAddress + static_cast<uint64_t>(section.size);
        if (end > (static_cast<uint64_t>(numeric_limits<uint32_t>::max()) + 1u)) {
            throw runtime_error("Section exceeds the 32-bit address space: " + sectionName);
        }

        nextFreeAddress = end;
    }

    for (ObjectFile& obj : objectFiles) {
        for (const string& sectionName : obj.sectionOrder) {
            Section& inputSection = obj.sections.at(sectionName);
            const OutputSection& outputSection = outputSections.at(sectionName);

            inputSection.baseAddress = checkedOffsetSum(
                outputSection.baseAddress,
                inputSection.outputOffset,
                "Input-section address for " + sectionName
            );
        }
    }
}


void Linker::resolveSymbols() {

    //prvi prolaz, definisemo sve simbole

    for (ObjectFile& obj : objectFiles) {
        for (const auto& pair : obj.symbols) {
            const Symbol& sym = pair.second;

            if (!sym.defined) continue;

            uint32_t finalAddress = 0;

            if (sym.section == "ABS") {
                finalAddress = sym.offset;
            }
            else {
                const Section& sec = obj.sections.at(sym.section);
                finalAddress = checkedOffsetSum(
                    sec.baseAddress,
                    sym.offset,
                    "Address of symbol " + sym.name
                );
            }

            obj.localResolvedSymbols[sym.name] = finalAddress;

            if (sym.bind == SymbolBind::GLOBAL) {
                if (resolvedSymbols.find(sym.name) != resolvedSymbols.end()) {
                    throw runtime_error("Multiple definition of symbol: " + sym.name);
                }
        
                resolvedSymbols[sym.name] = {sym.name, finalAddress};        
            }
        }
    }

    //drugi prolaz, gledamo eksterne i nedefinisane globalne simbole

    for (ObjectFile& obj : objectFiles) {
        for (const auto& pair : obj.symbols) {
            const Symbol& sym = pair.second;

            if (!sym.defined && (sym.bind == SymbolBind::EXTERN || sym.bind == SymbolBind::GLOBAL)) {
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

void Linker::write32(vector<uint8_t>& bytes, uint32_t offset, uint32_t value) {
    const size_t index = static_cast<size_t>(offset);

    if (index > bytes.size() || bytes.size() - index < 4) {
        throw runtime_error("Relocation write outside section");
    }

    bytes[index + 0] = static_cast<uint8_t>(value & 0xFFu);
    bytes[index + 1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    bytes[index + 2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    bytes[index + 3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

void Linker::writeDisp12(vector<uint8_t>& bytes, uint32_t instrOffset, int64_t value) {
    const size_t index = static_cast<size_t>(instrOffset);

    if (index > bytes.size() || bytes.size() - index < 4) {
        throw runtime_error("DISP12 relocation outside section");
    }

    // DISP12 means the absolute signed relocation value S + A, not a
    // PC-relative displacement. It must fit in the instruction's signed
    // 12-bit Disp field.
    if (value < -2048 || value > 2047) {
        throw runtime_error(
            "DISP12 relocation value does not fit in signed 12 bits: " +
            to_string(value)
        );
    }

    const uint32_t encoded = static_cast<uint32_t>(value) & 0xFFFu;

    bytes[index + 2] = static_cast<uint8_t>(
        (static_cast<uint32_t>(bytes[index + 2]) & 0xF0u) |
        ((encoded >> 8) & 0x0Fu)
    );
    bytes[index + 3] = static_cast<uint8_t>(encoded & 0xFFu);
}

void Linker::applyRelocations() {
    for (ObjectFile& obj : objectFiles) {
        for (const Relocation& rel : obj.relocations) {
            const uint32_t symbolAddress = resolveSymbolForObject(obj, rel.symbol);
            const Section& inputSection = obj.sections.at(rel.section);
            OutputSection& outputSection = outputSections.at(rel.section);

            const uint32_t outputOffset = checkedOffsetSum(
                inputSection.outputOffset,
                rel.offset,
                "Output relocation offset"
            );

            switch (rel.type) {
                case RelocationType::ABS32:
                    // ABS32 is (S + A) modulo 2^32.
                    write32(
                        outputSection.bytes,
                        outputOffset,
                        addModulo32(symbolAddress, rel.addend)
                    );
                    break;

                case RelocationType::DISP12:
                    // DISP12 is the signed absolute value S + A and must fit
                    // in the 12-bit field.
                    writeDisp12(
                        outputSection.bytes,
                        outputOffset,
                        addSigned(symbolAddress, rel.addend)
                    );
                    break;
            }
        }
    }
}


void Linker::writeHexFile(const std::string& outputPath) {
    resetOutputState();
    mergeSections();
    placeSections();
    resolveSymbols();
    applyRelocations();

    vector<string> sectionsByAddress = outputSectionOrder;
    unordered_map<string, size_t> originalOrder;

    for (size_t index = 0; index < outputSectionOrder.size(); index++) {
        originalOrder.emplace(outputSectionOrder[index], index);
    }

    sort(sectionsByAddress.begin(), sectionsByAddress.end(), [&](const string& first, const string& second) {
        const OutputSection& firstSection = outputSections.at(first);
        const OutputSection& secondSection = outputSections.at(second);

        if (firstSection.baseAddress != secondSection.baseAddress) {
            return firstSection.baseAddress < secondSection.baseAddress;
        }

        return originalOrder.at(first) < originalOrder.at(second);
    });

    ofstream out(outputPath);

    if (!out) throw runtime_error("Cannot open output file: " + outputPath);

    for (const string& sectionName : sectionsByAddress) {
        const OutputSection& sec = outputSections.at(sectionName);

        const auto& bytes = sec.bytes;
        uint32_t base = sec.baseAddress;

        for (size_t i = 0; i < bytes.size(); i += 8) {
            out << hex << uppercase
                << setw(8) << setfill('0')
                << (base + checkedUint32Size(i, "Hex output offset"))
                << ": ";

            for (size_t j = 0; j < 8 && i + j < bytes.size(); j++) {
                if (j != 0) out << ' ';
                out << setw(2) << setfill('0')
                    << static_cast<unsigned>(bytes[i + j]);
            }

            out << "\n";
        }
    }

    if (!out) throw runtime_error("Failed while writing hex output: " + outputPath);
}


void Linker::writeMergedObjectFile(const string& outputPath) {
    unordered_map<string, pair<size_t, const Symbol*>> globalDefinitions;

    for (size_t objectIndex = 0; objectIndex < objectFiles.size(); objectIndex++) {
        const ObjectFile& obj = objectFiles[objectIndex];

        for (const auto& [name, symbol] : obj.symbols) {
            if (!symbol.defined || symbol.bind != SymbolBind::GLOBAL) continue;

            auto [iterator, inserted] = globalDefinitions.emplace(name, make_pair(objectIndex, &symbol));
            (void) iterator;

            if (!inserted) {
                throw runtime_error("Multiple definition of symbol: " + name);
            }
        }
    }

    unordered_set<string> usedOutputNames;

    for (const ObjectFile& obj : objectFiles) {
        for (const auto& [name, symbol] : obj.symbols) {
            if (symbol.bind != SymbolBind::LOCAL) usedOutputNames.insert(name);
        }
    }

    vector<unordered_map<string, string>> outputNames(objectFiles.size());

    for (size_t objectIndex = 0; objectIndex < objectFiles.size(); objectIndex++) {
        const ObjectFile& obj = objectFiles[objectIndex];
        vector<string> symbolNames;
        symbolNames.reserve(obj.symbols.size());

        for (const auto& [name, symbol] : obj.symbols) {
            (void) symbol;
            symbolNames.push_back(name);
        }

        sort(symbolNames.begin(), symbolNames.end());

        for (const string& name : symbolNames) {
            const Symbol& symbol = obj.symbols.at(name);

            if (symbol.bind != SymbolBind::LOCAL) {
                outputNames[objectIndex][name] = name;
                continue;
            }

            string candidate = ".L" + to_string(objectIndex) + "." + name;
            while (!usedOutputNames.insert(candidate).second) candidate += "_";
            outputNames[objectIndex][name] = candidate;
        }
    }

    unordered_map<string, Symbol> outputSymbols;

    //Lokalni simboli moraju dobiti jedinstvena imena jer ce svi biti u jednoj tabeli simbola.
    for (size_t objectIndex = 0; objectIndex < objectFiles.size(); objectIndex++) {
        const ObjectFile& obj = objectFiles[objectIndex];

        for (const auto& [name, symbol] : obj.symbols) {
            if (symbol.bind != SymbolBind::LOCAL) continue;

            Symbol outputSymbol = symbol;
            outputSymbol.name = outputNames[objectIndex].at(name);

            if (outputSymbol.defined && outputSymbol.section != "ABS") {
                const Section& inputSection = obj.sections.at(outputSymbol.section);
                outputSymbol.offset = checkedOffsetSum(
                    inputSection.outputOffset,
                    outputSymbol.offset,
                    "Relocatable symbol offset for " + outputSymbol.name
                );
            }

            outputSymbols.emplace(outputSymbol.name, std::move(outputSymbol));
        }
    }

    //Definisani globalni simbol ima prednost u odnosu na spoljasnje deklaracije istog imena.
    for (const auto& [name, definition] : globalDefinitions) {
        const ObjectFile& obj = objectFiles[definition.first];
        Symbol outputSymbol = *definition.second;

        if (outputSymbol.section != "ABS") {
            const Section& inputSection = obj.sections.at(outputSymbol.section);
            outputSymbol.offset = checkedOffsetSum(
                inputSection.outputOffset,
                outputSymbol.offset,
                "Relocatable global-symbol offset for " + name
            );
        }

        outputSymbols[name] = std::move(outputSymbol);
    }

    //Nerazreseni simboli ostaju u izlaznom predmetnom programu za naredno povezivanje.
    for (const ObjectFile& obj : objectFiles) {
        for (const auto& [name, symbol] : obj.symbols) {
            if (symbol.bind == SymbolBind::LOCAL || symbol.defined) continue;
            if (outputSymbols.find(name) != outputSymbols.end()) continue;

            Symbol outputSymbol;
            outputSymbol.name = name;
            outputSymbol.section = "UND";
            outputSymbol.offset = 0;
            outputSymbol.bind = symbol.bind;
            outputSymbol.defined = false;

            outputSymbols.emplace(name, std::move(outputSymbol));
        }
    }

    //Ako postoji i GLOBAL UND i EXTERN UND zapis, cuvamo GLOBAL vezivanje.
    for (const ObjectFile& obj : objectFiles) {
        for (const auto& [name, symbol] : obj.symbols) {
            if (symbol.defined || symbol.bind != SymbolBind::GLOBAL) continue;

            auto outputIt = outputSymbols.find(name);
            if (outputIt != outputSymbols.end() && !outputIt->second.defined) {
                outputIt->second.bind = SymbolBind::GLOBAL;
            }
        }
    }

    vector<Relocation> outputRelocations;

    for (size_t objectIndex = 0; objectIndex < objectFiles.size(); objectIndex++) {
        const ObjectFile& obj = objectFiles[objectIndex];

        for (const Relocation& relocation : obj.relocations) {
            Relocation outputRelocation = relocation;
            const Section& inputSection = obj.sections.at(relocation.section);

            outputRelocation.offset = checkedOffsetSum(
                inputSection.outputOffset,
                relocation.offset,
                "Relocatable relocation offset"
            );

            auto nameIt = outputNames[objectIndex].find(relocation.symbol);
            if (nameIt == outputNames[objectIndex].end()) {
                throw runtime_error("Relocation references missing output symbol: " + relocation.symbol);
            }

            outputRelocation.symbol = nameIt->second;
            outputRelocations.push_back(std::move(outputRelocation));
        }
    }

    vector<const Symbol*> orderedSymbols;
    orderedSymbols.reserve(outputSymbols.size());

    for (const auto& [name, symbol] : outputSymbols) {
        (void) name;
        orderedSymbols.push_back(&symbol);
    }

    sort(orderedSymbols.begin(), orderedSymbols.end(), [](const Symbol* first, const Symbol* second) {
        return first->name < second->name;
    });

    unordered_map<string, size_t> sectionRank;
    for (size_t index = 0; index < outputSectionOrder.size(); index++) {
        sectionRank.emplace(outputSectionOrder[index], index);
    }

    sort(outputRelocations.begin(), outputRelocations.end(), [&](const Relocation& first, const Relocation& second) {
        const size_t firstSection = sectionRank.at(first.section);
        const size_t secondSection = sectionRank.at(second.section);

        if (firstSection != secondSection) return firstSection < secondSection;
        if (first.offset != second.offset) return first.offset < second.offset;
        if (first.symbol != second.symbol) return first.symbol < second.symbol;
        if (first.type != second.type) return static_cast<int>(first.type) < static_cast<int>(second.type);
        return first.addend < second.addend;
    });

    ofstream out(outputPath);
    if (!out) throw runtime_error("Cannot open output file: " + outputPath);

    out << "MOJOBJ 1\n";
    out << "SECTIONS " << outputSectionOrder.size() << "\n";

    for (const string& sectionName : outputSectionOrder) {
        const OutputSection& section = outputSections.at(sectionName);

        out << "SECTION " << section.name << " " << section.bytes.size() << "\n";

        for (size_t index = 0; index < section.bytes.size(); index++) {
            if (index != 0) out << ' ';

            out << hex << uppercase << setw(2) << setfill('0')
                << static_cast<unsigned>(section.bytes[index]);
        }

        out << dec << "\n";
    }

    out << "SYMBOLS " << orderedSymbols.size() << "\n";

    for (const Symbol* symbol : orderedSymbols) {
        out << "SYMBOL "
            << symbol->name << " "
            << symbol->section << " "
            << symbol->offset << " "
            << symbolBindToString(symbol->bind) << " "
            << (symbol->defined ? "DEF" : "UND")
            << "\n";
    }

    out << "RELOCATIONS " << outputRelocations.size() << "\n";

    for (const Relocation& relocation : outputRelocations) {
        out << "RELOCATION "
            << relocation.section << " "
            << relocation.offset << " "
            << relocation.symbol << " "
            << relocationTypeToString(relocation.type) << " "
            << relocation.addend
            << "\n";
    }

    out << "END\n";

    if (!out) throw runtime_error("Failed while writing relocatable output: " + outputPath);
}


void Linker::writeRelocatableFile(const string& outputPath) {
    resetOutputState();
    mergeSections();
    writeMergedObjectFile(outputPath);
}
