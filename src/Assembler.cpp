#include "../inc/Assembler.hpp"



#include <fstream>
#include <iostream>
#include <iomanip>


#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <sstream>
#include <functional>



using namespace std;




static bool containsWhiteSpace(const string& value) {
    for (size_t i = 0; i < value.size(); i++) {
        if (isspace(static_cast<unsigned char>(value[i]))) return true;
    }
    return false;
}



void Assembler::validateObjectState() const {
    if (sectionOrder.size() != sections.size()) {
        throw runtime_error("Internal error: section order does not match the section table");
    }

    unordered_set<string> seenSections;

    for (const string& sectionName: sectionOrder) {
        if (sectionName.empty()) {
            throw runtime_error("Section name cannot be empty");
        }

        if (containsWhiteSpace(sectionName)) {
            throw runtime_error("Section name contains whitespace: " + sectionName);
        }


        if (sectionName == "UND" || sectionName == "ABS") {
            throw runtime_error("Reserved section name used as a real section: " + sectionName);
        }

        if (!seenSections.insert(sectionName).second) {
            throw runtime_error("Section appears more than once in section order: " + sectionName);
        }

        if (sections.find(sectionName) == sections.end()) {
            throw runtime_error("Section order references unknown section: " + sectionName);
        }
    }


    for (const auto& [tableName, symbol] : symbols) {
        if (symbol.name.empty()) {
            throw runtime_error("Symbol has an empty name");
        }

        if (tableName != symbol.name) {
            throw runtime_error("Symbol-table key does not match symbol name" + tableName);
        }

        if (containsWhiteSpace(symbol.name)) {
            throw runtime_error("Symbol name contains whitespace: " + symbol.name);
        }

        if (symbol.defined) {
            if (symbol.bind == SymbolBind::EXTERN) {
                throw runtime_error("External symbol is also defined: " + symbol.name);
            }

            if (symbol.section != "ABS" && sections.find(symbol.section) == sections.end()) {
                throw runtime_error("Defined symbol references unknown section: " + symbol.name + " -> " + symbol.section);
            }

            if (symbol.section != "ABS") {
                const Section& section = sections.at(symbol.section);
                if (static_cast<size_t>(symbol.offset) > section.bytes.size()) {
                    throw runtime_error("Defined symbol is outside of its section: " + symbol.name);
                }
            }
        }
        else {
            if (symbol.section != "UND") {
                throw runtime_error("Undefined symbol does not belong to UND: " + symbol.name);
            }

            //local references should have been defined by the end of the assembly.
            //GLOBAL and EXTERN undefined symbols are left for the linker.
            if (symbol.bind == SymbolBind::LOCAL) {
                throw runtime_error("Unresolved local symbol: " + symbol.name);
            }
        }


    }


    for (const Relocation& relocation : relocations) {
        auto sectionIt = sections.find(relocation.section);

        if (sectionIt == sections.end()) {
            throw runtime_error("Relocation references unknown sections: " + relocation.section);
        }

        if (symbols.find(relocation.symbol) == symbols.end()) {
            throw runtime_error("Relocation references unknown symbol: " + relocation.symbol);
        }

        const size_t sectionSize = sectionIt->second.bytes.size();




        //both relocation types patch fields belongign to a four byt eobject

        if (relocation.offset > sectionSize || sectionSize - relocation.offset < 4) {
            throw runtime_error("Relocation exceeds section bounds : section=" + relocation.section + ", symbol=" + relocation.symbol);
        }

    }
}



void Assembler::assemble(const string& filename) {

    ifstream file(filename);
    if (!file) throw runtime_error("Cannot open file: " + filename);

    sections.clear();
    symbols.clear();
    relocations.clear();
    sectionOrder.clear();
    literalPools.clear();
    deferredDisp12.clear();
    equDefinitions.clear();
    equOrder.clear();
    equVisitState.clear();
    currentSection = nullptr;
    assemblyEnded = false;

    string line;
    int lineNumber = 0;

   
    while(!assemblyEnded && getline(file, line)) {
        lineNumber++;

        try {
            ParsedLine parsed = Parser::parseLine(line, lineNumber);
            processLine(parsed);
        }
        catch (const exception& error) {
            const string message = error.what();
            if (message.rfind("Line ", 0) == 0) throw;
            throw runtime_error("Line " + to_string(lineNumber) + ": " + message);
        }
    }

    finalizeAssembly();
}


//helper
//helper
void Assembler::switchSection(const string& sectionName) {
    auto it = sections.find(sectionName);

    if (it == sections.end()) {
        Section section;
        section.name = sectionName;

        sections.emplace(sectionName, std::move(section));
        sectionOrder.push_back(sectionName);
    }

    currentSection = &sections.at(sectionName);
}




void Assembler::processDirective(const ParsedLine& line) {
    if (line.mnemonic == ".section") {

        if (line.operands.size() != 1) {
            throw runtime_error(".section expects exactly one operand");
        }

        if (line.operands[0] == "UND" || line.operands[0] == "ABS") {
            throw runtime_error("Reserved section name: " + line.operands[0]);
        }

        switchSection(line.operands[0]);
    }
    else if (line.mnemonic == ".global") {

        if (line.operands.empty()) {
            throw runtime_error(".global expects at least one symbol");
        }

        for (const string& name : line.operands) {
            markGlobal(name);
        }
    }
    else if (line.mnemonic == ".extern") {

        if (line.operands.empty()) {
            throw runtime_error(".extern expects at least one symbol");
        }
        for (const string& name : line.operands) {
            markExtern(name);
        }
    }
    else if (line.mnemonic == ".word") {
        processWordDirective(line);
    }
    else if (line.mnemonic == ".skip") {
        processSkipDirective(line);
    }
    else if (line.mnemonic == ".ascii") {
        processAsciiDirective(line);
    }
    else if (line.mnemonic == ".equ") {
        processEquDirective(line);
    }
    else if (line.mnemonic == ".end") {
        if (!line.operands.empty()) {
            throw runtime_error(".end expects no operands");
        }
        assemblyEnded = true;
    }
    else {
        throw runtime_error("Unknown directive: " + line.mnemonic);
    }
}

void Assembler::emitByte(uint8_t byte) {
    if (currentSection == nullptr) throw runtime_error("No active section");

    currentSection->bytes.push_back(byte);
}

void Assembler::emitWord(uint32_t value) {
    emitByte(static_cast<uint8_t>(value & 0xFFu));
    emitByte(static_cast<uint8_t>((value >> 8) & 0xFFu));
    emitByte(static_cast<uint8_t>((value >> 16) & 0xFFu));
    emitByte(static_cast<uint8_t>((value >> 24) & 0xFFu));
}




