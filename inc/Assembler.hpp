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
    INVALID,
    LITERAL,
    SYMBOL
};


typedef struct JumpTarget {
    JumpTargetKind kind = JumpTargetKind::INVALID;
    int64_t literal = 0;
    std::string symbol;

} JumpTarget;




typedef struct LiteralPoolRequest {
    uint32_t instructionOffset = 0;
    bool containsSymbol = false;
    uint32_t literal = 0;
    std::string symbol;
    int32_t addend = 0;
} LiteralPoolRequest;


typedef struct DeferredDisp12 {
    std::string section;
    uint32_t instructionOffset = 0;
    std::string symbol;
    int32_t addend = 0;
    std::string context;
} DeferredDisp12;


typedef struct EquDefinition {
    std::string expression;
    int lineNumber = 0;
} EquDefinition;


typedef struct LinearExpressionValue {
    int64_t constant = 0;
    std::unordered_map<std::string, int64_t> sectionCoefficients;
} LinearExpressionValue;



class Assembler {

public:

    void assemble(const std::string& filename);

    void writeObjectFile(const std::string& outputPath);
    
protected:
private:

    std::unordered_map<std::string, Section> sections;
    std::unordered_map<std::string, Symbol> symbols;
    std::vector<Relocation> relocations;
    Section* currentSection = nullptr;

    std::vector<std::string> sectionOrder;

    std::unordered_map<std::string, std::vector<LiteralPoolRequest>> literalPools;
    std::vector<DeferredDisp12> deferredDisp12;

    std::unordered_map<std::string, EquDefinition> equDefinitions;
    std::vector<std::string> equOrder;
    std::unordered_map<std::string, int> equVisitState;

    bool assemblyEnded = false;


    void processLine(const ParsedLine line);

    void switchSection(const std::string& sectionName);

    void processDirective(const ParsedLine& line);
    void processInstruction(const ParsedLine& line);

    void defineLabel(const std::string& name);


    void markGlobal(const std::string& name);
    void markExtern(const std::string& name);




    void emitInstruction(uint8_t oc, uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int32_t disp);
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
    bool tryParseNumber(const std::string& text, int64_t& value) const;
    int64_t parseNumber(const std::string& text, const std::string& context) const;
    bool fitsSigned12(int64_t value) const;
    uint32_t numberToWord(int64_t value, const std::string& context) const;

    bool isValidSymbolName(const std::string& name) const;
    void requireValidSymbolName(const std::string& name, const std::string& context) const;

    JumpTarget parseJumpTarget(const std::string& op);
    void addRelocation(uint32_t offset, const std::string& symbol, RelocationType type, int32_t addend = 0);
    void addRelocationForSection(const std::string& section, uint32_t offset, const std::string& symbol, RelocationType type, int32_t addend = 0);

    uint32_t currentOffset(const std::string& description) const;
    void requestLiteralPoolLiteral(uint32_t instructionOffset, uint32_t literal);
    void requestLiteralPoolSymbol(uint32_t instructionOffset, const std::string& symbol, int32_t addend = 0);
    void patchDisp12(std::vector<uint8_t>& bytes, uint32_t instructionOffset, int64_t value, const std::string& context) const;
    void appendWord(Section& section, uint32_t value) const;


    //continuation of instrution encoding
    void encodeJump(const ParsedLine& line);
    void encodeCall(const ParsedLine& line);

    void encodeBranch(const ParsedLine& line, uint8_t mode);

    Operand parseOperand(const std::string& operand);


    void encodeLd(const ParsedLine& line);
    void encodeSt(const ParsedLine& line);

    void encodeIret(const ParsedLine& line);



    void processWordDirective(const ParsedLine& line);
    void processSkipDirective(const ParsedLine& line);
    void processAsciiDirective(const ParsedLine& line);
    void processEquDirective(const ParsedLine& line);
    std::vector<uint8_t> decodeAsciiString(const std::string& operand, int lineNumber) const;

    void resolveEquDefinitions();
    LinearExpressionValue evaluateEquSymbol(const std::string& name);
    LinearExpressionValue evaluateExpression(const std::string& expression, int lineNumber);
    void finalizeLiteralPools();
    void resolveDeferredDisp12();
    void resolveAssemblerKnownRelocations();
    void finalizeAssembly();



    std::string symbolBindToString(SymbolBind bind);

    void validateObjectState() const;
   
};





#endif
