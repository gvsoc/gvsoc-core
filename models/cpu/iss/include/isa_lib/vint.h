#ifndef VINT_H
#define VINT_H

#include "spatz.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR REGISTER FILE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "flexfloat.h"
#include <stdint.h>
#include <math.h>
#include <fenv.h>
#include <cores/ri5cy/class.hpp>
#pragma STDC FENV_ACCESS ON

#define FF_INIT_2(a, b, e, m)                        \
    flexfloat_t ff_a, ff_b, ff_res;                  \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_b, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);                    \
    flexfloat_set_bits(&ff_b, b);

#define FF_INIT_3(a, b, c, e, m)                     \
    flexfloat_t ff_a, ff_b, ff_c, ff_res;            \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_b, env);                             \
    ff_init(&ff_c, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);                    \
    flexfloat_set_bits(&ff_b, b);                    \
    flexfloat_set_bits(&ff_c, c);






///////////////////////////////////////////////////////////////////
// #include "types.hpp"
// #include "math.h"
// //#include "isa_lib/vint.h"


// //#define MAX(a, b) ((a) > (b) ? (a) : (b))
// //#define MIN(a, b) ((a) < (b) ? (a) : (b))

// #define LIB_CALL3(name, s0, s1, s2) name(iss, s0, s1, s2)
// #define LIB_CALL4(name, s0, s1, s2, s3) name(iss, s0, s1, s2, s3)
// #define LIB_CALL5(name, s0, s1, s2, s3, s4) name(iss, s0, s1, s2, s3, s4)
// #define LIB_CALL6(name, s0, s1, s2, s3, s4, s5) name(iss, s0, s1, s2, s3, s4, s5)
// #define LIB_CALL7(name, s0, s1, s2, s3, s4, s5, s6) name(iss, s0, s1, s2, s3, s4, s5, s6)

// #define REG_SET(reg,val) (*insn->out_regs_ref[reg] = (val))
// #define REG_GET(reg) (*insn->in_regs_ref[reg])

// #define SIM_GET(index) insn->sim[index]
// #define UIM_GET(index) insn->uim[index]

// #define REG_IN(reg) (insn->in_regs[reg])
// #define REG_OUT(reg) (insn->out_regs[reg])

// typedef uint8_t iss_Vel_t;
// #define ISS_NB_VREGS 32
// //#define NB_VEL VLEN/SEW
// #define NB_VEL 256/8
// #define VLMAX NB_VEL*iss->spatz.LMUL

// #define XLEN = ISS_REG_WIDTH
// #define FLEN = ISS_REG_WIDTH
// #define ELEN = MAX(XLEN,FLEN)

// class Spatz;
///////////////////////////////////////////////////////////////////

//VRegfile::VRegfile(Iss &iss) : iss(iss){}

inline void VRegfile::reset(bool active){
    if (active){
        for (int i = 0; i < ISS_NB_VREGS; i++){
            for (int j = 0; j < NB_VEL; j++){
                this->vregs[i][j] = i == 0 ? 0 : 0x57575757;
            }
        }
    }
}

/*inline void Regfile::set_Vreg(int reg, iss_Vel_t* value){
    for (int i = vstart; i < vl; i++){
        this->vregs[reg][i] = value++;
    }
}*/

/*inline iss_Vel_t* Regfile::get_Vreg(int reg){
    return &this->vregs[reg];
}*/

static inline void lib_VVADD (Iss *iss, int vs1, int vs2, int vd, bool vm){
    //for (int i = 0; i < NB_VEL; i++){
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            iss->spatz.vregfile.vregs[vd][i] = iss->spatz.vregfile.vregs[vs1][i] + iss->spatz.vregfile.vregs[vs2][i];
        }
    }
}

static inline void lib_VXADD (Iss *iss, iss_reg_t rs1, int vs2, int vd, bool vm){// b is a iss_Vel_t bit register
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            iss->spatz.vregfile.vregs[vd][i] = iss->spatz.vregfile.vregs[vs2][i] + (iss_Vel_t)rs1;
        }
    }
}