void Assembler::emitInstruction(uint8_t oc, uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int32_t disp) {
    if (disp < -2048 || disp > 2047) {
        throw runtime_error("Instruction displacement does not fit in signed 12 bits: " + to_string(disp));
    }

    const uint32_t d = static_cast<uint32_t>(disp) & 0xFFFu;

    const uint8_t b0 = static_cast<uint8_t>((static_cast<uint32_t>(oc) << 4) | mode);
    const uint8_t b1 = static_cast<uint8_t>((static_cast<uint32_t>(regA) << 4) | regB);
    const uint8_t b2 = static_cast<uint8_t>((static_cast<uint32_t>(regC) << 4) | ((d >> 8) & 0xFu));
    const uint8_t b3 = static_cast<uint8_t>(d & 0xFFu);

    emitByte(b0);
    emitByte(b1);
    emitByte(b2);
    emitByte(b3);
}


uint8_t Assembler::parseGpr(const string& operand) {
    if (operand == "%sp") return 14;

    if (operand == "%pc") return 15;

    if (operand.size() < 3) throw runtime_error("Invalid register: " + operand);
    if (operand[0] != '%' || operand[1] != 'r') throw runtime_error("Invalid register: " + operand);

    const string indexText = operand.substr(2);
    for (char character : indexText) {
        if (!isdigit(static_cast<unsigned char>(character))) {
            throw runtime_error("Invalid register: " + operand);
        }
    }

    const int reg = stoi(indexText);

    if (reg < 0 || reg > 15) throw runtime_error("Invalid register: " + operand);

    return static_cast<uint8_t>(reg);
}


uint8_t Assembler::parseCsr(const string& operand) {
    if (operand == "%status") return 0;
    if (operand == "%handler") return 1;
    if (operand == "%cause") return 2;

    throw runtime_error("Invalid CSR register: " + operand);

}



void Assembler::encodeArithmetic(const ParsedLine& line, uint8_t mode) {
    if (line.operands.size() != 2) throw runtime_error(line.mnemonic + " expects 2 operends");

    uint8_t src = parseGpr(line.operands[0]);
    uint8_t dst = parseGpr(line.operands[1]);

    emitInstruction(0x5, mode, dst, dst, src, 0);
}


void Assembler::encodeLogicBinary(const ParsedLine& line, uint8_t mode) {
    if (line.operands.size() != 2) throw runtime_error(line.mnemonic + " expects 2 operands");


    uint8_t src = parseGpr(line.operands[0]);
    uint8_t dst = parseGpr(line.operands[1]);

    emitInstruction(0x6, mode, dst, dst, src, 0);
}

void Assembler::encodeNot(const ParsedLine& line) {
    if (line.operands.size() != 1) throw runtime_error("not expects 1 operand");


    uint8_t reg = parseGpr(line.operands[0]);

    emitInstruction(0x6, 0x0, reg, reg, 0, 0);
}

void Assembler::encodeShift(const ParsedLine& line, uint8_t mode) {

    if (line.operands.size() != 2) {
        throw runtime_error(line.mnemonic + " expects 2 operands");
    }
    uint8_t src = parseGpr(line.operands[0]);
    uint8_t dst = parseGpr(line.operands[1]);

    emitInstruction(0x7, mode, dst, dst, src, 0);
}

void Assembler::encodeXchg(const ParsedLine& line) {
    if (line.operands.size() != 2) throw runtime_error("xchg expects 2 operands");

    uint8_t reg1 = parseGpr(line.operands[0]);
    uint8_t reg2 = parseGpr(line.operands[1]);

    emitInstruction(0x4, 0x0, 0, reg1, reg2, 0);
}

void Assembler::encodePush(const ParsedLine& line) {
    if (line.operands.size() != 1) throw runtime_error("push expects 1 operand");

    uint8_t reg = parseGpr(line.operands[0]);

    emitInstruction(0x8, 0x1, 14, 0, reg, -4);
}


//helper nivoa 2
void Assembler::encodePopToRegister(uint8_t reg) {
    emitInstruction(0x9, 0x3, reg, 14, 0, 4);
}


void Assembler::encodePop(const ParsedLine& line) {
    if (line.operands.size() != 1) throw runtime_error("pop expects 1 operand");

    uint8_t reg = parseGpr(line.operands[0]);


    encodePopToRegister(reg);
}

void Assembler::encodeRet(const ParsedLine& line) {
    if (!line.operands.empty()) throw runtime_error("ret expects no operands");

    encodePopToRegister(15);
}


void Assembler::encodeInt(const ParsedLine& line) {
    if (!line.operands.empty()) throw runtime_error("int expects no operands");

    emitInstruction(0x1, 0x0, 0, 0, 0, 0);
}

void Assembler::encodeCsrrd(const ParsedLine& line) {
    if (line.operands.size() != 2) throw runtime_error("csrrd expects 2 operands");

    uint8_t csr = parseCsr(line.operands[0]);
    uint8_t gpr = parseGpr(line.operands[1]);

    emitInstruction(0x9, 0x0, gpr, csr, 0, 0);
}

void Assembler::encodeCsrwr(const ParsedLine& line) {
    if (line.operands.size() != 2) throw runtime_error("csrwr expects 2 operands");

    uint8_t gpr = parseGpr(line.operands[0]);
    uint8_t csr = parseCsr(line.operands[1]);

    emitInstruction(0x9, 0x4, csr, gpr, 0, 0);
}








//helperi za jmp

bool Assembler::tryParseNumber(const string& text, int64_t& value) const {
    string number = text;
    trim(number);

    if (number.empty()) return false;

    bool negative = false;
    size_t start = 0;

    if (number[0] == '+' || number[0] == '-') {
        negative = number[0] == '-';
        start = 1;
    }

    if (start == number.size()) return false;

    int base = 10;
    if (number.size() >= start + 2 && number[start] == '0' && (number[start + 1] == 'x' || number[start + 1] == 'X')) {
        base = 16;
        start += 2;
    }

    if (start == number.size()) return false;

    for (size_t index = start; index < number.size(); index++) {
        const unsigned char character = static_cast<unsigned char>(number[index]);

        if (base == 10) {
            if (!isdigit(character)) return false;
        }
        else {
            if (!isxdigit(character)) return false;
        }
    }

    const string digits = number.substr(start);
    size_t processed = 0;
    unsigned long long magnitude = 0;

    try {
        magnitude = stoull(digits, &processed, base);
    }
    catch (const exception&) {
        return false;
    }

    if (processed != digits.size()) return false;

    if (negative) {
        if (magnitude > 0x80000000ULL) return false;
        value = -static_cast<int64_t>(magnitude);
    }
    else {
        if (magnitude > 0xFFFFFFFFULL) return false;
        value = static_cast<int64_t>(magnitude);
    }

    return true;
}


bool Assembler::isNumber(const std::string& s) {
    int64_t ignored = 0;
    return tryParseNumber(s, ignored);
}


