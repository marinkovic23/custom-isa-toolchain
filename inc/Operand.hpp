#pragma once

#include "Common.hpp"

enum class OperandKind {
    INVALID,
    IMMEDIATE_LITERAL,
    IMMEDIATE_SYMBOL,
    MEMORY_LITERAL,
    MEMORY_SYMBOL,
    REGISTER,
    REGISTER_INDIRECT,
    REGISTER_INDIRECT_LITERAL,
    REGISTER_INDIRECT_SYMBOL
};

typedef struct Operand {

    OperandKind kind = OperandKind::INVALID;
    uint8_t reg = 0;
    int64_t literal = 0;
    std::string symbol;


} Operand;


