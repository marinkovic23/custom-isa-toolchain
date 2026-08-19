#pragma once

#include "Common.hpp"

enum class RelocationType {
    // Writes (S + A) modulo 2^32 as a little-endian 32-bit word.
    ABS32,

    // Writes the signed value (S + A) into the instruction's 12-bit Disp
    // field. This relocation is absolute, not PC-relative, and therefore
    // requires -2048 <= S + A <= 2047.
    DISP12
};

typedef struct Relocation {
    std::string section;
    uint32_t offset = 0;
    std::string symbol;
    RelocationType type = RelocationType::ABS32;

    int32_t addend = 0;
} Relocation;