int64_t Assembler::parseNumber(const string& text, const string& context) const {
    int64_t value = 0;

    if (!tryParseNumber(text, value)) {
        throw runtime_error("Invalid numeric literal for " + context + ": " + text);
    }

    return value;
}


bool Assembler::fitsSigned12(int64_t value) const {
    return value >= -2048 && value <= 2047;
}


uint32_t Assembler::numberToWord(int64_t value, const string& context) const {
    if (value < static_cast<int64_t>(numeric_limits<int32_t>::min()) || value > static_cast<int64_t>(numeric_limits<uint32_t>::max())) {
        throw runtime_error(context + " does not fit in 32 bits: " + to_string(value));
    }

    return static_cast<uint32_t>(value);
}


bool Assembler::isValidSymbolName(const string& name) const {
    if (name.empty()) return false;

    const unsigned char first = static_cast<unsigned char>(name.front());
    if (!(isalpha(first) || name.front() == '_' || name.front() == '.' || name.front() == '$')) return false;

    for (size_t index = 1; index < name.size(); index++) {
        const unsigned char character = static_cast<unsigned char>(name[index]);
        if (!(isalnum(character) || name[index] == '_' || name[index] == '.' || name[index] == '$')) return false;
    }

    return true;
}


void Assembler::requireValidSymbolName(const string& name, const string& context) const {
    if (!isValidSymbolName(name)) {
        throw runtime_error("Invalid symbol name in " + context + ": " + name);
    }
}


JumpTarget Assembler::parseJumpTarget(const string& op) {
    JumpTarget target;
    int64_t literal = 0;

    if (tryParseNumber(op, literal)) {
        target.kind = JumpTargetKind::LITERAL;
        target.literal = literal;
    }
    else {
        requireValidSymbolName(op, "jump target");
        target.kind = JumpTargetKind::SYMBOL;
        target.symbol = op;
    }

    return target;
}


void Assembler::addRelocation(uint32_t offset, const string& symbol, RelocationType type, int32_t addend) {
    if (currentSection == nullptr) {
        throw runtime_error("Cannot create relocation outside of a section");
    }

    addRelocationForSection(currentSection->name, offset, symbol, type, addend);
}


void Assembler::addRelocationForSection(const string& section, uint32_t offset, const string& symbol, RelocationType type, int32_t addend) {
    if (symbol.empty()) {
        throw runtime_error("Cannot create relocation for an empty symbol name");
    }

    requireValidSymbolName(symbol, "relocation");

    auto it = symbols.find(symbol);

    if (it == symbols.end()) {

        Symbol placeholder;
        placeholder.name = symbol;
        placeholder.section = "UND";
        placeholder.offset = 0;
        placeholder.bind = SymbolBind::LOCAL;
        placeholder.defined = false;

        symbols.emplace(symbol, std::move(placeholder));
    }

    Relocation r;
    r.section = section;
    r.offset = offset;
    r.symbol = symbol;
    r.type = type;
    r.addend = addend;

    relocations.push_back(r);
}


static uint32_t checkedUint32Size(size_t size, const string& description) {
    if (size > static_cast<size_t>(numeric_limits<uint32_t>::max())) {
        throw runtime_error(description + " exceeds 32-bit range");
    }

    return static_cast<uint32_t>(size);
}

uint32_t Assembler::currentOffset(const string& description) const {
    if (currentSection == nullptr) {
        throw runtime_error(description + " requested outside of a section");
    }

    return checkedUint32Size(currentSection->bytes.size(), description);
}


void Assembler::requestLiteralPoolLiteral(uint32_t instructionOffset, uint32_t literal) {
    if (currentSection == nullptr) throw runtime_error("Literal pool request outside of a section");

    LiteralPoolRequest request;
    request.instructionOffset = instructionOffset;
    request.containsSymbol = false;
    request.literal = literal;

    literalPools[currentSection->name].push_back(request);
}


void Assembler::requestLiteralPoolSymbol(uint32_t instructionOffset, const string& symbol, int32_t addend) {
    if (currentSection == nullptr) throw runtime_error("Literal pool request outside of a section");

    requireValidSymbolName(symbol, "literal pool");

    if (symbols.find(symbol) == symbols.end()) {
        Symbol placeholder;
        placeholder.name = symbol;
        symbols.emplace(symbol, placeholder);
    }

    LiteralPoolRequest request;
    request.instructionOffset = instructionOffset;
    request.containsSymbol = true;
    request.symbol = symbol;
    request.addend = addend;

    literalPools[currentSection->name].push_back(request);
}


void Assembler::patchDisp12(vector<uint8_t>& bytes, uint32_t instructionOffset, int64_t value, const string& context) const {
    const size_t offset = static_cast<size_t>(instructionOffset);

    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw runtime_error(context + " points outside of the instruction stream");
    }

    if (!fitsSigned12(value)) {
        throw runtime_error(context + " does not fit in signed 12 bits: " + to_string(value));
    }

    const uint32_t encoded = static_cast<uint32_t>(value) & 0xFFFu;

    bytes[offset + 2] = static_cast<uint8_t>(
        (static_cast<uint32_t>(bytes[offset + 2]) & 0xF0u) |
        ((encoded >> 8) & 0x0Fu)
    );
    bytes[offset + 3] = static_cast<uint8_t>(encoded & 0xFFu);
}


void Assembler::appendWord(Section& section, uint32_t value) const {
    section.bytes.push_back(static_cast<uint8_t>(value & 0xFFu));
    section.bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    section.bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    section.bytes.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}


void Assembler::encodeJump(const ParsedLine& line) {
    if (line.operands.size() != 1) throw runtime_error("jmp expects one operand");

    JumpTarget target = parseJumpTarget(line.operands[0]);

    if (target.kind == JumpTargetKind::LITERAL && fitsSigned12(target.literal)) {
        emitInstruction(0x3, 0x0, 0, 0, 0, static_cast<int32_t>(target.literal));
        return;
    }

    const uint32_t instructionOffset = currentOffset("jmp instruction offset");
    emitInstruction(0x3, 0x8, 15, 0, 0, 0);

    if (target.kind == JumpTargetKind::LITERAL) {
        requestLiteralPoolLiteral(instructionOffset, numberToWord(target.literal, "jmp target"));
    }
    else {
        requestLiteralPoolSymbol(instructionOffset, target.symbol);
    }
}


void Assembler::encodeCall(const ParsedLine& line) {
    if (line.operands.size() != 1) throw runtime_error("call expects 1 operand");

    JumpTarget target = parseJumpTarget(line.operands[0]);

    if (target.kind == JumpTargetKind::LITERAL && fitsSigned12(target.literal)) {
        emitInstruction(0x2, 0x0, 0, 0, 0, static_cast<int32_t>(target.literal));
        return;
    }

    const uint32_t instructionOffset = currentOffset("call instruction offset");

    //mode 1 performs an indirect call through the literal pool
    emitInstruction(0x2, 0x1, 15, 0, 0, 0);

    if (target.kind == JumpTargetKind::LITERAL) {
        requestLiteralPoolLiteral(instructionOffset, numberToWord(target.literal, "call target"));
    }
    else {
        requestLiteralPoolSymbol(instructionOffset, target.symbol);
    }
}


