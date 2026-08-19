#include "../inc/Emulator.hpp"

#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <unistd.h>

using namespace std;


namespace {

constexpr uint32_t TERM_OUT = 0xFFFFFF00u;
constexpr uint32_t TERM_IN = 0xFFFFFF04u;
constexpr uint32_t TIM_CFG = 0xFFFFFF10u;

constexpr uint32_t STATUS_TIMER_MASK = 0x1u;
constexpr uint32_t STATUS_TERMINAL_MASK = 0x2u;
constexpr uint32_t STATUS_GLOBAL_MASK = 0x4u;

class InvalidInstruction final : public runtime_error {
public:
    explicit InvalidInstruction(const string& message)
        : runtime_error(message) {}
};

[[noreturn]] void invalidInstruction(const string& message) {
    throw InvalidInstruction(message);
}

uint32_t parseHexWord(const string& text, const string& context) {
    size_t processed = 0;
    unsigned long long value = 0;

    try {
        value = stoull(text, &processed, 16);
    }
    catch (const exception&) {
        throw runtime_error("Invalid hexadecimal " + context + ": " + text);
    }

    if (processed != text.size() || value > numeric_limits<uint32_t>::max()) {
        throw runtime_error("Invalid hexadecimal " + context + ": " + text);
    }

    return static_cast<uint32_t>(value);
}

uint8_t parseHexByte(const string& text) {
    if (text.size() != 2) {
        throw runtime_error("Invalid byte in hex file: " + text);
    }

    const uint32_t value = parseHexWord(text, "byte");

    if (value > 0xFFu) {
        throw runtime_error("Invalid byte in hex file: " + text);
    }

    return static_cast<uint8_t>(value);
}

}


static uint32_t addDisplacement(uint32_t base, int32_t displacement) {
    // The emulated machine performs 32-bit address/register arithmetic.
    // Converting the signed displacement to uint32_t makes negative values
    // wrap modulo 2^32, matching the processor's 32-bit behavior.
    return base + static_cast<uint32_t>(displacement);
}

static uint32_t addDisplacement(uint32_t first, uint32_t second, int32_t displacement) {
    return first + second + static_cast<uint32_t>(displacement);
}

Emulator::~Emulator() {
    restoreTerminal();
}

uint8_t Emulator::read8(uint32_t address) const {
    auto it = memory.find(address);
    if (it == memory.end())
        return 0;
    return it->second;
}

void Emulator::write8(uint32_t address, uint8_t value) {
    memory[address] = value;
}

uint32_t Emulator::read32(uint32_t address) const {

    uint32_t b0 = read8(address);
    uint32_t b1 = read8(address + 1);
    uint32_t b2 = read8(address + 2);
    uint32_t b3 = read8(address + 3);

    return b0
        | (b1 << 8)
        | (b2 << 16)
        | (b3 << 24);

}

void Emulator::write32Raw(uint32_t address, uint32_t value) {
    write8(address,     static_cast<uint8_t>(value & 0xFFu));
    write8(address + 1, static_cast<uint8_t>((value >> 8) & 0xFFu));
    write8(address + 2, static_cast<uint8_t>((value >> 16) & 0xFFu));
    write8(address + 3, static_cast<uint8_t>((value >> 24) & 0xFFu));
}

void Emulator::write32(uint32_t address, uint32_t value) {
    write32Raw(address, value);

    if (address == TERM_OUT) {
        cout.put(static_cast<char>(value & 0xFFu));
        cout.flush();
    }
    else if (address == TIM_CFG) {
        configureTimer(value);
    }
}

uint32_t Emulator::fetchInstruction(uint32_t address) const {
    //Instruction bytes are written in architectural order OC/MOD, registers,
    //and displacement.  Data words remain little-endian and still use read32.
    const uint32_t b0 = read8(address);
    const uint32_t b1 = read8(address + 1);
    const uint32_t b2 = read8(address + 2);
    const uint32_t b3 = read8(address + 3);

    return (b0 << 24)
         | (b1 << 16)
         | (b2 << 8)
         | b3;
}


