#ifndef ASSEMBLER_HPP___


#define ASSEMBLER_HPP___

#include <unordered_map>
#include <string>
#include <cstdint>

#include "Parser.hpp"

#include "Section.hpp"
#include "Symbol.hpp"
#include "Relocation.hpp"

#include "Operand.hpp"



//zasad, posle ce nesto drugo
enum class JumpTargetKind {
    LITERAL,
    SYMBOL
};


typedef struct {
    JumpTargetKind kind;
    int32_t literal = 0;
    std::string symbol;

} JumpTarget;





class Assembler {

public:

    void assemble(const std::string& filename);

    void writeObjectFile(const std::string& outputPath);
    
protected:
private:

    std::unordered_map<std::string, Section> sections;
    std::unordered_map<std::string, Symbol> symbols;
    std::vector<Relocation> relocations;
    Section* currentSection;


    void processLine(const ParsedLine line);

    void switchSection(const std::string line);

    void processDirective(const ParsedLine& line);
    void processInstruction(const ParsedLine& line);

    void defineLabel(const std::string& name);


    void markGlobal(const std::string& name);
    void markExtern(const std::string& name);




    void emitInstruction(uint8_t oc, uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int16_t disp);
    void emitByte(uint8_t value);
    void emitWord(uint32_t value);



    uint8_t parseGpr(const std::string& operand);

    uint8_t parseCsr(const std::string& operand);


  
    void encodeArithmetic(const ParsedLine& line, uint8_t mode);
    void encodeLogicBinary(const ParsedLine& line, uint8_t mode);
    void encodeNot(const ParsedLine& line);
    void encodeShift(const ParsedLine& line, uint8_t mode);
    void encodeXchg(const ParsedLine& line);


    void encodePopToRegister(uint8_t reg);
    void encodePush(const ParsedLine& line);
    void encodePop(const ParsedLine& line);
    void encodeRet(const ParsedLine& line);


    void encodeInt(const ParsedLine& line);
    void encodeCsrrd(const ParsedLine& line);
    void encodeCsrwr(const ParsedLine& line);



    // helpers here
    bool isNumber(const std::string& s);
    JumpTarget parseJumpTarget(const string& op);
    void addRelocation(uint32_t offset, const std::string& symbol);


    //continuation of instrution encoding
    void encodeJump(const ParsedLine& line);
    void encodeCall(const ParsedLine& line);

    void encodeBranch(const ParsedLine& line, uint8_t mode);

    Operand parseOperand(const string& operand);


    void encodeLd(const ParsedLine& line);
    void encodeSt(const ParsedLine& line);

    void encodeIret(const ParsedLine& line);




    std::string symbolBindToString(SymbolBind bind);
   
};





#endif