void Assembler::encodeBranch(const ParsedLine& line, uint8_t mode) {
    if (line.operands.size() != 3) throw runtime_error(line.mnemonic + " expects 3 operands");

    uint8_t reg1 = parseGpr(line.operands[0]);
    uint8_t reg2 = parseGpr(line.operands[1]);

    JumpTarget target = parseJumpTarget(line.operands[2]);

    if (target.kind == JumpTargetKind::LITERAL && fitsSigned12(target.literal)) {
        emitInstruction(0x3, mode, 0, reg1, reg2, static_cast<int32_t>(target.literal));
        return;
    }

    uint8_t indirectMode;

    if (mode == 0x1) indirectMode = 0x9;      // beq indirect

    else if (mode == 0x2) indirectMode = 0xA; // bne indirect

    else if (mode == 0x3) indirectMode = 0xB; // bgt indirect

    else throw runtime_error("Invalid branch mode");

    const uint32_t instructionOffset = currentOffset(line.mnemonic + " instruction offset");
    emitInstruction(0x3, indirectMode, 15, reg1, reg2, 0);

    if (target.kind == JumpTargetKind::LITERAL) {
        requestLiteralPoolLiteral(instructionOffset, numberToWord(target.literal, line.mnemonic + " target"));
    }
    else {
        requestLiteralPoolSymbol(instructionOffset, target.symbol);
    }
}



Operand Assembler::parseOperand(const string& raw) {
    string op = raw;
    trim(op);


    Operand result;

    if (op.empty()) return result;

    // $literal or $symbol
    if (op[0] == '$') {
        string value = op.substr(1);
        trim(value);

        if (value.empty()) {
            throw runtime_error("Missing immediate operand after $");
        }

        int64_t literal = 0;

        if (tryParseNumber(value, literal)) {
            result.kind = OperandKind::IMMEDIATE_LITERAL;
            result.literal = static_cast<int64_t>(numberToWord(literal, "immediate operand"));
        }
        else {
            requireValidSymbolName(value, "immediate operand");
            result.kind = OperandKind::IMMEDIATE_SYMBOL;
            result.symbol = value;
        }

        return result;
    }

    // %r1, %sp, %pc
    if (op[0] == '%') {
        result.kind = OperandKind::REGISTER;
        result.reg = parseGpr(op);
        return result;
    }

    // [%r1] or [%r1 + 4] ili [%r1 + symbol]
    if (op.size() >= 2 && op.front() == '[' && op.back() == ']') {
        string inside = op.substr(1, op.size() - 2);
        trim(inside);

        const size_t plusPos = inside.find('+');

        if (plusPos == string::npos) {
            result.kind = OperandKind::REGISTER_INDIRECT;
            result.reg = parseGpr(inside);
            return result;
        }

        if (inside.find('+', plusPos + 1) != string::npos) {
            throw runtime_error("Invalid register-indirect operand: " + raw);
        }

        string regPart = inside.substr(0, plusPos);
        trim(regPart);
        string dispPart = inside.substr(plusPos + 1);
        trim(dispPart);

        if (dispPart.empty()) {
            throw runtime_error("Missing displacement in operand: " + raw);
        }

        result.reg = parseGpr(regPart);

        int64_t literal = 0;
        if (tryParseNumber(dispPart, literal)) {
            if (!fitsSigned12(literal)) {
                throw runtime_error("Register-indirect displacement does not fit in signed 12 bits: " + dispPart);
            }

            result.kind = OperandKind::REGISTER_INDIRECT_LITERAL;
            result.literal = static_cast<int32_t>(literal);
        }
        else {
            requireValidSymbolName(dispPart, "register-indirect operand");
            result.kind = OperandKind::REGISTER_INDIRECT_SYMBOL;
            result.symbol = dispPart;
        }

        return result;
    }

    int64_t literal = 0;
    if (tryParseNumber(op, literal)) {
        result.kind = OperandKind::MEMORY_LITERAL;
        result.literal = static_cast<int64_t>(numberToWord(literal, "memory operand"));
    }
    else {
        requireValidSymbolName(op, "memory operand");
        result.kind = OperandKind::MEMORY_SYMBOL;
        result.symbol = op;
    }

    return result;
}


void Assembler::encodeLd(const ParsedLine& line) {
    if (line.operands.size() != 2) throw runtime_error("ld expects 2 operands");

    Operand src = parseOperand(line.operands[0]);
    uint8_t dst = parseGpr(line.operands[1]);

    switch (src.kind) {
        case OperandKind::INVALID:
            throw runtime_error("Invalid source operand for ld: " + line.operands[0]);

        case OperandKind::IMMEDIATE_LITERAL: {
            const uint32_t value = static_cast<uint32_t>(src.literal);
            const int64_t signedValue = value <= 0x7FFFFFFFu
                ? static_cast<int64_t>(value)
                : static_cast<int64_t>(static_cast<int32_t>(value));

            //gpr[A] <= gpr[B] + D
            //dst <= r0 + literal
            if (fitsSigned12(signedValue)) {
                emitInstruction(0x9, 0x1, dst, 0, 0, static_cast<int32_t>(signedValue));
            }
            else {
                const uint32_t instructionOffset = currentOffset("ld immediate instruction offset");
                emitInstruction(0x9, 0x2, dst, 15, 0, 0);
                requestLiteralPoolLiteral(instructionOffset, value);
            }
            break;
        }


        case OperandKind::IMMEDIATE_SYMBOL: {
            const uint32_t instructionOffset = currentOffset("ld immediate-symbol instruction offset");
            emitInstruction(0x9, 0x2, dst, 15, 0, 0);
            requestLiteralPoolSymbol(instructionOffset, src.symbol);
            break;
        }


        case OperandKind::REGISTER:
            // dst <= src.reg + 0
            emitInstruction(0x9, 0x1, dst, src.reg, 0, 0);
            break;


        case OperandKind::REGISTER_INDIRECT:
            // dst <= mem32[reg + r0 + 0]
            emitInstruction(0x9, 0x2, dst, src.reg, 0, 0);
            break;


        case OperandKind::REGISTER_INDIRECT_LITERAL:
            // dst <= mem32[reg + r0 + literal]
            emitInstruction(0x9, 0x2, dst, src.reg, 0, static_cast<int32_t>(src.literal));
            break;



        case OperandKind::REGISTER_INDIRECT_SYMBOL: {
            //sme samo ako je simbol poznat i staej u 12 bita
            const uint32_t instructionOffset = currentOffset("ld register-indirect instruction offset");
            emitInstruction(0x9, 0x2, dst, src.reg, 0, 0);
            deferredDisp12.push_back({currentSection->name, instructionOffset, src.symbol, 0, "ld [%reg + symbol]"});
            break;
        }


        case OperandKind::MEMORY_LITERAL: {
            const uint32_t address = static_cast<uint32_t>(src.literal);
            const int64_t signedAddress = address <= 0x7FFFFFFFu
                ? static_cast<int64_t>(address)
                : static_cast<int64_t>(static_cast<int32_t>(address));

            if (fitsSigned12(signedAddress)) {
                //dst <= mem32[r0 + r0 + literal]
                emitInstruction(0x9, 0x2, dst, 0, 0, static_cast<int32_t>(signedAddress));
            }
            else {
                //first load the full address from the literal pool, then dereference it
                const uint32_t instructionOffset = currentOffset("ld memory-literal instruction offset");
                emitInstruction(0x9, 0x2, dst, 15, 0, 0);
                requestLiteralPoolLiteral(instructionOffset, address);
                emitInstruction(0x9, 0x2, dst, dst, 0, 0);
            }
            break;
        }
        
        case OperandKind::MEMORY_SYMBOL: {
            //first load the address of the symbol, then load the value at that address
            const uint32_t instructionOffset = currentOffset("ld memory-symbol instruction offset");
            emitInstruction(0x9, 0x2, dst, 15, 0, 0);
            requestLiteralPoolSymbol(instructionOffset, src.symbol);
            emitInstruction(0x9, 0x2, dst, dst, 0, 0);
            break;
        }
    }
}



