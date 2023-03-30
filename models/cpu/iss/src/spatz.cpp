#include "spatz.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR REGISTER FILE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VRegfile::VRegfile(Iss &iss) : iss(iss){}

void VRegfile::reset(bool active){
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

static inline void lib_VVADD (Spatz *spatz, int vs1      , int vs2, int vd, bool vm){
    //for (int i = 0; i < NB_VEL; i++){
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            spatz->vregfile.vregs[vd][i] = spatz->vregfile.vregs[vs1][i] + spatz->vregfile.vregs[vs2][i];
        }
    }
}

static inline void lib_VXADD (Spatz *spatz, iss_reg_t rs1, int vs2, int vd, bool vm){// b is a iss_Vel_t bit register
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            spatz->vregfile.vregs[vd][i] = spatz->vregfile.vregs[vs2][i] + (iss_Vel_t)rs1;
        }
    }
}

static inline void lib_VIADD (Spatz *spatz, iss_sim_t sim, int vs2, int vd, bool vm){// b is a iss_Vel_t bit immediate 
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            spatz->vregfile.vregs[vd][i] = spatz->vregfile.vregs[vs2][i] + (iss_Vel_t)sim;
        }
    }
}

static inline void lib_VVSUB (Spatz *spatz, int vs1      , int vs2, int vd, bool vm){
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            spatz->vregfile.vregs[vd][i] = spatz->vregfile.vregs[vs1][i] - spatz->vregfile.vregs[vs2][i];
        }
    }
}

static inline void lib_VXSUB (Spatz *spatz, iss_reg_t rs1, int vs2, int vd, bool vm){// b is a iss_Vel_t bit register
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            spatz->vregfile.vregs[vd][i] = spatz->vregfile.vregs[vs2][i] - (iss_Vel_t)rs1;
        }
    }
}

static inline void lib_VVFMAC(Spatz *spatz, int vs1      , int vs2, int vd, bool vm){
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            switch (SEW){
            case 8:

                break;
            case 16:
                FF_INIT_3(spatz->vregfile.vregs[vs1][i], spatz->vregfile.vregs[vs2][i], spatz->vregfile.vregs[vd][i], 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            case 32:
                FF_INIT_3(spatz->vregfile.vregs[vs1][i], spatz->vregfile.vregs[vs2][i], spatz->vregfile.vregs[vd][i], 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            case 64:
                FF_INIT_3(spatz->vregfile.vregs[vs1][i], spatz->vregfile.vregs[vs2][i], spatz->vregfile.vregs[vd][i], 11, 53)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            default:
                break;
            }
        }
    }
}

static inline void lib_VXFMAC(Spatz *spatz, iss_reg_t rs1, int vs1, int vd, bool vm){
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            switch (SEW){
            case 8:

                break;
            case 16:
                FF_INIT_3(spatz->vregfile.vregs[vs1][i], rs1, spatz->vregfile.vregs[vd][i], 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            case 32:
                FF_INIT_3(spatz->vregfile.vregs[vs1][i], rs1, spatz->vregfile.vregs[vd][i], 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            case 64:
                FF_INIT_3(spatz->vregfile.vregs[vs1][i], rs1, spatz->vregfile.vregs[vd][i], 11, 53)
                feclearexcept(FE_ALL_EXCEPT);
                ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);                
                break;
            default:
                break;
            }
        }
    }
}

static inline void lib_VVFADD(Spatz *spatz, int vs1      , int vs2, int vd, bool vm){
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            switch (SEW){
            case 8:

                break;
            case 16:
                FF_INIT_2(spatz->vregfile.vregs[vs1][i], spatz->vregfile.vregs[vs2][i], 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);
                break;
            case 32:
                FF_INIT_2(spatz->vregfile.vregs[vs1][i], spatz->vregfile.vregs[vs2][i], 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);               
                break;
            case 64:
                FF_INIT_2(spatz->vregfile.vregs[vs1][i], spatz->vregfile.vregs[vs2][i],11, 53)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);               
                break;
            default:
                break;
            }
        }
    }
}

