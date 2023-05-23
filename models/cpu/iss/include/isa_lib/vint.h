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
#define mask(vm,bin) (!(vm) && !bin[i%8])

static inline void printBin(int size, bool *a, string name){
    printf("%s = ",&name[0]);
    for(int i = size-1; i >= 0  ; i--){
        printf("%d",a[i]);
    }
    printf("\n");
}

static inline void printHex(int size, bool *a, string name){
    printf("%s = ",&name[0]);
    int hexVal = 0;
    for(int i = size-1; i >= 0  ; i-=4){
        hexVal = a[i]*8 + a[i-1]*4 + a[i-2]*2 + a[i-3];
        printf("%x",hexVal);
    }
    printf("\n");
}

static inline int bin8ToChar(bool *bin,int s, int e){
    int c = 0;
    for(int i = s; i < e;i++){
        c += bin[i]*pow(2,i-s);
    }
    return c;
}

static inline int sewCase(int sew){
    int iteration = 0;
    switch (sew){
    case(8 ):iteration = 1;break;
    case(16):iteration = 2;break;
    case(32):iteration = 4;break;
    case(64):iteration = 8;break;
    default:printf("This SEW(%d) is not supported\n",sew);return -1;
        break;
    }
    return iteration;
}

static inline void intToBin(int size, int64_t dataIn, bool* res){
    int64_t temp = dataIn;
    for (int j = 0; j < size; j++) {
        if (temp%2) {
            res[j] = 1;
        } else {
            res[j] = 0;
        }
        temp = temp/2;
    }
}

static inline void buildDataInt(Iss *iss, int vs, int i, int64_t* data){
    iss_Vel_t temp[8];
    int iteration = sewCase(iss->spatz.SEW);
    for(int j = 0;j < iteration;j++){
        temp[j] = iss->spatz.vregfile.vregs[vs][i*iteration+j];
    }
    *data = 0;
    for(int j = 0;j < iteration;j++){
        *data += temp[j]*pow(2,8*j);
    }
}

static inline void buildDataBin(Iss *iss, int vs, int i, bool* dataBin){
    int iteration = sewCase(iss->spatz.SEW);
    for(int j = 0;j < iteration;j++){
        intToBin(8,(int64_t)abs(iss->spatz.vregfile.vregs[vs][i*iteration+j]), &dataBin[j*8]);
    }
}

static inline void myAbs(Iss *iss, int vs, int i, int64_t* data){
    iss_Vel_t temp[8];
    int cin = 0;
    int iteration = sewCase(iss->spatz.SEW);
    if(iss->spatz.vregfile.vregs[vs][i*iteration+iteration-1] > 127){
        cin = 1;
        for(int j = 0;j < iteration;j++){
            temp[j] = 255 - iss->spatz.vregfile.vregs[vs][i*iteration+j];
        }
    }else{
        cin = 0;
        for(int j = 0;j < iteration;j++){
            temp[j] = iss->spatz.vregfile.vregs[vs][i*iteration+j];
        }
    }

    *data = 0;
    for(int j = 0;j < iteration;j++){
        *data += temp[j]*(int64_t)pow(2,8*j);
    }
    *data += cin;
    *data = cin?(-*data):*data;
}

static inline void myAbsU(Iss *iss, int vs, int i, uint64_t* data){
    iss_Vel_t temp[8];
    int iteration = sewCase(iss->spatz.SEW);

    for(int j = 0;j < iteration;j++){
        temp[j] = iss->spatz.vregfile.vregs[vs][i*iteration+j];
    }

    *data = 0;
    for(int j = 0;j < iteration;j++){
        *data += temp[j]*(uint64_t)pow(2,8*j);
    }
}

static inline void writeToVReg(Iss *iss, int size, int vd, int i, bool *bin){
    int iteration = sewCase(size);
    for(int j = 0; j < iteration; j++){
        iss->spatz.vregfile.vregs[vd][i*iteration+j] = bin8ToChar(bin,8*j,8*(j+1));        
    }
}

static inline void binToInt(int size, bool* dataIn, int64_t *res){
    *res = 0;
    for (int j = 0; j < size; j++) {
        *res += dataIn[j]*(int64_t)pow(2,j);
    }
}

static inline void twosComplement(int size, bool* res){
    for (int j = 0; j < size; j++) {
        res[j] = !(res[j]);
    }
    for (int j = 0; j < size; j++) {
        if(res[j] == 1){
            res[j] = 0;
        }else{
            res[j] = 1;
            break;
        }
    }
}

static inline void extend2x(int size, bool *a, bool* res, bool isSgined){
    for(int i = 0; i < size ;i++){
        res[i] = a[i];
    }
    if(isSgined){
        for(int i = size; i < size*2 ;i++){
            res[i] = a[size-1];
        }
    }else{
        for(int i = size; i < size*2 ;i++){
            res[i] = 0;
        }
    }
}

