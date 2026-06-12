#include "../hpp/Assembler.hpp"



#include <fstream>
#include <iostream>
#include <iomanip>


using namespace std;










void Assembler::assemble(const string& filename) {

    ifstream file(filename);
    if (!file) throw runtime_error("Cannot open file: " + filename);


    string line;

   
    while(getline(file, line)) {
        ParsedLine parsed = Parser::parseLine(line);
        processLine(parsed);
    }
}


//helper
void Assembler::switchSection(const string sectionName) {

    if (sections.find(sectionName) == sections.end()) {
        Section s;
        s.name = sectionName;
        sections[sectionName] = s;
    }

    currentSection = &sections[sectionName];
}




void Assembler::processDirective(const ParsedLine& line) {
    if (line.mnemonic == ".section") {
        switchSection(line.operands[0]);
    }
    else if (line.mnemonic == ".global") {
        for (const auto& name : line.operands) {
            markGlobal(name);
        }
    }
    else if (line.mnemonic == ".extern") {
        for (const auto& name : line.operands) {
            markExtern(name);
        }
    }
}

void Assembler::emitByte(uint8_t byte) {
    if (currentSection == nullptr) throw runtime_error("No active section");

    currentSection->bytes.push_back(byte);
}

void Assembler::emitWord(uint32_t value) {
    emitByte(value & 0xFF);
    emitByte((value >> 8) & 0xFF);
    emitByte((value >> 16) & 0xFF);
    emitByte((value >> 24) & 0xFF);
}