void Emulator::loadHexFile(const string& path) {
    ifstream in(path);

    if (!in)
        throw runtime_error("Cannot open hex file: " + path);

    memory.clear();

    string line;
    size_t lineNumber = 0;

    while (getline(in, line)) {
        lineNumber++;

        if (line.empty())
            continue;

        stringstream ss(line);

        string addressToken;
        ss >> addressToken;

        if (addressToken.empty())
            continue;

        if (addressToken.back() != ':')
            throw runtime_error("Invalid hex line " + to_string(lineNumber) + ", missing ':'");

        addressToken.pop_back();

        uint32_t address = parseHexWord(addressToken, "address on line " + to_string(lineNumber));

        string byteToken;

        while (ss >> byteToken) {
            const uint8_t value = parseHexByte(byteToken);

            write8(address, value);
            address++;
        }
    }
}

void Emulator::printState() const {
    cout << "-----------------------------------------------------------------\n";
    cout << "Emulated processor executed halt instruction\n";
    cout << "Emulated processor state:\n";

    for (int i = 0; i < 16; i++) {
        cout << "r" << dec << i << "=0x"
             << hex << uppercase << setw(8) << setfill('0')
             << gpr[i];

        if (i % 4 == 3)
            cout << "\n";
        else
            cout << " ";
    }
}


void Emulator::run() {
    halted = false;
    terminalInterruptPending = false;
    timerInterruptPending = false;
    suppressInterruptCheckOnce = false;

    for (uint32_t& reg : gpr) reg = 0;
    for (uint32_t& reg : csr) reg = 0;

    gpr[15] = 0x40000000; // pc
    gpr[0] = 0;

    write32Raw(TERM_IN, 0);
    write32Raw(TERM_OUT, 0);
    write32Raw(TIM_CFG, 0);
    configureTimer(0);
    configureTerminal();

    try {
        while (!halted) {
            pollTerminal();
            updateTimer();

            const uint32_t pc = gpr[15];
            const uint32_t instr = fetchInstruction(pc);

            gpr[15] += 4;

            try {
                execute(instr);
            }
            catch (const InvalidInstruction&) {
                executeInterrupt(1);
            }

            gpr[0] = 0;

            pollTerminal();
            updateTimer();

            if (!halted) {
                if (suppressInterruptCheckOnce) {
                    suppressInterruptCheckOnce = false;
                }
                else {
                    servicePendingInterrupts();
                }
            }

            gpr[0] = 0;
        }
    }
    catch (...) {
        restoreTerminal();
        throw;
    }

    restoreTerminal();
    printState();
}

int32_t Emulator::signExtend12(uint32_t value) {
    value &= 0xFFF;

    if (value & 0x800)
        return static_cast<int32_t>(value | 0xFFFFF000);

    return static_cast<int32_t>(value);
}


void Emulator::executeArithmetic(uint8_t mod, uint8_t a, uint8_t b, uint8_t c) {
    if (mod == 0x0) gpr[a] = gpr[b] + gpr[c];
    else if (mod == 0x1) gpr[a] = gpr[b] - gpr[c];
    else if (mod == 0x2) gpr[a] = gpr[b] * gpr[c];
    else if (mod == 0x3) {
        if (gpr[c] == 0) invalidInstruction("Division by zero");
        gpr[a] = gpr[b] / gpr[c];
    }
    else invalidInstruction("Invalid arithmetic mod");
}

void Emulator::executeLogic(uint8_t mod, uint8_t a, uint8_t b, uint8_t c) {
    if (mod == 0x0) {
        if (c != 0) invalidInstruction("Invalid not instruction encoding");
        gpr[a] = ~gpr[b];
    }
    else if (mod == 0x1) gpr[a] = gpr[b] & gpr[c];
    else if (mod == 0x2) gpr[a] = gpr[b] | gpr[c];
    else if (mod == 0x3) gpr[a] = gpr[b] ^ gpr[c];
    else invalidInstruction("Invalid logic mod");
}