static inline void binShift(int size, int shamt, bool *a, bool *res, bool shr, bool isSigned){
    if(shr){
        if(isSigned){
            for(int i = 0; i < shamt; i++){
                res[size-1-i] = a[size-1];
            }
        }else{
            for(int i = 0; i < shamt; i++){
                res[size-1-i] = 0;
            }
        }
        for(int i = shamt; i < size; i++){
            res[i-shamt] = a[i];
        }
    }else{
        for(int i = 0; i < shamt; i++){
            res[i] = 0;
        }
        for(int i = shamt; i < size; i++){
            res[i] = a[i-shamt];
        }
    }
}

static inline void binSub2(int size, bool *data1, bool *data2, bool *res){
    bool bin = 0;
    for(int i = 0; i < size; i++){
        res[i] = data1[i] ^ data2[i] ^ bin;
        bin = (!data1[i] & data2[i]) | ((!data1[i] | data2[i]) & bin);
    }
}

static inline void binSum2(int size, bool *data1, bool *data2, bool *res){
    bool cin = 0;
    for(int i = 0; i < size; i++){
        res[i] = data1[i] ^ data2[i] ^ cin;
        cin = (data1[i] & data2[i]) | (cin & data2[i]) | (data1[i] & cin);
    }
}

static inline void binSum4(int size, bool *data1, bool *data2, bool *data3, bool *data4, bool *res){
    bool temp0[128];
    bool temp1[128];
    binSum2(size,data1,data2,temp0);
    binSum2(size,data3,data4,temp1);
    binSum2(size,temp0,temp1,res);

    // if(size == 128){
    //     printHex(size, temp0,"temp0");
    //     printHex(size, temp1,"temp1");
    //     printHex(size, res,"res sum");

    // }
}

static inline void binMul(int size, bool *a, bool *b, bool *res){
    int64_t aDec,bDec,temp;
    binToInt(size, a, &aDec);
    binToInt(size, b, &bDec);
    // printf("adec = %ld\tbdec = %ld\n",aDec,bDec);
    temp = aDec * bDec;
    // printf("tempH = %lx\n",temp);
    // printf("tempD = %ld\n",temp);
    
    intToBin(size*2, abs(temp), res);
    //intToBin(size*2, temp, res);
    
    // printf("tempH = %lx\n",temp);
    // printf("tempH = %lx\n",abs(temp));
    // printf("tempD = %ld\n",temp);
    // printf("tempD = %ld\n",abs(temp));
    //temp = 0xf000000000000000;
    if(temp >= 0){
        printf("temp is pos\n");
        //twosComplement(size*2, res);
    }else{
        printf("temp is neg\n");
        twosComplement(size*2, res);
    }
    // printf("tempH = %lx\n",temp);
    // printf("tempD = %ld\n",temp);
    //printHex(size*2, res, "RES TEMP");
}

static inline void binMulSumUp(int size, bool *M0Bin, bool *M1Bin,bool *M2Bin, bool *M3Bin, bool *res, bool isSigned){
    bool M02x[128];
    bool M12x[128];
    bool M22x[128];
    bool M32x[128];
    
    bool temp0[128];
    bool temp1[128];
    bool temp2[128];
    bool temp3[128];

    extend2x(size, M0Bin, M02x, isSigned);
    extend2x(size, M1Bin, M12x, isSigned);
    extend2x(size, M2Bin, M22x, isSigned);
    extend2x(size, M3Bin, M32x, isSigned);
    
    if(size == 64){
        printHex(size*2, M02x,"M0extend");
        printHex(size*2, M12x,"M1extend");
        printHex(size*2, M22x,"M2extend");
        printHex(size*2, M32x,"M3extend");
    }
    binShift(size*2, 0     , M02x, temp0, false, isSigned);
    binShift(size*2, size/2, M12x, temp1, false, isSigned);
    binShift(size*2, size/2, M22x, temp2, false, isSigned);
    binShift(size*2, size  , M32x, temp3, false, isSigned);
        
    if(size == 64){
        printHex(size*2, temp0,"M0Shift");
        printHex(size*2, temp1,"M1Shift");
        printHex(size*2, temp2,"M2Shift");
        printHex(size*2, temp3,"M3Shift");
    }
    binSum4(size*2, temp0, temp1, temp2, temp3, res);
}

inline void VRegfile::reset(bool active){
    if (active){
        for (int i = 0; i < ISS_NB_VREGS; i++){
            for (int j = 0; j < NB_VEL; j++){
                this->vregs[i][j] = i == 0 ? 0 : 0x57575757;
            }
        }
    }
}