static inline void lib_VIADD (Iss *iss, iss_sim_t sim, int vs2, int vd, bool vm){// b is a iss_Vel_t bit immediate 
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            iss->spatz.vregfile.vregs[vd][i] = iss->spatz.vregfile.vregs[vs2][i] + (iss_Vel_t)sim;
        }
    }
}

static inline void lib_VVSUB (Iss *iss, int vs1      , int vs2, int vd, bool vm){
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            iss->spatz.vregfile.vregs[vd][i] = iss->spatz.vregfile.vregs[vs1][i] - iss->spatz.vregfile.vregs[vs2][i];
        }
    }
}

static inline void lib_VXSUB (Iss *iss, iss_reg_t rs1, int vs2, int vd, bool vm){// b is a iss_Vel_t bit register
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            iss->spatz.vregfile.vregs[vd][i] = iss->spatz.vregfile.vregs[vs2][i] - (iss_Vel_t)rs1;
        }
    }
}

static inline void lib_VVFMAC(Iss *iss, int vs1      , int vs2, int vd, bool vm){
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            switch (iss->spatz.SEW){
            case 8:{

                break;
            }
            case 16:{
                FF_INIT_3(iss->spatz.vregfile.vregs[vs1][i], iss->spatz.vregfile.vregs[vs2][i], iss->spatz.vregfile.vregs[vd][i], 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);
                break;
            }
            case 32:{
                FF_INIT_3(iss->spatz.vregfile.vregs[vs1][i], iss->spatz.vregfile.vregs[vs2][i], iss->spatz.vregfile.vregs[vd][i], 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            }
            case 64:{
                FF_INIT_3(iss->spatz.vregfile.vregs[vs1][i], iss->spatz.vregfile.vregs[vs2][i], iss->spatz.vregfile.vregs[vd][i], 11, 53)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            }
            default:
                break;
            }
        }
    }
}

static inline void lib_VFFMAC(Iss *iss, iss_reg_t rs1, int vs1, int vd, bool vm){
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            switch (iss->spatz.SEW){
            case 8:{

                break;
            }
            case 16:{
                FF_INIT_3(iss->spatz.vregfile.vregs[vs1][i], rs1, iss->spatz.vregfile.vregs[vd][i], 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            }
            case 32:{
                FF_INIT_3(iss->spatz.vregfile.vregs[vs1][i], rs1, iss->spatz.vregfile.vregs[vd][i], 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            }
            case 64:{
                FF_INIT_3(iss->spatz.vregfile.vregs[vs1][i], rs1, iss->spatz.vregfile.vregs[vd][i], 11, 53)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            }
            default:
                break;
            }
        }
    }
}

static inline void lib_VVFADD(Iss *iss, int vs1      , int vs2, int vd, bool vm){
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            switch (iss->spatz.SEW){
            case 8:{

                break;
            }
            case 16:{
                FF_INIT_2(iss->spatz.vregfile.vregs[vs1][i], iss->spatz.vregfile.vregs[vs2][i], 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);
                break;
            }
            case 32:{
                FF_INIT_2(iss->spatz.vregfile.vregs[vs1][i], iss->spatz.vregfile.vregs[vs2][i], 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);               
                break;
            }
            case 64:{
                FF_INIT_2(iss->spatz.vregfile.vregs[vs1][i], iss->spatz.vregfile.vregs[vs2][i],11, 53)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);               
                break;
            }                
            default:
                break;
            }
        }
    }
}

