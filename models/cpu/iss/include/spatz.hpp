#ifndef SPATZ_HPP
#define SPATZ_HPP

#include "types.hpp"
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

#define REG_SET(reg,val) (*insn->out_regs_ref[reg] = (val))
#define REG_GET(reg) (*insn->in_regs_ref[reg])

#define SIM_GET(index) insn->sim[index]
#define UIM_GET(index) insn->uim[index]

#define REG_IN(reg) (insn->in_regs[reg])
#define REG_OUT(reg) (insn->out_regs[reg])

typedef uint8_t iss_Vel_t;
#define ISS_NB_VREGS 32
//#define NB_VEL VLEN/SEW
//#define NB_VEL 256/8
#define NB_VEL 2048/8//???????????????????????
//#define VLMAX NB_VEL*iss->spatz.LMUL
#define VLMAX (int)((2048*LMUL)/SEW)

#define XLEN = ISS_REG_WIDTH
#define FLEN = ISS_REG_WIDTH
#define ELEN = MAX(XLEN,FLEN)


// const float LMUL_VALUES[] = {1.0f, 2.0f, 4.0f, 8.0f, 0, 0.125f, 0.25f, 0.5f};

// const int SEW_VALUES[] = {8,16,32,64,128,256,512,1024};


// int   VLEN = 256;
// int   SEW  = SEW_VALUES[2];
// float LMUL = LMUL_VALUES[0];
// bool  VMA  = 0;
// bool  VTA  = 0;
// int vstart = 0;
// iss_reg_t vl;

//iss_uim_t vl;// the vl in vector CSRs


class VRegfile{
public:

    VRegfile(Iss &iss);

    inline void reset(bool active);

    iss_Vel_t vregs[ISS_NB_VREGS][(int)NB_VEL];

    //inline iss_reg_t *reg_ref(int reg);
    //inline iss_reg_t *reg_store_ref(int reg);
    //inline void set_Vreg(int reg, iss_Vel_t* value);
    //inline iss_Vel_t* get_Vreg(int reg);
    //inline iss_reg64_t get_reg64(int reg);
    //inline void set_reg64(int reg, iss_reg64_t value);

private:
    Iss &iss;

};

class Vlsu {
public:
    inline int Vlsu_io_access(Iss *iss, uint64_t addr, int size, uint8_t *data, bool is_write);

    inline void handle_pending_io_access(Iss *iss);
    static void data_response(void *__this, vp::io_req *req);


    Vlsu(Iss &iss);
    void build();

    vp::io_master io_itf[4];
    vp::io_req io_req;
    vp::clock_event *event;
    int io_retval;
    uint64_t io_pending_addr;
    int io_pending_size;
    uint8_t *io_pending_data;
    bool io_pending_is_write;
    bool waiting_io_response;

private:
    Iss &iss;

};
// define a new class named SPATZ like ISS in class.hpp


class Spatz
{
public:
    Spatz(Iss &iss);

    void build();
    void reset(bool reset);

    //const float LMUL_VALUES[8] = {1.0f, 2.0f, 4.0f, 8.0f, 0, 0.125f, 0.25f, 0.5f};

    //                          V 1.0
    const float LMUL_VALUES[8] = {1.0f, 2.0f, 4.0f, 8.0f, 1.0f, 0.125f, 0.25f, 0.5f};

    //                          V 0.8
    // const float LMUL_VALUES[4] = {1.0f, 2.0f, 4.0f, 8.0f};
    const int SEW_VALUES[8] = {8,16,32,64,128,256,512,1024};


    int   VLEN   = 256;
    int   SEW_t    = SEW_VALUES[2];
    float LMUL_t   = LMUL_VALUES[0];
    bool  VMA    = 0;
    bool  VTA    = 0;
    // int   vstart = 0;
    // uint8_t counter = 1;
    // iss_reg_t vtype;
    // iss_reg_t vl;

    VRegfile vregfile;
    Vlsu vlsu;

};



#endif