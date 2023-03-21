#include "Vregfile.hpp"

class VRegfile{
public:

    //VRegfile(Iss &iss);

    void reset(bool active);

    iss_Vel_t vregs[ISS_NB_VREGS][NB_VEL];

    //inline iss_reg_t *reg_ref(int reg);
    //inline iss_reg_t *reg_store_ref(int reg);
    inline void set_Vreg(int reg, iss_Vel_t* value);
    inline iss_Vel_t* get_Vreg(int reg);
    //inline iss_reg64_t get_reg64(int reg);
    //inline void set_reg64(int reg, iss_reg64_t value);

private:
    Iss &iss;

};


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


inline void Regfile::set_Vreg(int reg, iss_Vel_t* value){
    for (int i = 0; i < NB_VEL; i++){
        this->vregs[reg][i] = value++;
    }
}

inline iss_Vel_t* Regfile::get_Vreg(int reg){
    return &this->vregs[reg];
}