void Assembler::encodeSt(const ParsedLine& line) {
    if (line.operands.size() != 2) throw runtime_error("st expects 2 operands");

    uint8_t src = parseGpr(line.operands[0]);
    Operand dst = parseOperand(line.operands[1]);

    switch (dst.kind) {



        case OperandKind::REGISTER_INDIRECT:

            // mem32[reg + r0 + 0] <= src
            emitInstruction(0x8, 0x0, dst.reg, 0, src, 0);
            break;

        case OperandKind::REGISTER_INDIRECT_LITERAL:
            // mem32[reg + r0 + literal] <= src
            emitInstruction(0x8, 0x0, dst.reg, 0, src, static_cast<int32_t>(dst.literal));
            break;

        case OperandKind::REGISTER_INDIRECT_SYMBOL: {
            const uint32_t instructionOffset = currentOffset("st register-indirect instruction offset");
            emitInstruction(0x8, 0x0, dst.reg, 0, src, 0);
            deferredDisp12.push_back({currentSection->name, instructionOffset, dst.symbol, 0, "st [%reg + symbol]"});
            break;
        }
            
        case OperandKind::MEMORY_LITERAL: {
            const uint32_t address = static_cast<uint32_t>(dst.literal);
            const int64_t signedAddress = address <= 0x7FFFFFFFu
                ? static_cast<int64_t>(address)
                : static_cast<int64_t>(static_cast<int32_t>(address));

            if (fitsSigned12(signedAddress)) {
                // mem32[r0 + r0 + literal] <= src
                emitInstruction(0x8, 0x0, 0, 0, src, static_cast<int32_t>(signedAddress));
            }
            else {
                const uint32_t instructionOffset = currentOffset("st memory-literal instruction offset");
                emitInstruction(0x8, 0x2, 15, 0, src, 0);
                requestLiteralPoolLiteral(instructionOffset, address);
            }
            break;
        }

        case OperandKind::MEMORY_SYMBOL: {
            const uint32_t instructionOffset = currentOffset("st memory-symbol instruction offset");
            emitInstruction(0x8, 0x2, 15, 0, src, 0);
            requestLiteralPoolSymbol(instructionOffset, dst.symbol);
            break;
        }

        default:
            throw runtime_error("Invalid destination operand for st");

    }
}



void Assembler::encodeIret(const ParsedLine& line) {
    if (!line.operands.empty()) throw runtime_error("iret expects no operands");

    //status is below the saved pc on the stack.  Restore it first without
    //moving sp, then restore pc and discard both saved words at once.
    emitInstruction(0x9, 0x6, 0, 14, 0, 4);
    emitInstruction(0x9, 0x3, 15, 14, 0, 8);
}












void Assembler::processInstruction(const ParsedLine& line) {
    if (currentSection == nullptr) throw runtime_error("Instruction outside of section");
    

    if (line.mnemonic == "halt") {
        if (!line.operands.empty()) throw runtime_error("halt expects no operands");
        emitInstruction(0x0, 0x0, 0, 0, 0, 0);
    }

    else if (line.mnemonic == "add") {
        encodeArithmetic(line, 0x0);
    }
    else if (line.mnemonic == "sub") {
        encodeArithmetic(line, 0x1);
    }
    else if (line.mnemonic == "mul") {
        encodeArithmetic(line, 0x2);
    }
    else if (line.mnemonic == "div") {
        encodeArithmetic(line, 0x3);
    }
    else if (line.mnemonic == "not") {
        encodeNot(line);
    }
    else if (line.mnemonic == "and") {
        encodeLogicBinary(line, 0x1);
    }
    else if (line.mnemonic == "or") {
        encodeLogicBinary(line, 0x2);
    }
    else if (line.mnemonic == "xor") {
        encodeLogicBinary(line, 0x3);
    }
    else if (line.mnemonic == "shl") {
        encodeShift(line, 0x0);
    }
    else if (line.mnemonic == "shr") {
        encodeShift(line, 0x1);
    }
    else if (line.mnemonic == "xchg") {
        encodeXchg(line);
    }
    else if (line.mnemonic == "push") {
        encodePush(line);
    }
    else if (line.mnemonic == "pop") {
        encodePop(line);
    }
    else if (line.mnemonic == "ret") {
        encodeRet(line);
    }
    else if (line.mnemonic == "int") {
        encodeInt(line);
    }
    else if (line.mnemonic == "csrrd") {
        encodeCsrrd(line);
    }
    else if (line.mnemonic == "csrwr") {
        encodeCsrwr(line);
    }

    else if (line.mnemonic == "jmp") { //mora proveriti tipove operanada
        encodeJump(line);
    }


    else if (line.mnemonic == "call") {
        encodeCall(line);
    }

    else if (line.mnemonic == "beq") {
        encodeBranch(line, 0x1);
    }
    else if (line.mnemonic == "bne") {
        encodeBranch(line, 0x2);
    }
    else if (line.mnemonic == "bgt") {
        encodeBranch(line, 0x3);
    }

    else if (line.mnemonic == "ld") {
        encodeLd(line);
    }

    else if (line.mnemonic == "st") {
        encodeSt(line);
    }
    else if (line.mnemonic == "iret") {
        encodeIret(line);
    }
    else {
        throw runtime_error("Unknown instruction: " + line.mnemonic);
    }








   
}