void Emulator::executeShift(uint8_t mod, uint8_t a, uint8_t b, uint8_t c) {
    const uint32_t shiftAmount = gpr[c] & 0x1Fu;

    if (mod == 0x0) gpr[a] = gpr[b] << shiftAmount;
    else if (mod == 0x1) gpr[a] = gpr[b] >> shiftAmount;
    else invalidInstruction("Invalid shift mod");
}


void Emulator::executeLoad(uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int32_t disp) {
    switch(mode) {
        case 0x1:
        {
            if (regC != 0) invalidInstruction("Invalid immediate load encoding");
            gpr[regA] = addDisplacement(gpr[regB], disp);
            break;
        }
           
        case 0x2:
        {
            uint32_t addr = addDisplacement(gpr[regB], gpr[regC], disp);
            gpr[regA] = read32(addr);
            break;
        }
           
        case 0x3:
        {
            if (regC != 0) invalidInstruction("Invalid postincrement load encoding");
            gpr[regA] = read32(gpr[regB]);
            gpr[regB] = addDisplacement(gpr[regB], disp);
            break;
        }
           
        case 0x0:
        {
            if (regB >= 3 || regC != 0 || disp != 0) invalidInstruction("Invalid csrrd encoding");
            gpr[regA] = csr[regB];
            break;
        }
            
        case 0x4:
        {
            if (regA >= 3 || regC != 0 || disp != 0) invalidInstruction("Invalid csrwr encoding");
            csr[regA] = gpr[regB];
            break;
        }

        case 0x5:
        {
            if (regA >= 3 || regB >= 3 || regC != 0) invalidInstruction("Invalid CSR-or encoding");
            csr[regA] = csr[regB] | static_cast<uint32_t>(disp);
            break;
        }

        case 0x6:
        {
            if (regA >= 3) invalidInstruction("Invalid CSR register in CSR memory load");
            const uint32_t address = addDisplacement(gpr[regB], gpr[regC], disp);
            csr[regA] = read32(address);

            //This is the first half of the assembler's iret expansion.
            if (regA == 0 && regB == 14 && regC == 0 && disp == 4) {
                suppressInterruptCheckOnce = true;
            }
            break;
        }
            
        case 0x7:
        {
            if (regA >= 3 || regC != 0) invalidInstruction("Invalid CSR pop encoding");
            csr[regA] = read32(gpr[regB]);
            gpr[regB] = addDisplacement(gpr[regB], disp);
            break;
        }
            
        default:
            invalidInstruction("Unsupported ld mode");

        
    }
}

void Emulator::executeStore(uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int32_t disp) {
    switch (mode) {
        case 0x0:
        {
            uint32_t address = addDisplacement(gpr[regA], gpr[regB], disp);
            write32(address, gpr[regC]);
            break;
        }

        case 0x1:
        {
            if (regB != 0) invalidInstruction("Invalid predecrement store encoding");
            gpr[regA] = addDisplacement(gpr[regA], disp);
            write32(gpr[regA], gpr[regC]);
            break;
        }
            
        case 0x2:
        {
            uint32_t addressPointer = addDisplacement(gpr[regA], gpr[regB], disp);
            uint32_t address = read32(addressPointer);
            write32(address, gpr[regC]);
            break;
        }   
            
        default:
            invalidInstruction("Invalid store mode");
    }
}


void Emulator::executeCall(uint8_t mode, uint8_t regA, uint8_t regB, int32_t disp) {
    if (mode != 0x0 && mode != 0x1) {
        invalidInstruction("Invalid call mode");
    }

    gpr[14] -= 4;
    write32(gpr[14], gpr[15]);

    if (mode == 0x0) {
        gpr[15] = addDisplacement(gpr[regA], gpr[regB], disp);
    }
    else {
        uint32_t address = addDisplacement(gpr[regA], gpr[regB], disp);
        gpr[15] = read32(address);
    }
}

