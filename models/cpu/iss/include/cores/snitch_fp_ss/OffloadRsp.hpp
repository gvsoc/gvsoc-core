#pragma once
#include <cpu/iss/include/types.hpp>

class OffloadRsp
{
public:
    int rd;
    bool error;
    iss_reg_t data;
    unsigned int fflags;
    iss_reg_t pc;
    iss_insn_t *insn;
};