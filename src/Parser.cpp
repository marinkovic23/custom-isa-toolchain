#include "../inc/Parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>


using namespace std;

vector<ParsedLine> Parser::lines;


static string parserErrorPrefix(int lineNumber) {
    if (lineNumber <= 0) return "";
    return "Line " + to_string(lineNumber) + ": ";
}


void removeComment(string& line) {
    bool insideString = false;
    bool escaped = false;

    for (size_t index = 0; index < line.size(); index++) {
        const char character = line[index];

        if (insideString && escaped) {
            escaped = false;
            continue;
        }

        if (insideString && character == '\\') {
            escaped = true;
            continue;
        }

        if (character == '"') {
            insideString = !insideString;
            continue;
        }

        if (!insideString && character == '#') {
            line = line.substr(0, index);
            return;
        }
    }
}


void extractOperands(string& line, ParsedLine& result) {
    if (line.empty()) return;

    string current;
    bool insideString = false;
    bool escaped = false;
    int bracketDepth = 0;

    for (size_t index = 0; index < line.size(); index++) {
        const char character = line[index];

        if (insideString && escaped) {
            current.push_back(character);
            escaped = false;
            continue;
        }

        if (insideString && character == '\\') {
            current.push_back(character);
            escaped = true;
            continue;
        }

        if (character == '"') {
            insideString = !insideString;
            current.push_back(character);
            continue;
        }

        if (!insideString) {
            if (character == '[') bracketDepth++;
            if (character == ']') bracketDepth--;

            if (bracketDepth < 0) {
                throw runtime_error(parserErrorPrefix(result.lineNumber) + "Unexpected closing bracket");
            }

            if (character == ',' && bracketDepth == 0) {
                trim(current);

                if (current.empty()) {
                    throw runtime_error(parserErrorPrefix(result.lineNumber) + "Empty operand");
                }

                result.operands.push_back(current);
                current.clear();
                continue;
            }
        }

        current.push_back(character);
    }

    if (insideString) {
        throw runtime_error(parserErrorPrefix(result.lineNumber) + "Unterminated string literal");
    }

    if (bracketDepth != 0) {
        throw runtime_error(parserErrorPrefix(result.lineNumber) + "Unbalanced brackets in operand list");
    }

    trim(current);

    if (!current.empty()) {
        result.operands.push_back(current);
    }
    else if (!result.operands.empty()) {
        throw runtime_error(parserErrorPrefix(result.lineNumber) + "Empty operand after comma");
    }
}


static size_t findColonOutsideString(const string& line) {
    bool insideString = false;
    bool escaped = false;

    for (size_t index = 0; index < line.size(); index++) {
        const char character = line[index];

        if (insideString && escaped) {
            escaped = false;
            continue;
        }

        if (insideString && character == '\\') {
            escaped = true;
            continue;
        }

        if (character == '"') {
            insideString = !insideString;
            continue;
        }

        if (!insideString && character == ':') return index;
    }

    return string::npos;
}


void extractLabel(string& line, ParsedLine& result) {
    const size_t colon = findColonOutsideString(line);

    if (colon == string::npos) return;

    string beforeColon = line.substr(0, colon);
    trim(beforeColon);

    if (beforeColon.empty()) {
        throw runtime_error(parserErrorPrefix(result.lineNumber) + "Empty label");
    }

    for (char character : beforeColon) {
        if (isspace(static_cast<unsigned char>(character))) {
            throw runtime_error(parserErrorPrefix(result.lineNumber) + "Label must be at the beginning of the line");
        }
    }

    result.label = beforeColon;

    line = line.substr(colon + 1);
    trim(line);

    if (findColonOutsideString(line) != string::npos) {
        throw runtime_error(parserErrorPrefix(result.lineNumber) + "More than one label in a line");
    }
}


void extractMnemonic(string& line, ParsedLine& result) {
    if (line.empty()) return;

    stringstream ss(line);
    ss >> result.mnemonic;

    const size_t pos = line.find(result.mnemonic);
    line = line.substr(pos + result.mnemonic.size());
    trim(line);
}



//helpers
ParsedLine Parser::parseLine(string line, int lineNumber) {
    ParsedLine result;
    result.lineNumber = lineNumber;

    removeComment(line);
    trim(line);

    extractLabel(line, result);
    extractMnemonic(line, result);

    if (!result.mnemonic.empty()) {
        result.isDirective = result.mnemonic.front() == '.';
        result.isInstruction = !result.isDirective;
    }

    extractOperands(line, result);

    return result;
}
