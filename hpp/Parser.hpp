#ifndef PARSER_H___

#define PARSER_H___

#include <vector>

#include "Common.hpp"




class Assembler;


typedef struct {
    std::string label;
    bool isDirective;
    bool isInstruction;

    std::string mnemonic;
    std::vector<std::string> operands;
    int lineNumber;



} ParsedLine; //zapravo poluparsirana linija




class Parser {
public:

protected:


private:
    friend class Assembler;
    static ParsedLine parseLine(std::string line);
    static std::vector<ParsedLine> lines;


};








#endif