static inline void lib_VFFADD(Iss *iss, iss_reg_t rs1, int vs1, int vd, bool vm){
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(vm || !(iss->spatz.vregfile.vregs[0][i]%2)){
            switch (iss->spatz.SEW){
            case 8:{

                break;
            }
            case 16:{
                FF_INIT_2(iss->spatz.vregfile.vregs[vs1][i], rs1, 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);
                break;
            }
            case 32:{
                FF_INIT_2(iss->spatz.vregfile.vregs[vs1][i], rs1, 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);               
                break;
            }
            case 64:{
                FF_INIT_2(iss->spatz.vregfile.vregs[vs1][i], rs1,11, 53)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                iss->spatz.vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);               
                break;
            }
            default:
                break;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR LOAD STORE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline int Vlsu::Vlsu_io_access(Iss *iss, uint64_t addr, int size, uint8_t *data, bool is_write)// size in byte
{
    this->io_pending_addr = addr;
    this->io_pending_size = size;
    this->io_pending_data = data;
    this->io_pending_is_write = is_write;
    this->waiting_io_response = true;

    this->handle_pending_io_access(iss);

    return this->io_retval;
}

inline void Vlsu::handle_pending_io_access(Iss *iss)
{
    if (this->io_pending_size > 0){
        vp::io_req *req = &this->io_req;

        uint32_t addr = this->io_pending_addr;
        uint32_t addr_aligned = addr & ~(iss->spatz.VLEN / 8 - 1);
        int size = addr_aligned + iss->spatz.VLEN/8 - addr;
        if (size > this->io_pending_size){
            size = this->io_pending_size;
        }

        req->init();
        req->set_addr(addr);
        req->set_size(size);
        req->set_is_write(this->io_pending_is_write);
        req->set_data(this->io_pending_data);

        this->io_pending_data += size;
        this->io_pending_size -= size;
        this->io_pending_addr += size;

        int err = this->io_itf.req(req);
        if (err == vp::IO_REQ_OK){
            this->event->enqueue(this->io_req.get_latency() + 1);
        }
        else if (err == vp::IO_REQ_INVALID){
            this->waiting_io_response = false;
            this->io_retval = 1;
        }
        else{

        }
    }
    else{
        this->waiting_io_response = false;
        this->io_retval = 0;
    }
}

//VLEN = vectors bit width = 256
//NB_VEL = number of element in each vector = 256/8 = 32
//The starting memory address of load instruction is in rs1
static inline void lib_VLE8V (Iss *iss, int rs1, int vd , bool vm){
    uint8_t* data;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i+=4){
        if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,false);
        }else{
            iss->spatz.vlsu.handle_pending_io_access(iss);
        }
        iss->spatz.vregfile.vregs[vd][i+0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? data[0] : iss->spatz.vregfile.vregs[vd][i+0];
        iss->spatz.vregfile.vregs[vd][i+1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? data[1] : iss->spatz.vregfile.vregs[vd][i+1];
        iss->spatz.vregfile.vregs[vd][i+2] = (vm || !(iss->spatz.vregfile.vregs[0][i+2]%2)) ? data[2] : iss->spatz.vregfile.vregs[vd][i+2];
        iss->spatz.vregfile.vregs[vd][i+3] = (vm || !(iss->spatz.vregfile.vregs[0][i+3]%2)) ? data[3] : iss->spatz.vregfile.vregs[vd][i+3];
    }
}

static inline void lib_VLE16V(Iss *iss, int rs1, int vd , bool vm){
    uint8_t* data;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i+=2){
        if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,false);
        }else{
            iss->spatz.vlsu.handle_pending_io_access(iss);
        }
        iss->spatz.vregfile.vregs[vd][i+0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? (data[1]*pow(2,8) + data[0]) : iss->spatz.vregfile.vregs[vd][i+0];
        iss->spatz.vregfile.vregs[vd][i+1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? (data[3]*pow(2,8) + data[2]) : iss->spatz.vregfile.vregs[vd][i+1];
    }
}

static inline void lib_VLE32V(Iss *iss, int rs1, int vd , bool vm){
    uint8_t* data;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i+=1){
        if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,false);
        }else{
            iss->spatz.vlsu.handle_pending_io_access(iss);
        }
        iss->spatz.vregfile.vregs[vd][i+0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? (data[3]*pow(2,8*3) + data[2]*pow(2,8*2) + data[1]*pow(2,8) + data[0]) : iss->spatz.vregfile.vregs[vd][i+0];
    }
}