void Emulator::executeJump(uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int32_t disp) {
    bool shouldJump = false;
    bool indirect = false;

    switch(mode) {
        case 0x0:
            shouldJump = true;
            indirect = false;
            break;
        case 0x1:
            shouldJump = (gpr[regB] == gpr[regC]);
            indirect = false;
            break;
        case 0x2:
            shouldJump = (gpr[regB] != gpr[regC]);
            indirect = false;
            break;
        case 0x3:
            shouldJump = static_cast<int32_t>(gpr[regB]) > static_cast<int32_t>(gpr[regC]);
            indirect = false;
            break;
        case 0x8:
            shouldJump = true;
            indirect = true;
            break;
        case 0x9:
            shouldJump = (gpr[regB] == gpr[regC]);
            indirect = true;
            break;
        case 0xA:
            shouldJump = (gpr[regB] != gpr[regC]);
            indirect = true;
            break;
        case 0xB:
            shouldJump = static_cast<int32_t>(gpr[regB]) > static_cast<int32_t>(gpr[regC]);
            indirect = true;
            break;
        default:
            invalidInstruction("Invalid jump mode");
    }
    if (!shouldJump) return;

    uint32_t address = addDisplacement(gpr[regA], disp);
    if (indirect) gpr[15] = read32(address);
    else gpr[15] = address;
}

void Emulator::executeInterrupt(uint32_t causeValue) {
    //push status
    gpr[14] -= 4;
    write32(gpr[14], csr[0]);

    //push pc
    gpr[14] -= 4;
    write32(gpr[14], gpr[15]);
    
    //cause <= causeValue
    csr[2] = causeValue;

    //globalno maskiranje prekida
    csr[0] = csr[0] | STATUS_GLOBAL_MASK;

    // pc <= handler
    gpr[15] = csr[1];
}

void Emulator::servicePendingInterrupts() {
    if ((csr[0] & STATUS_GLOBAL_MASK) != 0) return;

    //Terminal has priority if both peripheral requests are pending.
    if (terminalInterruptPending && (csr[0] & STATUS_TERMINAL_MASK) == 0) {
        terminalInterruptPending = false;
        executeInterrupt(3);
    }
    else if (timerInterruptPending && (csr[0] & STATUS_TIMER_MASK) == 0) {
        timerInterruptPending = false;
        executeInterrupt(2);
    }
}

void Emulator::configureTerminal() {
    if (terminalConfigured) return;

    originalStdinFlags = fcntl(STDIN_FILENO, F_GETFL, 0);

    if (originalStdinFlags == -1) {
        throw runtime_error("Cannot read terminal file status flags");
    }

    if (fcntl(STDIN_FILENO, F_SETFL, originalStdinFlags | O_NONBLOCK) == -1) {
        throw runtime_error("Cannot make terminal input nonblocking");
    }

    stdinIsTerminal = isatty(STDIN_FILENO) != 0;

    if (stdinIsTerminal) {
        if (tcgetattr(STDIN_FILENO, &originalTerminalSettings) == -1) {
            fcntl(STDIN_FILENO, F_SETFL, originalStdinFlags);
            throw runtime_error("Cannot read terminal settings");
        }

        termios settings = originalTerminalSettings;
        settings.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        settings.c_cc[VMIN] = 0;
        settings.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &settings) == -1) {
            fcntl(STDIN_FILENO, F_SETFL, originalStdinFlags);
            throw runtime_error("Cannot configure terminal input");
        }
    }

    terminalConfigured = true;
}

void Emulator::restoreTerminal() {
    if (!terminalConfigured) return;

    if (stdinIsTerminal) {
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTerminalSettings);
    }

    if (originalStdinFlags != -1) {
        fcntl(STDIN_FILENO, F_SETFL, originalStdinFlags);
    }

    terminalConfigured = false;
}