static inline void lib_ADDVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data1+data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ADDVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1+data2;


        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ADDVI    (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);

        res = data1+data2;


        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_SUBVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data2 - data1;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_SUBVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);

        res = data2 - data1;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_RSUBVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);

        res = data1 - data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_RSUBVI   (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);

        res = data1 - data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(iss->spatz.SEW == 16){
            printf("is mask = %d\n",mask(vm,bin));
            printf("res = %ld\n",res);
            printBin(iss->spatz.SEW,resBin,"Bin RES");
            //printf("vd org val0 = %d\tvd org val1 = %d\n",iss->spatz.vregfile.vregs[vd][i*2+0],iss->spatz.vregfile.vregs[vd][i*2+1]);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ANDVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data1 & data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ANDVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 & data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ANDVI    (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 & data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ORVV     (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data1 | data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ORVX     (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 | data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ORVI     (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 | data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_XORVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data1 ^ data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_XORVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 ^ data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_XORVI    (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 ^ data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = (data1 > data2)?data2:data1;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = (data1 > data2)?data2:data1;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINUVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbsU(iss, vs1, i, &data1);
        myAbsU(iss, vs2, i, &data2);

        res = (data1 > data2)?data2:data1;

        intToBin(iss->spatz.SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbsU(iss, vs2, i, &data2);
        
        res = (data1 > data2)?data2:data1;
        
        intToBin(iss->spatz.SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = (data1 > data2)?data1:data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = (data1 > data2)?data1:data2;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXUVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbsU(iss, vs1, i, &data1);
        myAbsU(iss, vs2, i, &data2);

        res = (data1 > data2)?data1:data2;

        intToBin(iss->spatz.SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbsU(iss, vs2, i, &data2);
        
        res = (data1 > data2)?data1:data2;
        
        intToBin(iss->spatz.SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MULVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/


    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);

        if(data1[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data1);
        }
        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);

        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);
        if(sgn){
            twosComplement(iss->spatz.SEW*2,resBin);
            sgn = 0;
        }

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MULVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }


        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(iss->spatz.SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MULHVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/

    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);

        if(data1[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data1);
        }
        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        // printHex(iss->spatz.SEW/2, data1H, "data1H");
        // printHex(iss->spatz.SEW/2, data1L, "data1L");
        // printHex(iss->spatz.SEW/2, data2H, "data2H");
        // printHex(iss->spatz.SEW/2, data2L, "data2L");
        
        // printHex(iss->spatz.SEW, M0, "M0");
        // printHex(iss->spatz.SEW, M1, "M1");
        // printHex(iss->spatz.SEW, M2, "M2");
        // printHex(iss->spatz.SEW, M3, "M3");

        // printHex(iss->spatz.SEW*2, resBin, "RESBin befor twos");
        // printHex(iss->spatz.SEW*2, resBin, "resBin before");

        if(sgn){
            twosComplement(iss->spatz.SEW*2,resBin);
            sgn = 0;
        }
        // printHex(iss->spatz.SEW*2, resBin, "resBin after");

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, &resBin[iss->spatz.SEW]);
        }
    }
}

static inline void lib_MULHVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }


        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(iss->spatz.SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, &resBin[iss->spatz.SEW]);
        }
    }
}

static inline void lib_MULHUVV  (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/

    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);

        // if(data1[iss->spatz.SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(iss->spatz.SEW,data1);
        // }
        // if(data2[iss->spatz.SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(iss->spatz.SEW,data2);
        // }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        // printHex(iss->spatz.SEW/2, data1H, "data1H");
        // printHex(iss->spatz.SEW/2, data1L, "data1L");
        // printHex(iss->spatz.SEW/2, data2H, "data2H");
        // printHex(iss->spatz.SEW/2, data2L, "data2L");
        
        // printHex(iss->spatz.SEW, M0, "M0");
        // printHex(iss->spatz.SEW, M1, "M1");
        // printHex(iss->spatz.SEW, M2, "M2");
        // printHex(iss->spatz.SEW, M3, "M3");

        // printHex(iss->spatz.SEW*2, resBin, "RESBin befor twos");
        // printHex(iss->spatz.SEW*2, resBin, "resBin before");

        // if(sgn){
        //     twosComplement(iss->spatz.SEW*2,resBin);
        //     sgn = 0;
        // }
        // printHex(iss->spatz.SEW*2, resBin, "resBin after");

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, &resBin[iss->spatz.SEW]);
        }
    }
}

static inline void lib_MULHUVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);

        // if(data2[iss->spatz.SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(iss->spatz.SEW,data2);
        // }


        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        // if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
        //     twosComplement(iss->spatz.SEW*2,resBin);
        // }
        // sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, &resBin[iss->spatz.SEW]);
        }
    }
}

static inline void lib_MULHSUVV (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/

    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);

        // if(data1[iss->spatz.SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(iss->spatz.SEW,data1);
        // }
        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        // printHex(iss->spatz.SEW/2, data1H, "data1H");
        // printHex(iss->spatz.SEW/2, data1L, "data1L");
        // printHex(iss->spatz.SEW/2, data2H, "data2H");
        // printHex(iss->spatz.SEW/2, data2L, "data2L");
        
        // printHex(iss->spatz.SEW, M0, "M0");
        // printHex(iss->spatz.SEW, M1, "M1");
        // printHex(iss->spatz.SEW, M2, "M2");
        // printHex(iss->spatz.SEW, M3, "M3");

        // printHex(iss->spatz.SEW*2, resBin, "RESBin befor twos");
        // printHex(iss->spatz.SEW*2, resBin, "resBin before");

        if(sgn){
            twosComplement(iss->spatz.SEW*2,resBin);
            sgn = 0;
        }
        // printHex(iss->spatz.SEW*2, resBin, "resBin after");

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, &resBin[iss->spatz.SEW]);
        }
    }
}

