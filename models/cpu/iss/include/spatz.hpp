#ifndef SPATZ_HPP
#define SPATZ_HPP

#include "cpu/iss/include/types.hpp"
#include "math.h"
//#include "isa_lib/vint.h"


//#define MAX(a, b) ((a) > (b) ? (a) : (b))
//#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define LIB_CALL3(name, s0, s1, s2) name(iss, s0, s1, s2)
#define LIB_CALL4(name, s0, s1, s2, s3) name(iss, s0, s1, s2, s3)
#define LIB_CALL5(name, s0, s1, s2, s3, s4) name(iss, s0, s1, s2, s3, s4)
#define LIB_CALL6(name, s0, s1, s2, s3, s4, s5) name(iss, s0, s1, s2, s3, s4, s5)
#define LIB_CALL7(name, s0, s1, s2, s3, s4, s5, s6) name(iss, s0, s1, s2, s3, s4, s5, s6)
#define LIB_CALL8(name, s0, s1, s2, s3, s4, s5, s6, s7) name(iss, s0, s1, s2, s3, s4, s5, s6, s7)

// #define REG_SET(reg,val) (*insn->out_regs_ref[reg] = (val))
// #define REG_GET(reg) (*insn->in_regs_ref[reg])

#define SIM_GET(index) insn->sim[index]
#define UIM_GET(index) insn->uim[index]

#define REG_IN(reg) (insn->in_regs[reg])
#define REG_OUT(reg) (insn->out_regs[reg])


class Spatz
{
public:
    Spatz(Iss &iss);

    void build();
    void reset(bool reset);
};



#endif