void Assembler::processLine(ParsedLine parsed) {
    if (!parsed.label.empty()) {
        Assembler::defineLabel(parsed.label);
    }

    if (parsed.mnemonic.empty()) {
        return;
    }

    if (parsed.mnemonic.front() == '.') {
        processDirective(parsed);
    }
    else {
        processInstruction(parsed);
    }

}


void Assembler::defineLabel(const string& name) {
    if (currentSection == nullptr) throw runtime_error("Label outside of section " + name);

    requireValidSymbolName(name, "label");

    if (equDefinitions.find(name) != equDefinitions.end()) {
        throw runtime_error("Symbol is already defined using .equ: " + name);
    }
    


    uint32_t offset = currentOffset("Label offset");

    auto it = symbols.find(name);

    if (it != symbols.end() && it->second.defined) throw runtime_error("Symbol already defined: " + name);
    if (it != symbols.end() && it->second.bind == SymbolBind::EXTERN) throw runtime_error("Extern symbol also defined: " + name);
    

    Symbol& sym = symbols[name];
    sym.name = name;
    sym.section = currentSection->name;
    sym.offset = offset;
    sym.defined = true;
}


void Assembler::markGlobal(const string& name) {


    if (name.empty()) {
        throw runtime_error("Global symbol name cannot be empty");
    }

    requireValidSymbolName(name, ".global");

    Symbol& sym = symbols[name];


    if (sym.name.empty()) {
        sym.name = name;
    }

    if (sym.bind == SymbolBind::EXTERN) throw runtime_error("Symbol cannot be both extern and global: " + name);

    sym.bind = SymbolBind::GLOBAL;

    if(!sym.defined) {
        sym.section = "UND";
    }
}



void Assembler::markExtern(const string& name) {
    requireValidSymbolName(name, ".extern");

    if (equDefinitions.find(name) != equDefinitions.end()) {
        throw runtime_error("Symbol defined using .equ cannot be extern: " + name);
    }

    Symbol& sym = symbols[name];


    if (sym.defined) throw runtime_error("Defined symbol cannot be extern: " + name);
    if (sym.bind == SymbolBind::GLOBAL) throw runtime_error("Symbol cannot be both global and extern: " + name);


    sym.name = name;
    sym.section = "UND";
    sym.offset = 0;
    sym.defined = false;
    sym.bind = SymbolBind::EXTERN;

}






void Assembler::processWordDirective(const ParsedLine& line) {
    if (currentSection == nullptr) throw runtime_error(".word outside of section");
    if (line.operands.empty()) throw runtime_error(".word expects at least one initializer");

    for (const string& operand : line.operands) {
        int64_t literal = 0;

        if (tryParseNumber(operand, literal)) {
            emitWord(numberToWord(literal, ".word initializer"));
            continue;
        }

        requireValidSymbolName(operand, ".word");

        const uint32_t offset = currentOffset(".word relocation offset");
        emitWord(0);
        addRelocation(offset, operand, RelocationType::ABS32);
    }
}


void Assembler::processSkipDirective(const ParsedLine& line) {
    if (currentSection == nullptr) throw runtime_error(".skip outside of section");
    if (line.operands.size() != 1) throw runtime_error(".skip expects exactly one literal");

    const int64_t count = parseNumber(line.operands[0], ".skip");

    if (count < 0) throw runtime_error(".skip size cannot be negative");
    if (static_cast<uint64_t>(count) > static_cast<uint64_t>(numeric_limits<size_t>::max())) {
        throw runtime_error(".skip size is too large");
    }

    currentSection->bytes.insert(
        currentSection->bytes.end(),
        static_cast<size_t>(count),
        static_cast<uint8_t>(0)
    );
}


vector<uint8_t> Assembler::decodeAsciiString(const string& operand, int lineNumber) const {
    if (operand.size() < 2 || operand.front() != '"' || operand.back() != '"') {
        throw runtime_error(".ascii expects one quoted string");
    }

    vector<uint8_t> bytes;
    const string content = operand.substr(1, operand.size() - 2);

    for (size_t index = 0; index < content.size(); index++) {
        unsigned char character = static_cast<unsigned char>(content[index]);

        if (character != '\\') {
            bytes.push_back(static_cast<uint8_t>(character));
            continue;
        }

        if (index + 1 >= content.size()) {
            throw runtime_error("Line " + to_string(lineNumber) + ": incomplete escape sequence in .ascii");
        }

        const char escaped = content[++index];

        switch (escaped) {
            case 'n': bytes.push_back(static_cast<uint8_t>('\n')); break;
            case 'r': bytes.push_back(static_cast<uint8_t>('\r')); break;
            case 't': bytes.push_back(static_cast<uint8_t>('\t')); break;
            case '0': bytes.push_back(static_cast<uint8_t>(0)); break;
            case '\\': bytes.push_back(static_cast<uint8_t>('\\')); break;
            case '"': bytes.push_back(static_cast<uint8_t>('"')); break;

            case 'x': {
                if (index + 2 >= content.size()) {
                    throw runtime_error("Line " + to_string(lineNumber) + ": \\x escape requires two hexadecimal digits");
                }

                const string hexByte = content.substr(index + 1, 2);
                if (!isxdigit(static_cast<unsigned char>(hexByte[0])) || !isxdigit(static_cast<unsigned char>(hexByte[1]))) {
                    throw runtime_error("Line " + to_string(lineNumber) + ": invalid hexadecimal escape in .ascii");
                }

                const unsigned long value = stoul(hexByte, nullptr, 16);
                bytes.push_back(static_cast<uint8_t>(value));
                index += 2;
                break;
            }

            default:
                throw runtime_error("Line " + to_string(lineNumber) + ": unsupported escape sequence in .ascii");
        }
    }

    return bytes;
}


void Assembler::processAsciiDirective(const ParsedLine& line) {
    if (currentSection == nullptr) throw runtime_error(".ascii outside of section");
    if (line.operands.size() != 1) throw runtime_error(".ascii expects exactly one string");

    const vector<uint8_t> bytes = decodeAsciiString(line.operands[0], line.lineNumber);
    currentSection->bytes.insert(currentSection->bytes.end(), bytes.begin(), bytes.end());
}


void Assembler::processEquDirective(const ParsedLine& line) {
    if (line.operands.size() != 2) throw runtime_error(".equ expects a symbol and an expression");

    const string name = line.operands[0];
    const string expression = line.operands[1];

    requireValidSymbolName(name, ".equ");

    if (expression.empty()) throw runtime_error(".equ expression cannot be empty");
    if (equDefinitions.find(name) != equDefinitions.end()) throw runtime_error(".equ symbol already defined: " + name);

    auto existing = symbols.find(name);
    if (existing != symbols.end()) {
        if (existing->second.defined) throw runtime_error("Symbol already defined: " + name);
        if (existing->second.bind == SymbolBind::EXTERN) throw runtime_error("Extern symbol cannot be defined using .equ: " + name);
    }

    Symbol& symbol = symbols[name];
    symbol.name = name;
    symbol.section = "UND";
    symbol.offset = 0;
    symbol.defined = false;

    equDefinitions.emplace(name, EquDefinition{expression, line.lineNumber});
    equOrder.push_back(name);
}


