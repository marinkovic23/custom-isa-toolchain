#pragma once


#include "Common.hpp"


enum class SymbolBind {
    LOCAL,
    GLOBAL,
    EXTERN
};


typedef struct {
    std::string name;
    std::string section = "UND";
    uint32_t offset = 0;

    SymbolBind bind = SymbolBind::LOCAL;

    bool defined = false;

} Symbol;