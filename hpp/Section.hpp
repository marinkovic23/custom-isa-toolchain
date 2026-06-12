#pragma once

#include <cstdint>
#include <string>
#include <vector>

typedef struct {
    std::string name;  
    std::vector <uint8_t> bytes;

} Section;