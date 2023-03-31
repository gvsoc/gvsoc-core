#ifndef SPATZ_HPP
#define SPATZ_HPP

#include "types.hpp"
#include "math.h"
//#include "int.h"


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define LIB_CALL3(name, s0, s1, s2) name(iss, s0, s1, s2)
#define LIB_CALL4(name, s0, s1, s2, s3) name(iss, s0, s1, s2, s3)
#define LIB_CALL5(name, s0, s1, s2, s3, s4) name(iss, s0, s1, s2, s3, s4)
#define LIB_CALL6(name, s0, s1, s2, s3, s4, s5) name(iss, s0, s1, s2, s3, s4, s5)
#define LIB_CALL7(name, s0, s1, s2, s3, s4, s5, s6) name(iss, s0, s1, s2, s3, s4, s5, s6)

#define REG_SET(reg,val) (*insn->out_regs_ref[reg] = (val))
#define REG_GET(reg) (*insn->in_regs_ref[reg])

#define SIM_GET(index) insn->sim[index]
#define UIM_GET(index) insn->uim[index]

#define REG_IN(reg) (insn->in_regs[reg])
#define REG_OUT(reg) (insn->out_regs[reg])

typedef uint8_t iss_Vel_t;
#define ISS_NB_VREGS 32
//#define NB_VEL VLEN/SEW
#define NB_VEL 256/8
#define VLMAX NB_VEL*LMUL

#define XLEN = ISS_REG_WIDTH
#define FLEN = ISS_REG_WIDTH
#define ELEN = MAX(XLEN,FLEN)


const float LMUL_VALUES[] = {1.0f, 2.0f, 4.0f, 8.0f, 0, 0.125f, 0.25f, 0.5f};

const int SEW_VALUES[] = {8,16,32,64,128,256,512,1024};


int   VLEN = 256;
int   SEW  = SEW_VALUES[2];
float LMUL = LMUL_VALUES[0];
bool  VMA  = 0;
bool  VTA  = 0;
int vstart = 0;
uint32_t vl;

//iss_uim_t vl;// the vl in vector CSRs


class VRegfile{
public:

    VRegfile(Iss &iss);
    //VRegfile();

    void reset(bool active);

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

class Vlsu : public vp::Gdbserver_core{
public:
    int Vlsu_io_access(uint64_t addr, int size, uint8_t *data, bool is_write);

    void handle_pending_io_access();


    int gdbserver_get_id() override;
    void gdbserver_set_id(int id) override;
    std::string gdbserver_get_name() override;
    int gdbserver_reg_set(int reg, uint8_t *value) override;
    int gdbserver_reg_get(int reg, uint8_t *value) override;
    int gdbserver_regs_get(int *nb_regs, int *reg_size, uint8_t *value) override;
    int gdbserver_stop() override;
    int gdbserver_cont() override;
    int gdbserver_stepi() override;
    int gdbserver_state() override;
    void gdbserver_breakpoint_insert(uint64_t addr) override;
    void gdbserver_breakpoint_remove(uint64_t addr) override;
    void gdbserver_watchpoint_insert(bool is_write, uint64_t addr, int size) override;
    void gdbserver_watchpoint_remove(bool is_write, uint64_t addr, int size) override;
    int gdbserver_io_access(uint64_t addr, int size, uint8_t *data, bool is_write) override;



    vp::io_master io_itf;
    vp::io_req io_req;
    vp::clock_event *event;
    int io_retval;
    uint64_t io_pending_addr;
    int io_pending_size;
    uint8_t *io_pending_data;
    bool io_pending_is_write;
    bool waiting_io_response;
};
// define a new class named SPATZ like ISS in class.hpp


class Spatz
{
public:
//    Iss(vp::component &top);

    VRegfile vregfile;
    Vlsu vlsu;

};



#endif