static void addLinearValue(LinearExpressionValue& destination, const LinearExpressionValue& source, int64_t multiplier) {
    destination.constant += source.constant * multiplier;

    for (const auto& [section, coefficient] : source.sectionCoefficients) {
        destination.sectionCoefficients[section] += coefficient * multiplier;

        if (destination.sectionCoefficients[section] == 0) {
            destination.sectionCoefficients.erase(section);
        }
    }
}


LinearExpressionValue Assembler::evaluateExpression(const string& expression, int lineNumber) {
    size_t position = 0;

    const auto skipWhitespace = [&]() {
        while (position < expression.size() && isspace(static_cast<unsigned char>(expression[position]))) position++;
    };

    function<LinearExpressionValue()> parseExpression;
    function<LinearExpressionValue()> parseUnary;
    function<LinearExpressionValue()> parsePrimary;

    parsePrimary = [&]() -> LinearExpressionValue {
        skipWhitespace();

        if (position >= expression.size()) {
            throw runtime_error("Line " + to_string(lineNumber) + ": expected expression term");
        }

        if (expression[position] == '(') {
            position++;
            LinearExpressionValue value = parseExpression();
            skipWhitespace();

            if (position >= expression.size() || expression[position] != ')') {
                throw runtime_error("Line " + to_string(lineNumber) + ": missing closing parenthesis in .equ");
            }

            position++;
            return value;
        }

        const size_t start = position;
        while (position < expression.size()) {
            const char character = expression[position];
            if (isspace(static_cast<unsigned char>(character)) || character == '+' || character == '-' || character == '(' || character == ')') break;
            position++;
        }

        if (start == position) {
            throw runtime_error("Line " + to_string(lineNumber) + ": invalid term in .equ expression");
        }

        const string token = expression.substr(start, position - start);
        int64_t number = 0;

        if (tryParseNumber(token, number)) {
            LinearExpressionValue value;
            value.constant = number;
            return value;
        }

        requireValidSymbolName(token, ".equ expression");
        return evaluateEquSymbol(token);
    };

    parseUnary = [&]() -> LinearExpressionValue {
        skipWhitespace();

        if (position < expression.size() && expression[position] == '+') {
            position++;
            return parseUnary();
        }

        if (position < expression.size() && expression[position] == '-') {
            position++;
            LinearExpressionValue value = parseUnary();
            value.constant = -value.constant;
            for (auto& [section, coefficient] : value.sectionCoefficients) coefficient = -coefficient;
            return value;
        }

        return parsePrimary();
    };

    parseExpression = [&]() -> LinearExpressionValue {
        LinearExpressionValue value = parseUnary();

        while (true) {
            skipWhitespace();

            if (position >= expression.size() || (expression[position] != '+' && expression[position] != '-')) break;

            const char operation = expression[position++];
            const LinearExpressionValue right = parseUnary();
            addLinearValue(value, right, operation == '+' ? 1 : -1);
        }

        return value;
    };

    LinearExpressionValue result = parseExpression();
    skipWhitespace();

    if (position != expression.size()) {
        throw runtime_error("Line " + to_string(lineNumber) + ": unsupported content in .equ expression near: " + expression.substr(position));
    }

    return result;
}


LinearExpressionValue Assembler::evaluateEquSymbol(const string& name) {
    auto equIt = equDefinitions.find(name);

    if (equIt == equDefinitions.end()) {
        auto symbolIt = symbols.find(name);

        if (symbolIt == symbols.end() || !symbolIt->second.defined) {
            throw runtime_error("Undefined symbol in .equ expression: " + name);
        }

        const Symbol& symbol = symbolIt->second;
        LinearExpressionValue value;
        value.constant = static_cast<int64_t>(symbol.offset);

        if (symbol.section != "ABS") {
            value.sectionCoefficients[symbol.section] = 1;
        }

        return value;
    }

    const int state = equVisitState[name];
    if (state == 1) throw runtime_error("Circular .equ definition involving symbol: " + name);

    if (state == 2) {
        const Symbol& symbol = symbols.at(name);
        LinearExpressionValue value;
        value.constant = static_cast<int64_t>(symbol.offset);
        if (symbol.section != "ABS") value.sectionCoefficients[symbol.section] = 1;
        return value;
    }

    equVisitState[name] = 1;

    LinearExpressionValue value = evaluateExpression(equIt->second.expression, equIt->second.lineNumber);

    for (auto it = value.sectionCoefficients.begin(); it != value.sectionCoefficients.end();) {
        if (it->second == 0) it = value.sectionCoefficients.erase(it);
        else ++it;
    }

    Symbol& symbol = symbols[name];

    if (symbol.bind == SymbolBind::EXTERN) {
        throw runtime_error("Extern symbol cannot be defined using .equ: " + name);
    }

    if (value.sectionCoefficients.empty()) {
        symbol.section = "ABS";
        symbol.offset = numberToWord(value.constant, ".equ " + name);
    }
    else if (value.sectionCoefficients.size() == 1 && value.sectionCoefficients.begin()->second == 1) {
        const string& sectionName = value.sectionCoefficients.begin()->first;

        if (sections.find(sectionName) == sections.end()) {
            throw runtime_error(".equ references unknown section through symbol: " + name);
        }

        if (value.constant < 0 || value.constant > static_cast<int64_t>(numeric_limits<uint32_t>::max())) {
            throw runtime_error("Relocatable .equ value is outside of its section range: " + name);
        }

        symbol.section = sectionName;
        symbol.offset = static_cast<uint32_t>(value.constant);
    }
    else {
        throw runtime_error(".equ expression is not absolute or section-relocatable: " + name);
    }

    symbol.name = name;
    symbol.defined = true;
    equVisitState[name] = 2;

    return value;
}


void Assembler::resolveEquDefinitions() {
    for (const string& name : equOrder) {
        evaluateEquSymbol(name);
    }
}


void Assembler::resolveDeferredDisp12() {
    for (const DeferredDisp12& fixup : deferredDisp12) {
        auto symbolIt = symbols.find(fixup.symbol);

        if (symbolIt == symbols.end() || !symbolIt->second.defined || symbolIt->second.section != "ABS") {
            throw runtime_error(fixup.context + " requires a known absolute symbol: " + fixup.symbol);
        }

        const uint32_t word = symbolIt->second.offset + static_cast<uint32_t>(fixup.addend);
        const int64_t signedValue = word <= 0x7FFFFFFFu
            ? static_cast<int64_t>(word)
            : static_cast<int64_t>(static_cast<int32_t>(word));

        Section& section = sections.at(fixup.section);
        patchDisp12(section.bytes, fixup.instructionOffset, signedValue, fixup.context);
    }

    deferredDisp12.clear();
}


