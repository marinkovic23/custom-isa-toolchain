#include "../hpp/Parser.hpp"

#include <cctype>
#include <iostream>
#include <fstream>
#include <stringstream>


using namespace std;

vector<ParsedLine> lines;


//helpers
ParsedLine Parser::parseLine(string line) {
    line = removeComment(line);
    trim(line);
    ParsedLine result;

    extractLabel(line, result);
    extractMnemonic(line, result);
    extractOperands(line, result);


    return result;
}


void extractLabel(string& line, ParsedLine& result) {
    size_t colon = line.find(':');

    if (colon == string::npos) return;

    string beforeColon = line.susbtr(0, colon);
    
    trim(beforeColon);


    if (!beforeColon.empty()) {
        result.label = beforeColon;

        line = line.substr(colon + 1);
        trim(line);
    }
}

void extractMnemonic(string& line, ParsedLine& result) {
    if (line.empty()) return;


    stringstream ss(line);

    ss >>result.mnemonic;

    size_t pos = line.find(result.mnemonic);

    line = line.substr(pos + result.mnemonic.size());

    trim(line);

}

void extractOperands(string& line, ParsedLine& result) {
    string current;
    stringstream ss(line);

    while(getline(ss, current, ',')) {
        trim(current);
        if(!current.empty()) {
            result.operands.push_back(current);
        }
    }

}


void trim(string& line) {

    size_t start = 0;
    while (start < line.size() && isspace(line[start])) start++;
    size_t end = line.size();

    while(end > start && isspace(line[end - 1])) end--;
    line = line.substr(start, end - start);
}

void removeComment(string& line) {
    size_t pos = line.find('#');
    if (pos != string::npos) line = line.substr(0, pos);
}