static inline void lib_VFFADD(Spatz *spatz, iss_reg_t rs1, int vs1, int vd, bool vm){
    for (int i = vstart; i < vl; i++){
        if(vm || !(spatz->vregfile.vregs[0][i]%2)){
            switch (SEW){
            case 8:

                break;
            case 16:
                FF_INIT_2(spatz->vregfile.vregs[vs1][i], iss_reg_t rs1, 5, 10)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);
                break;
            case 32:
                FF_INIT_2(spatz->vregfile.vregs[vs1][i], iss_reg_t rs1, 8, 23)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);               
                break;
            case 64:
                FF_INIT_2(spatz->vregfile.vregs[vs1][i], iss_reg_t rs1,11, 53)
                feclearexcept(FE_ALL_EXCEPT);
                ff_add(&ff_res, &ff_a, &ff_b);
                //update_fflags_fenv(s);
                spatz->vregfile.vregs[vd][i] = flexfloat_get_bits(&ff_res);               
                break;
            default:
                break;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR LOAD STORE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Vlsu::Vlsu_io_access(uint64_t addr, int size, uint8_t *data, bool is_write)// size in byte
{
    this->io_pending_addr = addr;
    this->io_pending_size = size;
    this->io_pending_data = data;
    this->io_pending_is_write = is_write;
    this->waiting_io_response = true;

    this->handle_pending_io_access();

    return this->io_retval;
}

void Vlsu::handle_pending_io_access()
{
    if (this->io_pending_size > 0){
        vp::io_req *req = &this->io_req;

        uint32_t addr = this->io_pending_addr;
        uint32_t addr_aligned = addr & ~(VLEN / 8 - 1);
        int size = addr_aligned + VLEN/8 - addr;
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
static inline void lib_VLE8V (Spatz *spatz, int rs1, int vd , bool vm){
    uint8_t* data;
    for (int i = vstart; i < vl; i+=4){
        if(!i){
            spatz->vlsu.Vlsu_io_access(rs1,VLEN/8,data,false);
        }else{
            spatz->vlsu.handle_pending_io_access();
        }
        spatz->vregfile.vregs[vd][i+0] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? data[0] : spatz->vregfile.vregs[vd][i+0];
        spatz->vregfile.vregs[vd][i+1] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? data[1] : spatz->vregfile.vregs[vd][i+1];
        spatz->vregfile.vregs[vd][i+2] = (vm || !(spatz->vregfile.vregs[0][i+2]%2)) ? data[2] : spatz->vregfile.vregs[vd][i+2];
        spatz->vregfile.vregs[vd][i+3] = (vm || !(spatz->vregfile.vregs[0][i+3]%2)) ? data[3] : spatz->vregfile.vregs[vd][i+3];
    }
}

static inline void lib_VLE16V(Spatz *spatz, int rs1, int vd , bool vm){
    uint8_t* data;

    for (int i = vstart; i < vl; i+=2){
        if(!i){
            spatz->vlsu.Vlsu_io_access(rs1,VLEN/8,data,false);
        }else{
            spatz->vlsu.handle_pending_io_access();
        }
        spatz->vregfile.vregs[vd][i+0] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? (data[1]*pow(2,8) + data[0]) : spatz->vregfile.vregs[vd][i+0];
        spatz->vregfile.vregs[vd][i+1] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? (data[3]*pow(2,8) + data[2]) : spatz->vregfile.vregs[vd][i+1];
    }
}

static inline void lib_VLE32V(Spatz *spatz, int rs1, int vd , bool vm){
    uint8_t* data;

    for (int i = vstart; i < vl; i+=1){
        if(!i){
            spatz->vlsu.Vlsu_io_access(rs1,VLEN/8,data,false);
        }else{
            spatz->vlsu.handle_pending_io_access();
        }
        spatz->vregfile.vregs[vd][i+0] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? (data[3]*pow(2,8*3) + data[2]*pow(2,8*2) + data[1]*pow(2,8) + data[0]) : spatz->vregfile.vregs[vd][i+0];
    }
}

static inline void lib_VLE64V(Spatz *spatz, int rs1, int vd , bool vm){
    uint8_t* data;
    u_int64_t temp;
    for (int i = vstart; i < vl*2; i+=1){
        if(!i){
            spatz->vlsu.Vlsu_io_access(rs1,VLEN/8,data,false);
        }else{
            spatz->vlsu.handle_pending_io_access();
        }
        if(!(i%2)){
        temp = (data[0]*pow(2,8*3) + data[0]*pow(2,8*2) + data[0]*pow(2,8) + data[0]);
        }
        spatz->vregfile.vregs[vd][i+0] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? temp*pow(2,8*4) + (data[0]*pow(2,8*3) + data[0]*pow(2,8*2) + data[0]*pow(2,8) + data[0]) : spatz->vregfile.vregs[vd][i+0];
    }
}

static inline void lib_VSE8V (Spatz *spatz, int rs1, int vs3, bool vm){
    uint8_t* data;
    for (int i = vstart; i < vl; i+=4){
/*
        data[0] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? spatz->vregfile.vregs[vd][i+0] : 0;
        data[1] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? spatz->vregfile.vregs[vd][i+1] : 0;
        data[2] = (vm || !(spatz->vregfile.vregs[0][i+2]%2)) ? spatz->vregfile.vregs[vd][i+2] : 0;
        data[3] = (vm || !(spatz->vregfile.vregs[0][i+3]%2)) ? spatz->vregfile.vregs[vd][i+3] : 0;
*/
        data[3] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? spatz->vregfile.vregs[vd][i+0] : 0;
        data[2] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? spatz->vregfile.vregs[vd][i+1] : 0;
        data[1] = (vm || !(spatz->vregfile.vregs[0][i+2]%2)) ? spatz->vregfile.vregs[vd][i+2] : 0;
        data[0] = (vm || !(spatz->vregfile.vregs[0][i+3]%2)) ? spatz->vregfile.vregs[vd][i+3] : 0;

        if(!i){
            spatz->vlsu.Vlsu_io_access(rs1,VLEN/8,data,true);
        }else{
            spatz->vlsu.handle_pending_io_access();
        }
    }
}

static inline void lib_VSE16V(Spatz *spatz, int rs1, int vs3, bool vm){
    uint8_t* data;
    for (int i = vstart; i < vl; i+=2){
        /*
        data[0] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? spatz->vregfile.vregs[vd][i+0] : 0;
        data[1] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? spatz->vregfile.vregs[vd][i+1] : 0;
        */

        data[3] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? spatz->vregfile.vregs[vd][i+0]/pow(2,8) : 0;
        data[2] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? spatz->vregfile.vregs[vd][i+0] : 0;
        data[1] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? spatz->vregfile.vregs[vd][i+1]/pow(2,8) : 0;
        data[0] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? spatz->vregfile.vregs[vd][i+1] : 0;

        if(!i){
            spatz->vlsu.Vlsu_io_access(rs1,VLEN/8,data,true);
        }else{
            spatz->vlsu.handle_pending_io_access();
        }
    }
}

static inline void lib_VSE32V(Spatz *spatz, int rs1, int vs3, bool vm){
    uint8_t* data;
    for (int i = vstart; i < vl; i+=1){
        //data[0] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? spatz->vregfile.vregs[vd][i+0] : 0;
        data[3] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? spatz->vregfile.vregs[vd][i+0]/pow(2,8*3) : 0;
        data[2] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? spatz->vregfile.vregs[vd][i+0]/pow(2,8*2) : 0;
        data[1] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? spatz->vregfile.vregs[vd][i+1]/pow(2,8*1) : 0;
        data[0] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? spatz->vregfile.vregs[vd][i+1]/pow(2,8*0) : 0;
        if(!i){
            spatz->vlsu.Vlsu_io_access(rs1,VLEN/8,data,true);
        }else{
            spatz->vlsu.handle_pending_io_access();
        }
    }
}

static inline void lib_VSE64V(Spatz *spatz, int rs1, int vs3, bool vm){
    uint8_t* data;
    uint32_t temp;
    for (int i = vstart; i < vl*2; i+=1){
        if(i%2){
            temp = spatz->vregfile.vregs[vd][i+0]/pow(2,8*4);
            data[3] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*3) : 0;
            data[2] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*2) : 0;
            data[1] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*1) : 0;
            data[0] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*0) : 0;
        }else{
            temp = spatz->vregfile.vregs[vd][i+0];
            data[3] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*3) : 0;
            data[2] = (vm || !(spatz->vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*2) : 0;
            data[1] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*1) : 0;
            data[0] = (vm || !(spatz->vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*0) : 0;
        }
        if(!i){
            spatz->vlsu.Vlsu_io_access(rs1,VLEN/8,data,true);
        }else{
            spatz->vlsu.handle_pending_io_access();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR CONFIGURATION SETTING
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline iss_uim_t lib_VSETVLI(Spatz *spatz, int idxRs1, int idxRd, int rs1, iss_uim_t lmul, iss_uim_t sew, bool ta,  bool ma){
    uint32_t AVL;
    // SET NEW VTYPE
    // spatz_req.vtype = {1'b0, decoder_req_i.instr[27:20]};

    if(VLEN*LMUL_VALUES[lmul]/SEW_VALUES[sew] == VLMAX){
        vstart = 0;
        SEW = SEW_VALUES[sew];
        LMUL = LMUL_VALUES[lmul];
        VMA = ma;
        VTA = ta;
        //  localparam int unsigned MAXVL  = VLEN; and VLEN = 256
        if(!idxRs1){ // in SV implementation it is checked with rs1 = 1
            AVL = rs1;
            vl = MIN(AVL,VLMAX);//spec page 30 - part c and d
        }else if(VLMAX < rs1){
            AVL = UINT32_MAX;
            vl  = VLMAX;
            //vl = VLEN/SEW;
        }else{
            AVL = vl;
        }
    } else {
        // vtype for invalid situation
        //vtype_d = '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0};
        //vl_d    = '0;
    }
    return vl;
    //Not sure about the write back procedure
}