void Assembler::finalizeLiteralPools() {
    for (const string& sectionName : sectionOrder) {
        auto poolIt = literalPools.find(sectionName);
        if (poolIt == literalPools.end() || poolIt->second.empty()) continue;

        Section& section = sections.at(sectionName);

        while (section.bytes.size() % 4 != 0) {
            section.bytes.push_back(0);
        }

        for (const LiteralPoolRequest& request : poolIt->second) {
            const uint32_t poolOffset = checkedUint32Size(section.bytes.size(), "Literal-pool offset");
            const int64_t nextInstruction = static_cast<int64_t>(request.instructionOffset) + 4;
            const int64_t displacement = static_cast<int64_t>(poolOffset) - nextInstruction;

            patchDisp12(section.bytes, request.instructionOffset, displacement, "Literal-pool displacement");

            if (!request.containsSymbol) {
                appendWord(section, request.literal);
                continue;
            }

            auto symbolIt = symbols.find(request.symbol);

            if (symbolIt != symbols.end() && symbolIt->second.defined && symbolIt->second.section == "ABS") {
                appendWord(section, symbolIt->second.offset + static_cast<uint32_t>(request.addend));
            }
            else {
                appendWord(section, 0);
                addRelocationForSection(sectionName, poolOffset, request.symbol, RelocationType::ABS32, request.addend);
            }
        }
    }

    literalPools.clear();
}


void Assembler::resolveAssemblerKnownRelocations() {
    vector<Relocation> unresolved;
    unresolved.reserve(relocations.size());

    for (const Relocation& relocation : relocations) {
        auto symbolIt = symbols.find(relocation.symbol);

        if (symbolIt == symbols.end() || !symbolIt->second.defined || symbolIt->second.section != "ABS") {
            unresolved.push_back(relocation);
            continue;
        }

        Section& section = sections.at(relocation.section);
        const uint32_t value = symbolIt->second.offset + static_cast<uint32_t>(relocation.addend);

        if (relocation.type == RelocationType::ABS32) {
            const size_t offset = static_cast<size_t>(relocation.offset);
            if (offset > section.bytes.size() || section.bytes.size() - offset < 4) {
                throw runtime_error("ABS32 relocation outside of section");
            }

            section.bytes[offset + 0] = static_cast<uint8_t>(value & 0xFFu);
            section.bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
            section.bytes[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
            section.bytes[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
        }
        else {
            const int64_t signedValue = value <= 0x7FFFFFFFu
                ? static_cast<int64_t>(value)
                : static_cast<int64_t>(static_cast<int32_t>(value));
            patchDisp12(section.bytes, relocation.offset, signedValue, "DISP12 relocation");
        }
    }

    relocations = std::move(unresolved);
}


void Assembler::finalizeAssembly() {
    resolveEquDefinitions();
    resolveDeferredDisp12();
    finalizeLiteralPools();
    resolveAssemblerKnownRelocations();
}


//helper za writeobjectfile





//helper za writeobjectfile
string Assembler::symbolBindToString(SymbolBind bind) {
    switch(bind) {
        case SymbolBind::LOCAL: return "LOCAL";
        case SymbolBind::GLOBAL: return "GLOBAL";
        case SymbolBind::EXTERN: return "EXTERN";

    }

    throw runtime_error("Invalid symbol binding value");
}



static string relocationTypeToString(RelocationType type) {
    switch (type) {
        case RelocationType::ABS32:
            return "ABS32";
        case RelocationType::DISP12:
            return "DISP12";
    }

    throw runtime_error("Unknown relocation type: ");
}


void Assembler::writeObjectFile(const string& outputPath) {


    validateObjectState();


    ofstream out(outputPath);

    if (!out) throw runtime_error("Cannot open output file: " + outputPath);
    

    out << "MOJOBJ " << 1 << "\n"; //1 is the format version
    out << "SECTIONS " << sectionOrder.size() << "\n";

    for (const string& sectionName : sectionOrder) {
        const Section& sec = sections.at(sectionName);

        out << "SECTION " << sec.name << " " << sec.bytes.size() << "\n";

        for (size_t index = 0; index < sec.bytes.size(); index++) {
            if (index != 0) {out << ' ';}
            out << hex
                << uppercase
                << setw(2)
                << setfill('0')
                << static_cast<unsigned>(sec.bytes[index]);
        }

        out << dec << "\n";
    }

    vector <const Symbol*> orderedSymbols;
    
    orderedSymbols.reserve(symbols.size());

    for (const auto& [name, symbol] : symbols) {
        (void) name;
        orderedSymbols.push_back(&symbol);
    }

    sort(orderedSymbols.begin(), orderedSymbols.end(),[](const Symbol* first, const Symbol* second) {return first->name < second->name;});


    out << "SYMBOLS " << orderedSymbols.size() << "\n";

    for (const Symbol* sym : orderedSymbols) {


        out << "SYMBOL "
            << sym->name << " "
            << sym->section << " "
            << sym->offset << " "
            << symbolBindToString(sym->bind) << " "
            << (sym->defined ? "DEF" : "UND")
            << "\n";
    }


    unordered_map<string, size_t> sectionRank;

    for (size_t index = 0; index < sectionOrder.size(); index++) {
        sectionRank.emplace(sectionOrder[index], index);
    }

    vector<const Relocation*> orderedRelocations;
    orderedRelocations.reserve(relocations.size());

    for(const Relocation& relocation : relocations) {
        orderedRelocations.push_back(&relocation);
    }

    sort(orderedRelocations.begin(), orderedRelocations.end(), [&sectionRank](const Relocation* first, const Relocation* second) {
        size_t firstSection = sectionRank.at(first->section);
        size_t secondSection = sectionRank.at(second->section);

        if (firstSection != secondSection) {
            return firstSection < secondSection;
        }
        if (first->offset != second->offset) {
            return first->offset < second->offset;
        }

        if (first->symbol != second->symbol) {
            return first->symbol < second->symbol;
        }
        
        if (first->type != second->type) {
            return static_cast<int>(first->type) < static_cast<int>(second->type);
        }

        return first->addend < second->addend;
    });




    out << "RELOCATIONS " << orderedRelocations.size() << "\n";



    for (const Relocation* rel : orderedRelocations) {


        out << "RELOCATION" << " "
            << rel->section << " "
            << rel->offset << " "
            << rel->symbol << " "
            << relocationTypeToString(rel->type) << " "
            << rel->addend
            << "\n";
    }

    out << "END\n";

    if (!out) {
        throw runtime_error("Failed while writing object file: " + outputPath);
    }
}



