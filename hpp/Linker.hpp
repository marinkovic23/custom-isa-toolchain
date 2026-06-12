#ifndef __LINKER_H__
#define __LINKER_H__


#include "Common.hpp"
#include "Section.hpp"
#include "Symbol.hpp"
#include "Relocation.hpp"

typedef struct {
    std::string filename;

    std::unordered_map <std::string, Section> sections;
    std::unordered_map <std::string, Symbol> symbols;
    std::vector <Relocation> relocations;

    std::unordered_map <std::string, uint32_t> localResolvedSymbols;

} ObjectFile;


typedef struct {
    int objectIndex;
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
    uint32_t address;
} ResolvedSymbol;



class Linker {

public:
    void loadObjectFile(const std::string& filename);

protected:


private:
    std::vector<ObjectFile> objectFiles;



    std::unordered_map<std::string, OutputSection> outputSections;
    std::vector<std::string> outputSectionOrder;
    std::unordered_map<std::string, uint32_t> placeOptions;

    std::unordered_map <std::string, ResolvedSymbol> resolvedSymbols;



    void mergeSections();
    void placeSections();



    void readSections(std::ifstream& in, ObjectFile& obj);

    //helper
    SymbolBind stringToSymbolBind(const std::string& s);

    void readSymbols(std::ifstream& in, ObjectFile& obj);

    void readRelocations(std::ifstream& in, ObjectFile& obj);


    void resolveSymbols(); //vrv ce preci u public


};


#endif