static inline void lib_MULHSUVX (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }


        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        if(sgn == 1){
            twosComplement(iss->spatz.SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, &resBin[iss->spatz.SEW]);
        }
    }
}

static inline void lib_MVVV     (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, res;
    bool bin[8];
    bool resBin[64];
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        
        res = data1;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MVVX     (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int64_t res;
    bool bin[8];
    bool resBin[64];
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        res = rs1;

        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MVVI     (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int64_t res;
    bool bin[8];
    bool resBin[64];
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        res = sim;
        intToBin(iss->spatz.SEW, abs(res), resBin);
        if(sim < 0){
            twosComplement(iss->spatz.SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_WMULVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/


    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);

        if(data1[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data1);
        }
        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);

        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);
        if(sgn){
            twosComplement(iss->spatz.SEW*2,resBin);
            sgn = 0;
        }

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMULVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }


        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(iss->spatz.SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMULUVV  (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/

    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMULUVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMULSUVV (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/

    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        if(sgn){
            twosComplement(iss->spatz.SEW*2,resBin);
            sgn = 0;
        }

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMULSUVX (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }


        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, resBin, false);

        if(sgn == 1){
            twosComplement(iss->spatz.SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_MACCVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/


    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);
        buildDataBin(iss, vd , i, data3);

        if(data1[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data1);
        }
        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }
        if(data3[iss->spatz.SEW-1]){
            twosComplement(iss->spatz.SEW,data3);
            vdSgn = !vdSgn;            
        }
        extend2x(iss->spatz.SEW, data3, data3Ext, true);

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);

        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, mulOutBin, false);
        if(sgn){
            twosComplement(iss->spatz.SEW*2,mulOutBin);
            sgn = 0;
        }

        if(!vdSgn){
            binSum2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }else{
            binSub2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MACCVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);
        buildDataBin(iss, vd , i, data3);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }
        if(data3[iss->spatz.SEW-1]){
            twosComplement(iss->spatz.SEW,data3);
            vdSgn = !vdSgn;            
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, mulOutBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(iss->spatz.SEW*2,mulOutBin);
        }
        sgn = 0;

        if(!vdSgn){
            binSum2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }else{
            binSub2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MADDVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/

    // int64_t temp = -8137711004328982294;
    // if (temp < 0){
    //     printf("temp is neg\n");
    // }else{
    //     printf("temp is pos\n");
    // }

    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vd , i, data2);
        buildDataBin(iss, vs2, i, data3);
        
        if(iss->spatz.SEW == 64){
            printHex(iss->spatz.SEW, data1, "vs1 before");
            printHex(iss->spatz.SEW, data2, "vd  before");
            printHex(iss->spatz.SEW, data3, "vs2 before");
        }

        if(data1[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data1);
        }
        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }
        if(data3[iss->spatz.SEW-1]){
            twosComplement(iss->spatz.SEW,data3);
            vdSgn = !vdSgn;            
        }
        extend2x(iss->spatz.SEW, data3, data3Ext, true);

        if(iss->spatz.SEW == 64 && i == 6){
            printHex(iss->spatz.SEW, data1, "vs1 before");
            printHex(iss->spatz.SEW, data2, "vd  before");
            printHex(iss->spatz.SEW, data3, "vs2 before");
            printHex(iss->spatz.SEW*2, data3Ext, "vs2 before ext");
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        if(iss->spatz.SEW == 64 && i == 6){
            printHex(iss->spatz.SEW/2, data1H, "data1H");
            printHex(iss->spatz.SEW/2, data1L, "data1L");
            printHex(iss->spatz.SEW/2, data2H, "data2H");
            printHex(iss->spatz.SEW/2, data2L, "data2L");
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);

        if(iss->spatz.SEW == 64 && i == 6){
            printHex(iss->spatz.SEW, M0, "M0");
            printHex(iss->spatz.SEW, M1, "M1");
            printHex(iss->spatz.SEW, M2, "M2");
            printHex(iss->spatz.SEW, M3, "M3");
        }

        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, mulOutBin, false);
        if(sgn){
            printf("mulOut twos\n");
            twosComplement(iss->spatz.SEW*2,mulOutBin);
            sgn = 0;
        }

        if(iss->spatz.SEW == 64 && i == 6){
            printHex(iss->spatz.SEW*2, mulOutBin, "mulOut");
        }


        if(!vdSgn){
            printf("SUM\n");
            binSum2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }else{
            printf("SUB\n");            
            binSub2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }
        vdSgn = 0;

        if(iss->spatz.SEW == 64 && i == 6){
            printHex(iss->spatz.SEW*2, resBin, "resBin");
        }

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MADDVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128];
    bool sgn = 0;
    bool vdSgn = 0;
    printf("rs1 = %ld\n",rs1);
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vd , i, data2);
        buildDataBin(iss, vs2 , i, data3);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }
        if(data3[iss->spatz.SEW-1]){
            twosComplement(iss->spatz.SEW,data3);
            vdSgn = !vdSgn;            
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, mulOutBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(iss->spatz.SEW*2,mulOutBin);
        }
        sgn = 0;

        if(!vdSgn){
            binSum2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }else{
            binSub2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_NMSACVV  (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/


    int t = sewCase(iss->spatz.SEW);
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs1, i, data1);
        buildDataBin(iss, vs2, i, data2);
        buildDataBin(iss, vd , i, data3);

        if(data1[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data1);
        }
        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }
        if(data3[iss->spatz.SEW-1]){
            twosComplement(iss->spatz.SEW,data3);
            vdSgn = !vdSgn;            
        }
        extend2x(iss->spatz.SEW, data3, data3Ext, true);

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);

        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, mulOutBin, false);
        if(!sgn){//compare with VMACC
            twosComplement(iss->spatz.SEW*2,mulOutBin);
            sgn = 0;
        }

        if(!vdSgn){
            binSum2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }else{
            binSub2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}

