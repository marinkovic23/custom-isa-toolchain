#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include "Common.hpp"

#include <chrono>
#include <termios.h>

class Emulator {
private:
    std::unordered_map<uint32_t, uint8_t> memory;
    uint32_t gpr[16]{};


    uint32_t csr[3]{};

    bool halted = false;
    bool terminalInterruptPending = false;
    bool timerInterruptPending = false;

    bool terminalConfigured = false;
    bool stdinIsTerminal = false;
    int originalStdinFlags = -1;
    termios originalTerminalSettings{};

    std::chrono::milliseconds timerPeriod{500};
    std::chrono::steady_clock::time_point nextTimerInterrupt{};

    // iret is represented by two machine instructions.  The first one
    // restores status, so asynchronous interrupts must not be accepted
    // between those two instructions.
    bool suppressInterruptCheckOnce = false;

public:

    Emulator();
    ~Emulator();

    void loadHexFile(const std::string& path);
    void run();

private:
    uint8_t read8(uint32_t addr) const;
    uint32_t read32(uint32_t addr) const;
    void write8(uint32_t addr, uint8_t value);
    void write32(uint32_t addr, uint32_t value);
    void write32Raw(uint32_t addr, uint32_t value);

    uint32_t fetchInstruction(uint32_t address) const;


    int32_t signExtend12(uint32_t value);

    void executeArithmetic(uint8_t mod, uint8_t a, uint8_t b, uint8_t c);
    void executeLogic(uint8_t mod, uint8_t a, uint8_t b, uint8_t c);

    void executeShift(uint8_t mod, uint8_t a, uint8_t b, uint8_t c);
    void executeLoad(uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int32_t disp);
    
    void executeStore(uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int32_t disp);

    void executeCall(uint8_t mode, uint8_t regA, uint8_t regB, int32_t disp); 
    void executeJump(uint8_t mode, uint8_t regA, uint8_t regB, uint8_t regC, int32_t disp); 

    void executeInterrupt(uint32_t causeValue);
    void servicePendingInterrupts();

    void configureTerminal();
    void restoreTerminal();
    void pollTerminal();

    void configureTimer(uint32_t configuration);
    void updateTimer();
    std::chrono::milliseconds timerPeriodForConfiguration(uint32_t configuration) const;


    void execute(uint32_t instr);

    void printState() const;
};

inline Emulator::Emulator() {
    gpr[15] = 0x40000000;

    gpr[0] = 0;

    csr[0] = 0;
    csr[1] = 0;
    csr[2] = 0;

}

#endif