void Emulator::pollTerminal() {
    unsigned char character = 0;

    while (true) {
        const ssize_t result = read(STDIN_FILENO, &character, 1);

        if (result == 1) {
            write32Raw(TERM_IN, static_cast<uint32_t>(character));
            terminalInterruptPending = true;
            continue;
        }

        if (result == 0) return;

        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        if (errno == EINTR) continue;

        throw runtime_error("Cannot read terminal input");
    }
}

chrono::milliseconds Emulator::timerPeriodForConfiguration(uint32_t configuration) const {
    switch (configuration) {
        case 0x0: return chrono::milliseconds(500);
        case 0x1: return chrono::milliseconds(1000);
        case 0x2: return chrono::milliseconds(1500);
        case 0x3: return chrono::milliseconds(2000);
        case 0x4: return chrono::milliseconds(5000);
        case 0x5: return chrono::milliseconds(10000);
        case 0x6: return chrono::milliseconds(30000);
        case 0x7: return chrono::milliseconds(60000);
        default: return chrono::milliseconds(500);
    }
}

void Emulator::configureTimer(uint32_t configuration) {
    timerPeriod = timerPeriodForConfiguration(configuration);
    nextTimerInterrupt = chrono::steady_clock::now() + timerPeriod;
    timerInterruptPending = false;
}

void Emulator::updateTimer() {
    const auto now = chrono::steady_clock::now();

    if (now < nextTimerInterrupt) return;

    timerInterruptPending = true;

    do {
        nextTimerInterrupt += timerPeriod;
    } while (nextTimerInterrupt <= now);
}




void Emulator::execute(uint32_t instruction) {
    const uint8_t oc = static_cast<uint8_t>((instruction >> 28) & 0xFu);
    const uint8_t mod = static_cast<uint8_t>((instruction >> 24) & 0xFu);
    const uint8_t regA = static_cast<uint8_t>((instruction >> 20) & 0xFu);
    const uint8_t regB = static_cast<uint8_t>((instruction >> 16) & 0xFu);
    const uint8_t regC = static_cast<uint8_t>((instruction >> 12) & 0xFu);

    int32_t disp = signExtend12(instruction & 0xFFF);

    switch (oc) {
        case 0x0:
            if (instruction != 0) invalidInstruction("Invalid halt instruction encoding");
            halted = true;
            break;

        case 0x1:
            if (instruction != 0x10000000u) invalidInstruction("Invalid software interrupt encoding");
            executeInterrupt(4);
            break;

        case 0x2:
            if (regC != 0) invalidInstruction("Invalid call encoding");
            executeCall(mod, regA, regB, disp);
            break;
        
        case 0x3:
            executeJump(mod, regA, regB, regC, disp);
            break;

        case 0x4: 
        {
            if (mod != 0 || regA != 0 || disp != 0) invalidInstruction("Invalid xchg encoding");

            // xchg
            uint32_t tmp = gpr[regB];
            gpr[regB] = gpr[regC];
            gpr[regC] = tmp;
            break;
        }  
            

        case 0x5: // arithmetic
            if (disp != 0) invalidInstruction("Invalid arithmetic instruction encoding");
            executeArithmetic(mod, regA, regB, regC);
            break;

        case 0x6: // logic
            if (disp != 0) invalidInstruction("Invalid logic instruction encoding");
            executeLogic(mod, regA, regB, regC);
            break;

        case 0x7: // shift
            if (disp != 0) invalidInstruction("Invalid shift instruction encoding");
            executeShift(mod, regA, regB, regC);
            break;

        case 0x8:
            executeStore(mod, regA, regB, regC, disp);
            break;

        case 0x9:
            executeLoad(mod, regA, regB, regC, disp);
            break;

        default:
            invalidInstruction("Unsupported instruction");
    }
}
