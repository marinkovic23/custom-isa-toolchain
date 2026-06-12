#pragma once

#include "Common.hpp"

enum class OperandKind {
    IMMEDIATE_LITERAL,
    IMMEDIATE_SYMBOL,
    MEMORY_LITERAL,
    MEMORY_SYMBOL,
    REGISTER,
    REGISTER_INDIRECT,
    REGISTER_INDIRECT_LITERAL,
    REGISTER_INDIRECT_SYMBOL
};

typedef struct {

    OperandKind kind;
    int reg = -1;
    int32_t literal = 0;
    std::string symbol;


} Operand;


