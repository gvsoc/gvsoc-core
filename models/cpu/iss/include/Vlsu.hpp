#ifndef VLSU_hpp
#define VLSU_hpp

#include <types.hpp>


typedef uint8_t iss_Vel_t;
#define ELEN 8
#define VLEN 256
#define ISS_NB_VREGS 32
#define NB_VEL VLEN/ELEN


#define LIB_CALL3(name, s0, s1, s2) name(Vregfile vregfile,s0, s1, s2)
#define LIB_CALL4(name, s0, s1, s2, s3) name(Vregfile vregfile,s0, s1, s2, s3)


#define SIM_GET(index) insn->sim[index]
#define UIM_GET(index) insn->uim[index]

#define REG_IN(reg) (insn->in_regs[reg])
#define REG_OUT(reg) (insn->out_regs[reg])


class Vlsu;


//VLEN = vectors bit width = 256
//NB_VEL = number of element in each vector = 256/8 = 32
//The starting memory address of load instruction is in rs1
static inline void lib_VLE8V(Vlsu* vlsu,VRegfile* vregfile, int rs1, int vd, int vm){
    iss_Vel_t* data;
    for (int i = 0; i < NB_VEL; i+=4){
        if(vm || !(vregfile.vregs[0]%2)){
            if(!i){
                vlsu->Vlsu_io_access(rs1,VLEN/8,data,false)
            }else{
                vlsu->handle_pending_io_access();
            }
            vregfile.vregs[vd][i+0] = (vm || !(vregfile.vregs[0][i+0]%2)) ? data[0] : vregfile.vregs[vd][i+0];
            vregfile.vregs[vd][i+1] = (vm || !(vregfile.vregs[0][i+1]%2)) ? data[1] : vregfile.vregs[vd][i+1];
            vregfile.vregs[vd][i+2] = (vm || !(vregfile.vregs[0][i+2]%2)) ? data[2] : vregfile.vregs[vd][i+2];
            vregfile.vregs[vd][i+3] = (vm || !(vregfile.vregs[0][i+3]%2)) ? data[3] : vregfile.vregs[vd][i+3];
        }
    }
}

static inline void lib_VXADD(VRegfile *vregfile, iss_reg_t rs1, int vs2, int vd, int vm){// b is a iss_Vel_t bit register
    for (int i = 0; i < NB_VEL; i++){
        if(vm || !(vregfile.vregs[0][i]%2)){
            vregfile->vregs[vd][i] = vregfile->vregs[vs2][i] + (iss_Vel_t)rs1;
        }
    }
}

static inline void lib_VIADD(VRegfile *vregfile, iss_sim_t sim, int vs2, int vd, int vm){// b is a iss_Vel_t bit immediate 
    for (int i = 0; i < NB_VEL; i++){
        if(vm || !(vregfile.vregs[0][i]%2)){
            vregfile->vregs[vd][i] = vregfile->vregs[vs2][i] + (iss_Vel_t)sim;
        }
    }
}

static inline void lib_VVSUB(VRegfile *vregfile, int vs1, int vs2, int vd, int vm){
    for (int i = 0; i < NB_VEL; i++){
        if(vm || !(vregfile.vregs[0][i]%2)){
            vregfile.vregs[vd][i] = vregfile->vregs[vs1][i] - vregfile.vregs[vs2][i];
        }
    }
}

static inline void lib_VXSUB(VRegfile *vregfile, iss_reg_t vs1, int vs2, int vd, int vm){// b is a iss_Vel_t bit register
    for (int i = 0; i < NB_VEL; i++){
        if(vm || !(vregfile.vregs[0][i]%2)){
            vregfile->vregs[vd][i] = vregfile->vregs[vs2][i] - (iss_Vel_t)rs1;
        }
    }
}

static inline void lib_VVFMAC(VRegfile *vregfile, int vs1, int vs2, int vd, int vm){// b is a iss_Vel_t bit register
    for (int i = 0; i < NB_VEL; i++){
        if(vm || !(vregfile.vregs[0][i]%2)){
            switch (ELEN){
            case 8:

                break;
            case 16:
                FF_INIT_3(vregfile->vregs[vs1][i], vregfile->vregs[vs2][i], vregfile->vregs[vd][i], 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                vregfile->vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            case 32:
                FF_INIT_3(vregfile->vregs[vs1][i], vregfile->vregs[vs2][i], vregfile->vregs[vd][i], 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                vregfile->vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            case 64:
                FF_INIT_3(vregfile->vregs[vs1][i], vregfile->vregs[vs2][i], vregfile->vregs[vd][i], 11, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                vregfile->vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            default:
                break;
            }
        }
    }
}
/*    vregfile->vregs[vd][i] = vregfile->vregs[vs2][i] - (iss_Vel_t)rs1;
    REG_SET(0, LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0), REG_GET(1), REG_GET(2), 8, 23, UIM_GET(0)));
    return insn->next;

    unsigned int result = lib_flexfloat_madd(s, a, b, c, e, m);

    FF_INIT_3(a, b, c, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
    //update_fflags_fenv(s);
    flexfloat_get_bits(&ff_res);
}*/







#endif