static inline void lib_NMSACVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(iss->spatz.SEW);

    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, vs2, i, data2);
        buildDataBin(iss, vd , i, data3);

        if(data2[iss->spatz.SEW-1]){
            sgn = !sgn;
            twosComplement(iss->spatz.SEW,data2);
        }
        if(data3[iss->spatz.SEW-1]){
            twosComplement(iss->spatz.SEW,data3);
            vdSgn = !vdSgn;            
        }

        for(int j = 0;j < iss->spatz.SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+iss->spatz.SEW/2];
            data2H[j] = data2[j+iss->spatz.SEW/2];
        }

        binMul(iss->spatz.SEW/2,data1L,data2L,M0);
        binMul(iss->spatz.SEW/2,data1L,data2H,M1);
        binMul(iss->spatz.SEW/2,data1H,data2L,M2);
        binMul(iss->spatz.SEW/2,data1H,data2H,M3);
        binMulSumUp(iss->spatz.SEW, M0, M1, M2, M3, mulOutBin, false);

        if(!(rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){//compare with VMACC
            twosComplement(iss->spatz.SEW*2,mulOutBin);
        }
        sgn = 0;

        if(!vdSgn){
            binSum2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }else{
            binSub2(iss->spatz.SEW*2, mulOutBin, data3, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, iss->spatz.SEW, vd, i, resBin);
        }
    }
}



























static inline void lib_VVFMAC(Iss *iss, int vs1, int vs2, int vd, bool vm){
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
        uint32_t addr_aligned = addr & ~(4 - 1);
        int size = addr_aligned + 4 - addr;
        // printf("size = %d\n" , size);
        // printf("io_pending_size = %d\n" , this->io_pending_size);        
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

        int err = this->io_itf[0].req(req);
        if (err == vp::IO_REQ_OK){
            // this->event->enqueue(this->io_req.get_latency() + 1);
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
static inline void lib_VLE8V (Iss *iss, iss_reg_t rs1, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    // printf("VLE8\n");
    // printf("vd = %d\n",vd);
    // printf("iss->spatz.vstart = %d\n",iss->spatz.vstart);
    // printf("iss->spatz.vl = %ld\n",iss->spatz.vl);
    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i+=4){
 //       if(!i){
 //           printf("Vlsu_io_access\n");
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
 //       }else{
 //           printf("handle_pending_io_access\n");
 //           iss->spatz.vlsu.handle_pending_io_access(iss);
 //       }

        // printf("data0 = %d\n",data[0]);
        // printf("data1 = %d\n",data[1]);
        // printf("data2 = %d\n",data[2]);
        // printf("data3 = %d\n",data[3]);

        iss->spatz.vregfile.vregs[vd][i+0] = data[0];
        iss->spatz.vregfile.vregs[vd][i+1] = data[1];
        iss->spatz.vregfile.vregs[vd][i+2] = data[2];
        iss->spatz.vregfile.vregs[vd][i+3] = data[3];

        start_add += 4;
    }
}

static inline void lib_VLE16V(Iss *iss, iss_reg_t rs1, int vd , bool vm){
    // printf("LIB_VLE16V\n");
    // printf("vstart = %d\n",iss->spatz.vstart);
    // printf("vl = %ld\n",iss->spatz.vl);
    // printf("VLEN/8 = %d\n",iss->spatz.VLEN/8);
    // printf("VM = %d\n",vm);
    // printf("RS1 = %lu\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl*2; i+=4){
        //if(!i){
            //iss->spatz.vlsu.Vlsu_io_access(iss, rs1, iss->spatz.VLEN/8, data, false);
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    iss->spatz.vlsu.handle_pending_io_access(iss);
        //}

        // printf("data0 = %d\n",data[0]);
        // printf("data1 = %d\n",data[1]);
        // printf("data2 = %d\n",data[2]);
        // printf("data3 = %d\n",data[3]);

        // printf("vd = %d\n",vd);

        //iss->spatz.vregfile.vregs[vd][i+0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? (data[1]*pow(2,8) + data[0]) : iss->spatz.vregfile.vregs[vd][i+0];
        //iss->spatz.vregfile.vregs[vd][i+1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? (data[3]*pow(2,8) + data[2]) : iss->spatz.vregfile.vregs[vd][i+1];

        // iss->spatz.vregfile.vregs[vd][i+0] = (data[1]*pow(2,8) + data[0]);
        // iss->spatz.vregfile.vregs[vd][i+1] = (data[3]*pow(2,8) + data[2]);

        iss->spatz.vregfile.vregs[vd][i+0] = data[0];
        iss->spatz.vregfile.vregs[vd][i+1] = data[1];
        iss->spatz.vregfile.vregs[vd][i+2] = data[2];
        iss->spatz.vregfile.vregs[vd][i+3] = data[3];

        // printf("vd_vali = %d\n",(int)(data[1]*pow(2,8) + data[0]));
        // printf("vd_vali+1 = %d\n",(int)(data[3]*pow(2,8) + data[2]));
        start_add += 4;
    }
}

