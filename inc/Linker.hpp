#ifndef __LINKER_H__
#define __LINKER_H__


#include "Common.hpp"
#include "Section.hpp"
#include "Symbol.hpp"
#include "Relocation.hpp"

#include <fstream>


typedef struct {
    std::string filename;

    std::unordered_map <std::string, Section> sections;
    std::vector<std::string> sectionOrder;
    std::unordered_map <std::string, Symbol> symbols;
    std::vector <Relocation> relocations;

    std::unordered_map <std::string, uint32_t> localResolvedSymbols;

} ObjectFile;


typedef struct {
    int objectIndex = -1;
    std::string inputSectionName;
    std::string outputSectionName;
    uint32_t inputOffset = 0;
    uint32_t outputOffset = 0;
    uint32_t size = 0;

} PlacedSectionPart;

typedef struct {
    std::string name;
    uint32_t baseAddress = 0;
    uint32_t size = 0;
    std::vector <uint8_t> bytes;
} OutputSection;

typedef struct {
    std::string name;
    uint32_t address = 0;
} ResolvedSymbol;



class Linker {

public:
    void loadObjectFile(const std::string& filename);

    void setPlaceOptions(const std::unordered_map<std::string, uint32_t>& placements);

    void writeHexFile(const std::string& outputPath);

    void writeRelocatableFile(const std::string& outputPath);

protected:


private:
    std::vector<ObjectFile> objectFiles;



    std::unordered_map<std::string, OutputSection> outputSections;
    std::vector<std::string> outputSectionOrder;
    std::unordered_map<std::string, uint32_t> placeOptions;

    std::unordered_map <std::string, ResolvedSymbol> resolvedSymbols;



    void resetOutputState();
    void validateObjectFile(const ObjectFile& obj) const;

    void mergeSections();
    void placeSections();



    void readSections(std::ifstream& in, ObjectFile& obj);

    //helper
    SymbolBind stringToSymbolBind(const std::string& s);

    void readSymbols(std::ifstream& in, ObjectFile& obj);

    void readRelocations(std::ifstream& in, ObjectFile& obj);


    void resolveSymbols(); //vrv ce preci u public

    uint32_t resolveSymbolForObject(const ObjectFile& obj, const std::string& symbol);

    void write32(std::vector<uint8_t>& bytes, uint32_t offset, uint32_t value);

    void writeDisp12(std::vector<uint8_t>& bytes, uint32_t offset, int64_t value);

    void applyRelocations();

    void writeMergedObjectFile(const std::string& outputPath);

    
};


#endif
