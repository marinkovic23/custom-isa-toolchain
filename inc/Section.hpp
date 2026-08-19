#pragma once

#include <cstdint>
#include <string>
#include <vector>

typedef struct {
    std::string name;  
    std::vector <uint8_t> bytes;

    uint32_t baseAddress = 0;
    uint32_t outputOffset = 0;
    
} Section;