static inline void lib_VLE32V(Iss *iss, iss_reg_t rs1, int vd , bool vm){
    // printf("LIB_VLE32V\n");
    // printf("vstart = %d\n",iss->spatz.vstart);
    // printf("vl = %ld\n",iss->spatz.vl);
    // printf("VLEN/8 = %d\n",iss->spatz.VLEN/8);
    // printf("VM = %d\n",vm);
    // printf("RS1 = %lu\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl*4; i+=4){
        //if(!i){
            //iss->spatz.vlsu.Vlsu_io_access(iss, rs1, iss->spatz.VLEN/8, data, false);
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    iss->spatz.vlsu.handle_pending_io_access(iss);
        //}

        // printf("data0 = %d\n",data[0]);
        // printf("data1 = %d\n",data[1]);
        // printf("data2 = %d\n",data[2]);
        // printf("data3 = %d\n",data[3]);

        //printf("vd = %d\n",vd);

        //iss->spatz.vregfile.vregs[vd][i+0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? (data[1]*pow(2,8) + data[0]) : iss->spatz.vregfile.vregs[vd][i+0];
        //iss->spatz.vregfile.vregs[vd][i+1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? (data[3]*pow(2,8) + data[2]) : iss->spatz.vregfile.vregs[vd][i+1];

        // iss->spatz.vregfile.vregs[vd][i+0] = (data[1]*pow(2,8) + data[0]);
        // iss->spatz.vregfile.vregs[vd][i+1] = (data[3]*pow(2,8) + data[2]);

        iss->spatz.vregfile.vregs[vd][i+0] = data[0];
        iss->spatz.vregfile.vregs[vd][i+1] = data[1];
        iss->spatz.vregfile.vregs[vd][i+2] = data[2];
        iss->spatz.vregfile.vregs[vd][i+3] = data[3];

        // printf("vd0 = %d\n",iss->spatz.vregfile.vregs[vd][i+0]);
        // printf("vd1 = %d\n",iss->spatz.vregfile.vregs[vd][i+1]);
        // printf("vd2 = %d\n",iss->spatz.vregfile.vregs[vd][i+2]);
        // printf("vd3 = %d\n",iss->spatz.vregfile.vregs[vd][i+3]);


        // printf("vd_vali = %d\n",(int)(data[3]*pow(2,8*3) + data[2]*pow(2,8*2) + data[1]*pow(2,8) + data[0]));
        start_add += 4;
    }
}

static inline void lib_VLE64V(Iss *iss, iss_reg_t rs1, int vd , bool vm){
    // printf("LIB_VLE64V\n");
    // printf("vstart = %d\n",iss->spatz.vstart);
    // printf("vl = %ld\n",iss->spatz.vl);
    // printf("VLEN/8 = %d\n",iss->spatz.VLEN/8);
    // printf("VM = %d\n",vm);
    // printf("RS1 = %lu\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];

    for (int i = iss->spatz.vstart; i < iss->spatz.vl*8; i+=4){
        //if(!i){
            //iss->spatz.vlsu.Vlsu_io_access(iss, rs1, iss->spatz.VLEN/8, data, false);
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    iss->spatz.vlsu.handle_pending_io_access(iss);
        //}

        // printf("data0 = %d\n",data[0]);
        // printf("data1 = %d\n",data[1]);
        // printf("data2 = %d\n",data[2]);
        // printf("data3 = %d\n",data[3]);

        //printf("vd = %d\n",vd);

        //iss->spatz.vregfile.vregs[vd][i+0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? (data[1]*pow(2,8) + data[0]) : iss->spatz.vregfile.vregs[vd][i+0];
        //iss->spatz.vregfile.vregs[vd][i+1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? (data[3]*pow(2,8) + data[2]) : iss->spatz.vregfile.vregs[vd][i+1];

        // iss->spatz.vregfile.vregs[vd][i+0] = (data[1]*pow(2,8) + data[0]);
        // iss->spatz.vregfile.vregs[vd][i+1] = (data[3]*pow(2,8) + data[2]);

        iss->spatz.vregfile.vregs[vd][i+0] = data[0];
        iss->spatz.vregfile.vregs[vd][i+1] = data[1];
        iss->spatz.vregfile.vregs[vd][i+2] = data[2];
        iss->spatz.vregfile.vregs[vd][i+3] = data[3];

        // printf("vd0 = %d\n",iss->spatz.vregfile.vregs[vd][i+0]);
        // printf("vd1 = %d\n",iss->spatz.vregfile.vregs[vd][i+1]);
        // printf("vd2 = %d\n",iss->spatz.vregfile.vregs[vd][i+2]);
        // printf("vd3 = %d\n",iss->spatz.vregfile.vregs[vd][i+3]);


        // printf("vd_vali = %d\n",(int)(data[3]*pow(2,8*3) + data[2]*pow(2,8*2) + data[1]*pow(2,8) + data[0]));
        start_add += 4;
    }
}

