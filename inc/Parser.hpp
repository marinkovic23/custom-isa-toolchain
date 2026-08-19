#ifndef PARSER_H___

#define PARSER_H___

#include <vector>

#include "Common.hpp"




class Assembler;


typedef struct {
    std::string label;
    bool isDirective = false;
    bool isInstruction = false;

    std::string mnemonic;
    std::vector<std::string> operands;
    int lineNumber = 0;



} ParsedLine; //zapravo poluparsirana linija




class Parser {
public:

protected:


private:
    friend class Assembler;
    static ParsedLine parseLine(std::string line, int lineNumber = 0);
    static std::vector<ParsedLine> lines;


};








#endif