void Assembler::emitInstruction(uint8_t oc, uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int16_t disp) {
    uint32_t d = disp & 0xFFF;

    uint8_t b0 = (oc << 4) | mode;
    uint8_t b1 = (regA << 4) | regB;
    uint8_t b2 = (regC << 4) | ((d >> 8) & 0xF);
    uint8_t b3 = d & 0xFF;

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
    int reg = stoi(operand.substr(2));

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

    int src = parseGpr(line.operands[0]);
    int dst = parseGpr(line.operands[1]);

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

bool Assembler::isNumber(const std::string& s) {
    if (s.empty()) return false;
    size_t start = 0;

    if (s[0] == '+' || s[0] == '-') start = 1;

    if (start == s.size()) return false;

    for (size_t i = start; i < s.size(); i++) {
        if (!isdigit(s[i])) return false;
    }
    return true;
}



JumpTarget Assembler::parseJumpTarget(const string& op) {
    JumpTarget target;
    if (isNumber(op)) {
        target.kind = JumpTargetKind::LITERAL;
        target.literal = stoi(op);
    }
    else {
        target.kind = JumpTargetKind::SYMBOL;
        target.symbol = op;
    }

    return target;
}


void Assembler::addRelocation(uint32_t offset, const string& symbol) {
    Assembler::relocations.push_back({Assembler::currentSection->name, offset, symbol});
}

void Assembler::encodeJump(const ParsedLine& line) {
    if (line.operands.size() != 1) throw runtime_error("jmp expects one operand");


    JumpTarget target = parseJumpTarget(line.operands[0]);

    uint32_t instructionOffset = currentSection->bytes.size();

    if (target.kind == JumpTargetKind::LITERAL) {
        emitInstruction(0x3, 0x0, 0, 0, 0, target.literal);
    }
    else {
        emitInstruction(0x3, 0x0, 0, 0, 0, 0);
        addRelocation(instructionOffset, target.symbol);
    }

}

void Assembler::encodeCall(const ParsedLine& line) {
    if (line.operands.size() != 1) throw runtime_error("call expects 1 operand");

    JumpTarget target = parseJumpTarget(line.operands[0]);

    uint32_t instructionOffset = currentSection->bytes.size();

    if (target.kind == JumpTargetKind::LITERAL) emitInstruction(0x2, 0x0, 0, 0, 0, target.literal);
    else emitInstruction(0x2, 0x0, 0, 0, 0, 0);

    addRelocation(instructionOffset, target.symbol);
}

void Assembler::encodeBranch(const ParsedLine& line, uint8_t mode) {
    if (line.operands.size() != 3) throw runtime_error(line.mnemonic + " expects 3 operands");

    uint8_t reg1 = parseGpr(line.operands[0]);
    uint8_t reg2 = parseGpr(line.operands[1]);

    JumpTarget target = parseJumpTarget(line.operands[2]);
    uint32_t instructionOffset = currentSection->bytes.size();

    if (target.kind == JumpTargetKind::LITERAL) emitInstruction(0x3, mode, 0, reg1, reg2, target.literal);

    else emitInstruction(0x3, mode, 0, reg1, reg2, 0);

    addRelocation(instructionOffset, target.symbol);
    
}


Operand Assembler::parseOperand(const string& raw) {
    string op = raw;
    trim(op);


    Operand result;

    // $literal or $symbol

    string value;

    if (!op.empty() && op[0] == '$') {
        value = op.substr(1);
        trim(value);

    
    }

    if (isNumber(value)) {
        result.kind = OperandKind::IMMEDIATE_LITERAL;
        result.literal = stoi(value);
    }
    else {
        result.kind = OperandKind::IMMEDIATE_SYMBOL;
        result.symbol = value;
    }
    return result;

    // %r1, %sp, %oc
    if (!op.empty() && op[0] == '%') {
        result.kind = OperandKind::REGISTER;
        result.reg = parseGpr(op);
        return result;
    }

    // [%r1] or [%r1 + 4] ili [%r1 + symbol]

    string inside;
    if (op.size() >= 2 && op.front() == '[' && op.back() == ']') {
        inside = op.substr(1, op.size() - 2);
        trim(inside);

        size_t plusPos = inside.find('+');

        if (plusPos == string::npos) {
            result.kind = OperandKind::REGISTER_INDIRECT;
            trim(inside);
            result.reg = parseGpr(inside);
            return result;
        }


        string regPart = inside.substr(0, plusPos);
        trim(regPart);
        string dispPart = inside.substr(plusPos + 1);
        trim(dispPart);



        result.reg = parseGpr(regPart);

        if(isNumber(op)) {
            result.kind = OperandKind::MEMORY_LITERAL;
            result.literal = stoi(op);
        } else {
            result.kind = OperandKind::MEMORY_SYMBOL;
            result.symbol = op;
        }

        return result;

    }

    return result; //videcemo

}

void Assembler::encodeLd(const ParsedLine& line) {
    if (line.operands.size() != 2) throw runtime_error("ld expects 2 operands");

    Operand src = parseOperand(line.operands[0]);
    uint8_t dst = parseGpr(line.operands[1]);

    uint32_t instructionOffset = currentSection->bytes.size();

    switch (src.kind) {
        case OperandKind::IMMEDIATE_LITERAL:
            //gpr[A] <= gpr[B] + D
            //dst <= r0 + literal

            emitInstruction(0x9, 0x1, dst, 0, 0, src.literal);
            break;


        case OperandKind::IMMEDIATE_SYMBOL:
            emitInstruction(0x9, 0x1, dst, 0, 0, 0);
            addRelocation(instructionOffset, src.symbol);
            break;


        case OperandKind::REGISTER:
            // dst <= src.reg + 0
            emitInstruction(0x9, 0x2, dst, src.reg, 0, 0);
            break;


        case OperandKind::REGISTER_INDIRECT:
            // dst <= mem32[reg + r0 + 0]
            emitInstruction(0x9, 0x2, dst, src.reg, 0, 0);
            break;


        case OperandKind::REGISTER_INDIRECT_LITERAL:
            // dst <= mem32[reg + r0 + literal]
            emitInstruction(0x9, 0x2, dst, src.reg, 0, src.literal);
            break;



        case OperandKind::REGISTER_INDIRECT_SYMBOL:
            //sme samo ako je simbol poznat i staej u 12 bita
            //za nivo a ide greska
            throw runtime_error("ld [%reg + symbol] not supported yet");


        case OperandKind::MEMORY_LITERAL:
            //dst <= mem32[r0 + r0 + literal]
            emitInstruction(0x9, 0x2, dst, 0, 0, src.literal);
            break;
        
        case OperandKind::MEMORY_SYMBOL:
            emitInstruction(0x9, 0x2, dst, 0, 0, 0);
            addRelocation(instructionOffset, src.symbol);
            break;
    }


}


void Assembler::encodeSt(const ParsedLine& line) {
    if (line.operands.size() != 2) throw runtime_error("st expects 2 operands");

    uint8_t src = parseGpr(line.operands[0]);
    Operand dst = parseOperand(line.operands[1]);

    uint32_t instructionOffset = currentSection->bytes.size();

    switch (dst.kind) {



        case OperandKind::REGISTER_INDIRECT:

            // mem32[reg + r0 + 0] <= src
            emitInstruction(0x8, 0x0, dst.reg, 0, src, 0);
            break;

        case OperandKind::REGISTER_INDIRECT_LITERAL:
            // mem32[reg + r0 + literal] <= src
            emitInstruction(0x8, 0x0, dst.reg, 0, src, dst.literal);
            break;
        case OperandKind::REGISTER_INDIRECT_SYMBOL:
            throw runtime_error("st [%reg + symbol] not supported yet");
            
        case OperandKind::MEMORY_LITERAL:
            // mem32[r0 + r0 + literal] <= src
            emitInstruction(0x8, 0x0, 0, 0, src, dst.literal);
            break;
        case OperandKind::MEMORY_SYMBOL:
            emitInstruction(0x8, 0x0, 0, 0, src, 0);
            addRelocation(instructionOffset, dst.symbol);
            break;

        default:
            throw runtime_error("Invalid destination operand for st");

    }


}


void Assembler::encodeIret(const ParsedLine& line) {
    if (!line.operands.empty()) throw runtime_error("iret expects no operands");

    encodePopToRegister(15);

    emitInstruction(0x9, 0x7, 0, 14, 0, 4);
}












void Assembler::processInstruction(const ParsedLine& line) {
    if (currentSection == nullptr) throw runtime_error("Instruction outside of section");
    

    if (line.mnemonic == "halt") emitInstruction(0x0, 0x0, 0, 0, 0, 0);

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
    else if (line.mnemonic == "puhs") {
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








   
}


void Assembler::processLine(ParsedLine parsed) {
    if (!parsed.label.empty()) {
        Assembler::defineLabel(parsed.label);
    }

    if (parsed.mnemonic.empty()) {
        return;
    }

    if (parsed.isDirective) {
        processDirective(parsed);
    }
    else {
        processInstruction(parsed);
    }

}


void Assembler::defineLabel(const string& name) {
    if (currentSection == nullptr) throw runtime_error("Label outside of section " + name);
    


    uint32_t offset = currentSection->bytes.size();

    auto it = symbols.find(name);

    if (it != symbols.end() && it->second.defined) throw runtime_error("Symbol already defined: " + name);
    

    Symbol& sym = symbols[name];
    sym.name = name;
    sym.section = currentSection->name;
    sym.offset = offset;
    sym.defined = true;


    if (sym.bind == SymbolBind::EXTERN) throw runtime_error("Extern symbol also defined: " + name);
    

}

void Assembler::markGlobal(const string& name) {
    Symbol& sym = symbols[name];

    sym.name = name;

    if (sym.bind == SymbolBind::EXTERN) throw runtime_error("Symbol cannot be both extern and global: " + name);
    
}

void Assembler::markExtern(const string& name) {
    Symbol& sym = symbols[name];


    if (sym.defined) throw runtime_error("Defined symbol cannot be extern: " + name);


    sym.name = name;
    sym.section = "UND";
    sym.offset = 0;
    sym.defined = false;
    sym.bind = SymbolBind::EXTERN;

}





//helper za writeobjectfile
string Assembler::symbolBindToString(SymbolBind bind) {
    switch(bind) {
        case SymbolBind::LOCAL: return "LOCAL";
        case SymbolBind::GLOBAL: return "GLOBAL";
        case SymbolBind::EXTERN: return "EXTERN";

    }

    return "LOCAL";
}


void Assembler::writeObjectFile(const string& outputPath) {
    ofstream out(outputPath);

    if (!out) throw runtime_error("Cannot open output file: " + outputPath);
    

    out << "SECTIONS " << sections.size() << "\n";

    for (const auto& pair : sections) {
        const Section& sec = pair.second;

        out << sec.name << " " << sec.bytes.size() << "\n";

        for (uint8_t b : sec.bytes) {
            out << hex
                << uppercase
                << setw(2)
                << setfill('0')
                << static_cast<int>(b)
                << " ";
        }

        out << dec << "\n";
    }

    out << "SYMBOLS " << symbols.size() << "\n";

    for (const auto& pair : symbols) {
        const Symbol& sym = pair.second;

        out << sym.name << " "
            << sym.section << " "
            << sym.offset << " "
            << symbolBindToString(sym.bind) << " "
            << (sym.defined ? "DEF" : "UND")
            << "\n";
    }

    out << "RELOCATIONS " << relocations.size() << "\n";

    for (const Relocation& rel : relocations) {
        out << rel.section << " "
            << rel.offset << " "
            << rel.symbol
            << "\n";
    }

    out << "END\n";
}