static inline void lib_VSE8V (Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);

    uint64_t start_add = rs1;

    for (int i = iss->spatz.vstart; i < iss->spatz.vl; i+=4){
/*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+2]%2)) ? iss->spatz.vregfile.vregs[vs3][i+2] : 0;
        data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+3]%2)) ? iss->spatz.vregfile.vregs[vs3][i+3] : 0;
*/
        // data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        // data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        // data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+2]%2)) ? iss->spatz.vregfile.vregs[vs3][i+2] : 0;
        // data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+3]%2)) ? iss->spatz.vregfile.vregs[vs3][i+3] : 0;

        data[0]  = iss->spatz.vregfile.vregs[vs3][i+0];
        data[1]  = iss->spatz.vregfile.vregs[vs3][i+1];
        data[2]  = iss->spatz.vregfile.vregs[vs3][i+2];
        data[3]  = iss->spatz.vregfile.vregs[vs3][i+3];


        // printf("STORE8 \n");
        // printf("data0  = %d\n",data[0 ]);
        // printf("data1  = %d\n",data[1 ]);
        // printf("data2  = %d\n",data[2 ]);
        // printf("data3  = %d\n",data[3 ]);

        // printf("addr  = %lu\n",start_add);

        //if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
        //}else{
        //    iss->spatz.vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

static inline void lib_VSE16V(Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);

    uint64_t start_add = (uint64_t)rs1;    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl*2; i+=4){
        /*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;

        // if(!i){
        //     iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,true);
        // }else{
        //     iss->spatz.vlsu.handle_pending_io_access(iss);
        // }

        data[0]  = iss->spatz.vregfile.vregs[vs3][i+0];
        data[1]  = iss->spatz.vregfile.vregs[vs3][i+1];
        data[2]  = iss->spatz.vregfile.vregs[vs3][i+2];
        data[3]  = iss->spatz.vregfile.vregs[vs3][i+3];
        // printf("i = %d\t,vd = %d\n",i,vs3);

        // printf("STORE16 \n");
        // printf("data0  = %d\n",data[0]);
        // printf("data1  = %d\n",data[1]);
        // printf("data2  = %d\n",data[2]);
        // printf("data3  = %d\n",data[3]);

        // printf("addr  = %lu\n",start_add);

        //if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
        //}else{
        //    iss->spatz.vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

static inline void lib_VSE32V(Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);

    uint64_t start_add = rs1;    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl*4; i+=4){
        /*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;

        // if(!i){
        //     iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,true);
        // }else{
        //     iss->spatz.vlsu.handle_pending_io_access(iss);
        // }

        data[0]  = iss->spatz.vregfile.vregs[vs3][i+0];
        data[1]  = iss->spatz.vregfile.vregs[vs3][i+1];
        data[2]  = iss->spatz.vregfile.vregs[vs3][i+2];
        data[3]  = iss->spatz.vregfile.vregs[vs3][i+3];


        // printf("STORE32 \n");
        // printf("data0  = %d\n",data[0]);
        // printf("data1  = %d\n",data[1]);
        // printf("data2  = %d\n",data[2]);
        // printf("data3  = %d\n",data[3]);

        // printf("addr  = %lu\n",start_add);

        //if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
        //}else{
        //    iss->spatz.vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

static inline void lib_VSE64V(Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    // uint8_t data[iss->spatz.vl];
    // uint32_t temp;
    // for (int i = iss->spatz.vstart; i < iss->spatz.vl*2; i+=1){
    //     if(i%2){
    //         temp = iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8*4);
    //         data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*3) : 0;
    //         data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*2) : 0;
    //         data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*1) : 0;
    //         data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*0) : 0;
    //     }else{
    //         temp = iss->spatz.vregfile.vregs[vs3][i+0];
    //         data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*3) : 0;
    //         data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? temp/pow(2,8*2) : 0;
    //         data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*1) : 0;
    //         data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? temp/pow(2,8*0) : 0;
    //     }
    //     if(!i){
    //         iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,true);
    //     }else{
    //         iss->spatz.vlsu.handle_pending_io_access(iss);
    //     }
    // }
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);

    uint64_t start_add = rs1;    
    for (int i = iss->spatz.vstart; i < iss->spatz.vl*8; i+=4){
        /*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;

        // if(!i){
        //     iss->spatz.vlsu.Vlsu_io_access(iss, rs1,iss->spatz.VLEN/8,data,true);
        // }else{
        //     iss->spatz.vlsu.handle_pending_io_access(iss);
        // }

        data[0]  = iss->spatz.vregfile.vregs[vs3][i+0];
        data[1]  = iss->spatz.vregfile.vregs[vs3][i+1];
        data[2]  = iss->spatz.vregfile.vregs[vs3][i+2];
        data[3]  = iss->spatz.vregfile.vregs[vs3][i+3];


        // printf("STORE64 \n");
        // printf("data0  = %d\n",data[0]);
        // printf("data1  = %d\n",data[1]);
        // printf("data2  = %d\n",data[2]);
        // printf("data3  = %d\n",data[3]);

        // printf("addr  = %lu\n",start_add);

        //if(!i){
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
        //}else{
        //    iss->spatz.vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR CONFIGURATION SETTING
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//static inline iss_reg_t lib_VSETVLI(Iss *iss, int idxRs1, int idxRd, int rs1, iss_uim_t lmul, iss_uim_t sew, bool ta,  bool ma, iss_uim_t vtype){
static inline iss_reg_t lib_VSETVLI(Iss *iss, int idxRs1, int idxRd, int rs1, iss_uim_t lmul, iss_uim_t sew, iss_uim_t vtype){
    uint32_t AVL;

    // SET NEW VTYPE
    // spatz_req.vtype = {1'b0, decoder_req_i.instr[27:20]};

    iss->spatz.vtype = vtype;

    //implement it like what is implemented in SV
    //if(iss->spatz.VLEN*iss->spatz.LMUL_VALUES[lmul]/iss->spatz.SEW_VALUES[sew] == VLMAX){
        iss->spatz.vstart = 0;
        iss->spatz.SEW = iss->spatz.SEW_VALUES[sew];
        iss->spatz.LMUL = iss->spatz.LMUL_VALUES[lmul];
        //iss->spatz.VMA = ma;
        //iss->spatz.VTA = ta;

        //  localparam int unsigned MAXVL  = VLEN; and VLEN = 256
        if(idxRs1){
            AVL = rs1;
            iss->spatz.vl = MIN(AVL,VLMAX);//spec page 30 - part c and d
            //printf("CASE1\n");
        //}else if(VLMAX < rs1){
        }else if(!idxRs1 && idxRd){
            AVL = UINT32_MAX;
            iss->spatz.vl  = VLMAX;
            //printf("CASE2\n");
            //vl = VLEN/SEW;
        }else{
            AVL = iss->spatz.vl;
            //printf("CASE3\n");
        }
    //} else {
        // vtype for invalid situation
        //vtype_d = '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0};
        //vl_d    = '0;
    //}
    // printf("SEW = %d \n",iss->spatz.SEW);
    // printf("LMUL = %f \n",iss->spatz.LMUL);
    // printf("VL = %ld \n",iss->spatz.vl);
    // printf("AVL = %d \n",AVL);
    // printf("rs1 = %d \n",rs1);
    return iss->spatz.vl;
    //Not sure about the write back procedure
}
// static inline void lib_VSETVL(Iss *iss, int idxrs, int idxrd, int rs1, int rs2){
//     printf("inside the function\n");
//     printf("idxrs = %d\n",idxrs);
//     printf("idxrd = %d\n",idxrd);
//     printf("valrs1 = %d\n",rs1);
//     printf("valrs2 = %d\n",rs2);
// }


static inline iss_reg_t lib_VSETVL(Iss *iss, int idxRs1, int idxRd, int rs1, int rs2){
//static inline void lib_VSETVL(Iss *iss, int idxRs1, int idxRd, int rs1, int rs2){
    //printf("vsetvl\n");
    uint32_t AVL;

    // SET NEW VTYPE
    // spatz_req.vtype = {1'b0, decoder_req_i.instr[27:20]};

    iss->spatz.vtype = rs2;
    int sew = (rs2/4)%8;
    int lmul = rs2%4;
    //implement it like what is implemented in SV
    //if(iss->spatz.VLEN*iss->spatz.LMUL_VALUES[lmul]/iss->spatz.SEW_VALUES[sew] == VLMAX){
        iss->spatz.vstart = 0;
        iss->spatz.SEW = iss->spatz.SEW_VALUES[sew];
        iss->spatz.LMUL = iss->spatz.LMUL_VALUES[lmul];
        //iss->spatz.VMA = ma;
        //iss->spatz.VTA = ta;

        //  localparam int unsigned MAXVL  = VLEN; and VLEN = 256
        if(idxRs1){
            AVL = rs1;
            iss->spatz.vl = MIN(AVL,VLMAX);//spec page 30 - part c and d
            //printf("CASE1\n");
        //}else if(VLMAX < rs1){
        }else if(!idxRs1 && idxRd){
            AVL = UINT32_MAX;
            iss->spatz.vl  = VLMAX;
            //printf("CASE2\n");
            //vl = VLEN/SEW;
        }else{
            AVL = iss->spatz.vl;
            //printf("CASE3\n");
        }
    //} else {
        // vtype for invalid situation
        //vtype_d = '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0};
        //vl_d    = '0;
    //}
    // printf("SEW = %d \n",iss->spatz.SEW);
    // printf("LMUL = %f \n",iss->spatz.LMUL);
    // printf("VL = %ld \n",iss->spatz.vl);
    // printf("AVL = %d \n",AVL);
    // printf("rs1 = %d \n",rs1);
    return iss->spatz.vl;
    //Not sure about the write back procedure
}


#endif