#pragma once
#include <cpu/iss/include/types.hpp>

class OffloadReq
{
public:
    iss_reg_t pc;
    iss_insn_t insn;
    bool is_write;
    unsigned int frm;
};