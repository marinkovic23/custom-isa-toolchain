#pragma once

#include "Common.hpp"

typedef struct {
    std::string section;
    uint32_t offset;
    std::string symbol;
} Relocation;