static inline void lib_VLE64V(Iss *iss, int rs1, int vd , bool vm){
    uint8_t* data;
    u_int64_t temp;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl*2; i+=1){
        if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,false);
        }else{
            iss->spatz.vlsu.handle_pending_io_access(iss);
        }
        if(!(i%2)){
        temp = (data[0]*pow(2,8*3) + data[0]*pow(2,8*2) + data[0]*pow(2,8) + data[0]);
        }
        iss->spatz.vregfile.vregs[vd][i+0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp*pow(2,8*4) + (data[0]*pow(2,8*3) + data[0]*pow(2,8*2) + data[0]*pow(2,8) + data[0]) : iss->spatz.vregfile.vregs[vd][i+0];
    }
}

static inline void lib_VSE8V (Iss *iss, int rs1, int vs3, bool vm){
    uint8_t* data;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i+=4){
/*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+2]%2)) ? iss->spatz.vregfile.vregs[vs3][i+2] : 0;
        data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+3]%2)) ? iss->spatz.vregfile.vregs[vs3][i+3] : 0;
*/
        data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+2]%2)) ? iss->spatz.vregfile.vregs[vs3][i+2] : 0;
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+3]%2)) ? iss->spatz.vregfile.vregs[vs3][i+3] : 0;

        if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,true);
        }else{
            iss->spatz.vlsu.handle_pending_io_access(iss);
        }
    }
}

static inline void lib_VSE16V(Iss *iss, int rs1, int vs3, bool vm){
    uint8_t* data;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i+=2){
        /*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        */

        data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8) : 0;
        data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8) : 0;
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;

        if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,true);
        }else{
            iss->spatz.vlsu.handle_pending_io_access(iss);
        }
    }
}

static inline void lib_VSE32V(Iss *iss, int rs1, int vs3, bool vm){
    uint8_t* data;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i+=1){
        //data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8*3) : 0;
        data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8*2) : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8*1) : 0;
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8*0) : 0;
        if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,true);
        }else{
            iss->spatz.vlsu.handle_pending_io_access(iss);
        }
    }
}

static inline void lib_VSE64V(Iss *iss, int rs1, int vs3, bool vm){
    uint8_t* data;
    uint32_t temp;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl*2; i+=1){
        if(i%2){
            temp = iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8*4);
            data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*3) : 0;
            data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*2) : 0;
            data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*1) : 0;
            data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*0) : 0;
        }else{
            temp = iss->spatz.vregfile.vregs[vs3][i+0];
            data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*3) : 0;
            data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*2) : 0;
            data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*1) : 0;
            data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*0) : 0;
        }
        if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,true);
        }else{
            iss->spatz.vlsu.handle_pending_io_access(iss);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR CONFIGURATION SETTING
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline iss_reg_t lib_VSETVLI(Iss *iss, int idxRs1, int idxRd, int rs1, iss_uim_t lmul, iss_uim_t sew, bool ta,  bool ma){
    uint32_t AVL;
    // SET NEW VTYPE
    // spatz_req.vtype = {1'b0, decoder_req_i.instr[27:20]};

    if(iss->spatz.VLEN*iss->spatz.LMUL_VALUES[lmul]/iss->spatz.SEW_VALUES[sew] == VLMAX){
        iss->spatz.vstart = 0;
        iss->spatz.SEW = iss->spatz.SEW_VALUES[sew];
        iss->spatz.LMUL = iss->spatz.LMUL_VALUES[lmul];
        iss->spatz.VMA = ma;
        iss->spatz.VTA = ta;
        //  localparam int unsigned MAXVL  = VLEN; and VLEN = 256
        if(!idxRs1){ // in SV implementation it is checked with rs1 = 1
            AVL = rs1;
            iss->spatz.vl = MIN(AVL,VLMAX);//spec page 30 - part c and d
        }else if(VLMAX < rs1){
            AVL = UINT32_MAX;
            iss->spatz.vl  = VLMAX;
            //vl = VLEN/SEW;
        }else{
            AVL = iss->spatz.vl;
        }
    } else {
        // vtype for invalid situation
        //vtype_d = '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0};
        //vl_d    = '0;
    }
    return iss->spatz.vl;
    //Not sure about the write back procedure
}



#endif