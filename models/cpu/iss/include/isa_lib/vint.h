#ifndef VINT_H
#define VINT_H

#include "spatz.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR REGISTER FILE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "flexfloat.h"
#include "int.h"
#include <stdint.h>
#include <math.h>
#include <fenv.h>
#include <cores/ri5cy/class.hpp>
#include "assert.h"


#pragma STDC FENV_ACCESS ON


#define FLOAT_INIT_2(a, b, e, m)                        \
    flexfloat_t ff_a, ff_b, ff_res;                  \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_b, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);                    \
    flexfloat_set_bits(&ff_b, b);

#define FLOAT_INIT_3(a, b, c, e, m)                        \
    flexfloat_t ff_a, ff_b, ff_c, ff_res;                  \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_b, env);                             \
    ff_init(&ff_c, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);                    \
    flexfloat_set_bits(&ff_b, b);                    \
    flexfloat_set_bits(&ff_c, c);



//clear_fflags(s, iss->csr.fcsr.fflags);
#define FLOAT_EXEC_2(iss, name, a, b, e, m ,res) \
    FLOAT_INIT_2(a, b, e, m)                \
    feclearexcept(FE_ALL_EXCEPT);              \
    name(&ff_res, &ff_a, &ff_b);               \
    update_fflags_fenv(iss);                     \
    res = flexfloat_get_bits(&ff_res);


#define FLOAT_EXEC_3(iss, name, a, b, c, e, m ,res) \
    FLOAT_INIT_3(a, b, c, e, m)                   \
    feclearexcept(FE_ALL_EXCEPT);                 \
    name(iss ,&ff_res, &ff_a, &ff_b, &ff_c);           \
    update_fflags_fenv(iss);                        \
    res = flexfloat_get_bits(&ff_res);



#define mask(vm,bin) (!(vm) && !bin[i%8])

#define SEW iss->spatz.SEW_t
#define LMUL iss->spatz.LMUL_t
#define VL iss->csr.vl.value
#define VSTART iss->csr.vstart.value


// #define FLOAT_FUNC(name)\
//     static inline unsigned long int FLOAT_EXEC_2(Iss s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m){
//         FLOAT_INIT_2(a, b, e, m)
//         feclearexcept(FE_ALL_EXCEPT);
//         name(&ff_res, &ff_a, &ff_b);
//      // name(&ff_res, &ff_a, &ff_b);
//         update_fflags_fenv(s);
//         return flexfloat_get_bits(&ff_res);
//     }

// static inline unsigned long int lib_flexfloat_add_round(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m, unsigned long int round)
// {
//     int old = setFFRoundingMode(s, round);
//     unsigned long int result = lib_flexfloat_add(s, a, b, e, m);
//     restoreFFRoundingMode(old);
//     return result;
// }
// static inline unsigned long int lib_flexfloat_add(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
// {
//     FF_EXEC_2(s, ff_add, a, b, e, m)
// }




static inline void printBin(int size, bool *a,const char name[]){
    printf("%s = ",name);
    for(int i = size-1; i >= 0  ; i--){
        printf("%d",a[i]);
    }
    printf("\n");
}

static inline void printHex(int size, bool *a,const char name[]){
    printf("%s = ",name);
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

static inline int sewCase(int sew){ //sew/8
    int iteration = 0;
    switch (sew){
    case(8 ) :iteration = 1 ;break;
    case(16) :iteration = 2 ;break;
    case(32) :iteration = 4 ;break;
    case(64) :iteration = 8 ;break;
    case(128):iteration = 16;break;
    default:printf("This SEW(%d) is not supported\n",sew);return -1;
        break;
    }
    return iteration;
}

static inline void EMCase(int sew, uint8_t *m, uint8_t *e){
    switch (sew){
    case 8:{
        printf("8 bit is not supported for FLOAT\n");
        return;
        break;
    }
    case 16:{
        *e = 5;
        *m = 10;
        break;
    }
    case 32:{
        *e = 8;
        *m = 23;
        break;
    }
    case 64:{
        *e = 11;
        *m = 52;
        break;
    }                
    default:
        break;
    }
}

static inline void reverseBin(int size, bool* dataIn, bool* res){
    for (int i = 0; i < size; i++)
    {
        res[size-1-i] = dataIn[i];
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

static inline void intToBin(int size, int64_t dataIn, bool* res){
    int64_t temp = dataIn;
    for (int j = 0; j < size; j++) {
        if (!(temp%2)) {
            res[j] = 0;
        } else {
            res[j] = 1;
        }
        temp = temp/2;
    }
}

static inline void intToBinU(int size, uint64_t dataIn, bool* res){
    uint64_t temp = dataIn;
    for (int j = 0; j < size; j++) {
        if (!(temp%2)) {
            res[j] = 0;
        } else {
            res[j] = 1;
        }
        temp = temp/2;
        // printf("res[%d] = %d\n",j,res[j]);        
        // printf("temp = %lu\n",temp);
    }
}

static inline void doubleToBin(int size, double dataIn, bool* res, int *point){
    bool temp[size];
    int dec = floor(abs(dataIn));
    printf("dec = %d\n",dec);
    double frac = abs(dataIn) - dec;
    printf("frac = %f\n",frac);
    int i = 1;
    int dec_size = (dec == 0)?1:int(ceil(log(dec))+1);

    temp[0] = 0;
    while(dec > 0){
        if(!(dec%2)) {
            temp[i] = 0;
        } else {
            temp[i] = 1;
        }
        dec /= 2;
        i++;
    }
    while(i < size){
        frac *= 2;
        if(frac < 1) {
            temp[i] = 0;
        } else {
            frac -= 1;
            temp[i] = 1;
        }
        i++;
    }
    reverseBin(size, temp, res);
    if(dataIn < 0){
        twosComplement(size,res);
    }
    *point = dec_size;
}

static inline void binToDouble(int size, bool* a, int point, double *res){
    printHex(size,a,"double to bin input");
    *res = 0;
    bool flag = 0;
    if(a[size-1]){
        printf("double to bin twos \n");
        twosComplement(size,a);
        flag = 1;
    }
    int i = 0;
    while(i < point){
        *res += a[size-point+i] * pow(2,i);
        i++;
        // printf("dec : i = %d\tres = %f\n",i,*res);
    }
    while(i < size){
        *res += a[size-1-i] * pow(2,(point-i-1));
        i++;
        // printf("frac : i = %d\tres = %f\n",i,*res);        
    }
    printf("RES = %f\n",*res);
    if(flag){
        *res = -(*res);
    }    
}

static inline void buildDataInt(Iss *iss, int vs, int i, int64_t* data){
    iss_Vel_t temp[8];
    int iteration = sewCase(SEW);
    for(int j = 0;j < iteration;j++){
        temp[j] = iss->spatz.vregfile.vregs[vs][i*iteration+j];
        // printf("vs%d[%d] = %d\n",vs,i*iteration+j,iss->spatz.vregfile.vregs[vs][i*iteration+j]);
    }
    *data = 0;
    for(int j = 0;j < iteration;j++){
        *data += temp[j]*pow(2,8*j);
    }
}

static inline void buildDataBin(Iss *iss, int size, int vs, int i, bool* dataBin){
    int iteration = sewCase(size);
    for(int j = 0;j < iteration;j++){
        intToBinU(8,(uint64_t)abs(iss->spatz.vregfile.vregs[vs][i*iteration+j]), &dataBin[j*8]);
        // printf("vs%d[%d] = %d\n",vs,i*iteration+j,iss->spatz.vregfile.vregs[vs][i*iteration+j]);
        //printf("vs%d[%d] = %lu\n",vs,i*iteration+j,(uint64_t)abs(iss->spatz.vregfile.vregs[vs][i*iteration+j]));

    }
}

static inline void myAbs(Iss *iss, int vs, int i, int64_t* data){
    iss_Vel_t temp[8];
    int cin = 0;
    int iteration = sewCase(SEW);
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

static inline void myAbsU(Iss *iss,int size, int vs, int i, uint64_t* data){
    iss_Vel_t temp[8];
    int iteration = sewCase(size);

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
    // printf("i = %d\t,vd = %d\n",i,vd);
    for(int j = 0; j < iteration; j++){
        // printf("bin2char[%d] = %x\n",j,bin8ToChar(bin,8*j,8*(j+1)));
        iss->spatz.vregfile.vregs[vd][i*iteration+j] = bin8ToChar(bin,8*j,8*(j+1));        
    }
}

static inline void binToInt(int size, bool* dataIn, int64_t *res){
    *res = 0;
    for (int j = 0; j < size; j++) {
        *res += dataIn[j]*(int64_t)pow(2,j);
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

    // printHex(size, temp0,"temp0");
    // printHex(size, temp1,"temp1");
    // printHex(size, res,"res sum");
}

static inline void binMul(int size, bool *a, bool *b, bool *res){
    int64_t aDec,bDec;
    uint64_t temp;
    binToInt(size, a, &aDec);
    binToInt(size, b, &bDec);
    // printf("adec = %ld\tbdec = %ld\n",aDec,bDec);
    temp = aDec * bDec;
    // printf("tempH = %lx\n",temp);
    // printf("tempD = %lu\n",temp);
    
    //intToBin(size*2, abs(temp), res);
    //intToBin(size*2, abs(temp), res);
    intToBinU(size*2, temp, res);
    
    // printf("tempH = %lx\n",temp);
    // printf("tempH = %lx\n",abs(temp));
    // printf("tempD = %ld\n",temp);
    // printf("tempD = %ld\n",abs(temp));
    // if(temp >= 0){
    //     //printf("temp is pos\n");
    //     //twosComplement(size*2, res);
    // }else{
    //     //printf("temp is neg\n");
    //     twosComplement(size*2, res);
    // }
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
    
    // if(size == 16){
    //     printHex(size*2, M02x,"M0extend");
    //     printHex(size*2, M12x,"M1extend");
    //     printHex(size*2, M22x,"M2extend");
    //     printHex(size*2, M32x,"M3extend");
    // }
    binShift(size*2, 0     , M02x, temp0, false, isSigned);
    binShift(size*2, size/2, M12x, temp1, false, isSigned);
    binShift(size*2, size/2, M22x, temp2, false, isSigned);
    binShift(size*2, size  , M32x, temp3, false, isSigned);
        
    // if(size == 16){
    //     printHex(size*2, temp0,"M0Shift");
    //     printHex(size*2, temp1,"M1Shift");
    //     printHex(size*2, temp2,"M2Shift");
    //     printHex(size*2, temp3,"M3Shift");
    // }
    binSum4(size*2, temp0, temp1, temp2, temp3, res);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                      FLOATING POINT FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// check the iss

INLINE void ff_macc(Iss *iss, flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    // assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) && (dest->desc.frac_bits == c->desc.frac_bits) &&
    //       (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) && (a->desc.frac_bits == c->desc.frac_bits));
    
    
    dest->value = (a->value * b->value) + c->value;
    printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);

    // int aPoint,bPoint,cPoint,mulOutPoint,resPoint;
    // bool aBool[64],bBool[64],cBool[64];

    // doubleToBin(SEW, abs(a->value), aBool, &aPoint);
    // doubleToBin(SEW, abs(b->value), bBool, &bPoint);
    // doubleToBin(SEW, abs(c->value), cBool, &cPoint);

    // printHex(SEW,aBool,"aBin");
    // printHex(SEW,bBool,"bBin");
    // printHex(SEW,cBool,"cBin");
    // printf("aPoint = %d\tbPoint = %d\tcPoint = %d\n",aPoint,bPoint,cPoint);



    // bool aH[32], aL[32], bH[32], bL[32];
    // bool M0[64], M1[64], M2[64], M3[64];
    // bool resBin[128], mulOutBin[128], cExt[128], cExtShift[128], mulOutShift[128];

    // for (int j = 0; j < SEW*2; j++){
    //     if(j < SEW){
    //         cExt[j] = 0;
    //     }else{
    //         cExt[j] = cBool[j - SEW];
    //     }
    // }
    
    // double atemp, btemp, ctemp, mtemp;

    // binToDouble(SEW,aBool,aPoint,&atemp);
    // binToDouble(SEW,bBool,bPoint,&btemp);
    // binToDouble(SEW,cBool,cPoint,&ctemp);
    // printf("a* = %f\t,b* = %f\t,c* = %f\n",atemp,btemp,ctemp);



    // for(int j = 0;j < SEW/2;j++){
    //     aL[j] = aBool[j];
    //     bL[j] = bBool[j];
    //     aH[j] = aBool[j+SEW/2];
    //     bH[j] = bBool[j+SEW/2];
    // }
    // printHex(SEW/2,aH,"aH");
    // printHex(SEW/2,aL,"aL");
    // printHex(SEW/2,bH,"bH");
    // printHex(SEW/2,bL,"bL");

    // binMul(SEW/2,aL,bL,M0);
    // binMul(SEW/2,aL,bH,M1);
    // binMul(SEW/2,aH,bL,M2);
    // binMul(SEW/2,aH,bH,M3);

    // printHex(SEW,M0,"M0");
    // printHex(SEW,M1,"M1");
    // printHex(SEW,M2,"M2");
    // printHex(SEW,M3,"M3");


    // binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

    // printHex(SEW*2,mulOutBin,"mulOutBin");
    
    // // if(aPoint == 1 || bPoint == 1){
    // //     mulOutPoint = aPoint + bPoint - 1;
    // // }else{
    // //     mulOutPoint = aPoint + bPoint;
    // // }
    
    // mulOutPoint = aPoint + bPoint;
    // printf("mulOutPoint = %d \n",mulOutPoint);
    // binToDouble(SEW*2,mulOutBin,mulOutPoint,&mtemp);
    // printf("mul* = %f\n",mtemp);



    // if((a->value < 0) ^ (b->value < 0)){
    //     printf("MUL NEG\n");
    //     twosComplement(SEW*2,mulOutBin);
    // }
    // printf("mulOutPoint = %d\tcpoint = %d\n",mulOutPoint,cPoint);
    // if(mulOutPoint > cPoint){
    //     binShift(SEW*2, abs(mulOutPoint-cPoint), cExt, cExtShift, true, true);
    //     for(int j = 0; j < SEW*2; j++){
    //         mulOutShift[j] = mulOutBin[j];
    //     }
    //     resPoint = mulOutPoint;
    // }else if(mulOutPoint < cPoint){
    //     binShift(SEW*2, abs(cPoint-mulOutPoint), mulOutBin, mulOutShift, true, true);
    //     for(int j = 0; j < SEW*2; j++){
    //         cExtShift[j] = cExt[j];
    //     }
    //     resPoint = cPoint;        
    // } else{
    //     for(int j = 0; j < SEW*2; j++){
    //         cExtShift[j] = cExt[j];
    //         mulOutShift[j] = mulOutBin[j];
    //     }
    //     resPoint = cPoint;        
    // }

    // printf("resPoint = %d\n",resPoint);
    // printHex(SEW*2,cExtShift,"cExtShift");
    // printHex(SEW*2,mulOutShift,"mulOutShift");


    // if(c->value > 0){
    //     printf("MACC\n");
    //     binSum2(SEW*2, mulOutShift, cExtShift, resBin);
    // }else{
    //     printf("MSUB\n");
    //     binSub2(SEW*2, mulOutShift, cExtShift, resBin);
    // }

    // binToDouble(SEW*2,resBin,resPoint,&dest->value);






    
    // printf("a = %f\tb = %f\tc = %f\n",a->value,b->value,c->value);
    // printf("dest = %f\n",dest->value);

    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_nmacc(Iss *iss, flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) && (dest->desc.frac_bits == c->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) && (a->desc.frac_bits == c->desc.frac_bits));
    
    
    dest->value = -(a->value * b->value) - c->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_msac(Iss *iss, flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) && (dest->desc.frac_bits == c->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) && (a->desc.frac_bits == c->desc.frac_bits));
    
    
    dest->value = (a->value * b->value) - c->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_nmsac(Iss *iss, flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) && (dest->desc.frac_bits == c->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) && (a->desc.frac_bits == c->desc.frac_bits));
    
    
    dest->value = -(a->value * b->value) + c->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_madd(Iss *iss, flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) && (dest->desc.frac_bits == c->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) && (a->desc.frac_bits == c->desc.frac_bits));
    
    
    dest->value = (a->value * c->value) + b->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_nmadd(Iss *iss, flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) && (dest->desc.frac_bits == c->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) && (a->desc.frac_bits == c->desc.frac_bits));
    
    
    dest->value = -(a->value * c->value) - b->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_msub(Iss *iss, flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) && (dest->desc.frac_bits == c->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) && (a->desc.frac_bits == c->desc.frac_bits));
    
    
    dest->value = (a->value * c->value) - b->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_nmsub(Iss *iss, flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) && (dest->desc.frac_bits == c->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) && (a->desc.frac_bits == c->desc.frac_bits));
    
    
    dest->value = -(a->value * c->value) + b->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





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
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data1+data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ADDVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1+data2;


        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ADDVI    (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);

        res = data1+data2;


        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_SUBVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data2 - data1;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_SUBVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);

        res = data2 - data1;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_RSUBVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);

        res = data1 - data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_RSUBVI   (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);

        res = data1 - data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(SEW == 16){
            printf("is mask = %d\n",mask(vm,bin));
            printf("res = %ld\n",res);
            printBin(SEW,resBin,"Bin RES");
            //printf("vd org val0 = %d\tvd org val1 = %d\n",iss->spatz.vregfile.vregs[vd][i*2+0],iss->spatz.vregfile.vregs[vd][i*2+1]);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ANDVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data1 & data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ANDVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 & data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ANDVI    (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 & data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ORVV     (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data1 | data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ORVX     (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 | data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_ORVI     (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 | data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_XORVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = data1 ^ data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_XORVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 ^ data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_XORVI    (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = sim;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data1 ^ data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = (data1 > data2)?data2:data1;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = (data1 > data2)?data2:data1;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINUVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);

        res = (data1 > data2)?data2:data1;

        intToBin(SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        
        res = (data1 > data2)?data2:data1;
        
        intToBin(SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        res = (data1 > data2)?data1:data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = (data1 > data2)?data1:data2;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXUVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);

        res = (data1 > data2)?data1:data2;

        intToBin(SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        
        res = (data1 > data2)?data1:data2;
        
        intToBin(SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
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


    int t = sewCase(SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        // printHex(8,bin,"mask bin");

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        // printf("vs1 = %d\t,vs2 = %d\n",vs1,vs2);
        // printHex(SEW,data1,"data1Hex");
        // printHex(SEW,data2,"data2Hex");

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        // printHex(SEW/2, data1H, "data1H");
        // printHex(SEW/2, data1L, "data1L");
        // printHex(SEW/2, data2H, "data2H");
        // printHex(SEW/2, data2L, "data2L");
        
        // printHex(SEW, M0, "M0");
        // printHex(SEW, M1, "M1");
        // printHex(SEW, M2, "M2");
        // printHex(SEW, M3, "M3");

        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);
        if(sgn){
            twosComplement(SEW*2,resBin);
            sgn = 0;
        }
        //printHex(SEW*2, resBin, "resBin");
        if(!mask(vm,bin)){
            //printf("mask\n");
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MULVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }


        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
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

    int t = sewCase(SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        // printHex(SEW/2, data1H, "data1H");
        // printHex(SEW/2, data1L, "data1L");
        // printHex(SEW/2, data2H, "data2H");
        // printHex(SEW/2, data2L, "data2L");
        
        // printHex(SEW, M0, "M0");
        // printHex(SEW, M1, "M1");
        // printHex(SEW, M2, "M2");
        // printHex(SEW, M3, "M3");

        // printHex(SEW*2, resBin, "RESBin befor twos");
        // printHex(SEW*2, resBin, "resBin before");

        if(sgn){
            twosComplement(SEW*2,resBin);
            sgn = 0;
        }
        // printHex(SEW*2, resBin, "resBin after");

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
        }
    }
}

static inline void lib_MULHVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }


        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
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

    int t = sewCase(SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    // bool sgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        // if(data1[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data1);
        // }
        // if(data2[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data2);
        // }

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        // printHex(SEW/2, data1H, "data1H");
        // printHex(SEW/2, data1L, "data1L");
        // printHex(SEW/2, data2H, "data2H");
        // printHex(SEW/2, data2L, "data2L");
        
        // printHex(SEW, M0, "M0");
        // printHex(SEW, M1, "M1");
        // printHex(SEW, M2, "M2");
        // printHex(SEW, M3, "M3");

        // printHex(SEW*2, resBin, "RESBin befor twos");
        // printHex(SEW*2, resBin, "resBin before");

        // if(sgn){
        //     twosComplement(SEW*2,resBin);
        //     sgn = 0;
        // }
        // printHex(SEW*2, resBin, "resBin after");

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
        }
    }
}

static inline void lib_MULHUVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    // bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        // if(data2[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data2);
        // }


        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        // if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
        //     twosComplement(SEW*2,resBin);
        // }
        // sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
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

    int t = sewCase(SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        // if(data1[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data1);
        // }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        // printHex(SEW/2, data1H, "data1H");
        // printHex(SEW/2, data1L, "data1L");
        // printHex(SEW/2, data2H, "data2H");
        // printHex(SEW/2, data2L, "data2L");
        
        // printHex(SEW, M0, "M0");
        // printHex(SEW, M1, "M1");
        // printHex(SEW, M2, "M2");
        // printHex(SEW, M3, "M3");

        // printHex(SEW*2, resBin, "RESBin befor twos");
        // printHex(SEW*2, resBin, "resBin before");

        if(sgn){
            twosComplement(SEW*2,resBin);
            sgn = 0;
        }
        // printHex(SEW*2, resBin, "resBin after");

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
        }
    }
}

static inline void lib_MULHSUVX (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }


        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        if(sgn == 1){
            twosComplement(SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
        }
    }
}

static inline void lib_MVVV     (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, res;
    bool bin[8];
    bool resBin[64];
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        myAbs(iss, vs1, i, &data1);
        
        res = data1;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            printf("MVVV\n");
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MVVX     (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int64_t res;
    bool bin[8];
    bool resBin[64];
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        res = rs1;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MVVI     (Iss *iss, int vs2, int64_t sim, int vd, bool vm){
    int64_t res;
    bool bin[8];
    bool resBin[64];
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        res = sim;
        intToBin(SEW, abs(res), resBin);
        if(sim < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
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


    int t = sewCase(SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);
        if(sgn){
            twosComplement(SEW*2,resBin);
            sgn = 0;
        }

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMULVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }


        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
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

    int t = sewCase(SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    // bool sgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMULUVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    // bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
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

    int t = sewCase(SEW);
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        if(sgn){
            twosComplement(SEW*2,resBin);
            sgn = 0;
        }

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMULSUVX (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }


        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, resBin, false);

        if(sgn == 1){
            twosComplement(SEW*2,resBin);
        }
        sgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
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


    int t = sewCase(SEW);
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW, vd , i, data3);

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3[SEW-1]){
            twosComplement(SEW,data3);
            vdSgn = !vdSgn;            
        }
        extend2x(SEW, data3, data3Ext, true);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);
        if(sgn){
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        if(!vdSgn){
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MACCVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW, vd , i, data3);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3[SEW-1]){
            //printf("DATA3 is neg");
            twosComplement(SEW,data3);
            vdSgn = !vdSgn;
        }

        extend2x(SEW, data3, data3Ext, true);


        // printHex(SEW, data1, "data1");
        // printHex(SEW, data2, "data2");
        // printHex(SEW, data3, "data3");

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            // printf("MUL TWOS\n");
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        // printHex(SEW*2, mulOutBin, "mulOutFinal");

        if(!vdSgn){
            // printf("SUM\n");
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            // printf("SUB\n");            
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        // printHex(SEW*2, resBin, "resBin");

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
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

    int t = sewCase(SEW);
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vd , i, data2);
        buildDataBin(iss, SEW, vs2, i, data3);
        
        // if(SEW == 64){
        //     printHex(SEW, data1, "vs1 before");
        //     printHex(SEW, data2, "vd  before");
        //     printHex(SEW, data3, "vs2 before");
        // }

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3[SEW-1]){
            twosComplement(SEW,data3);
            vdSgn = !vdSgn;            
        }
        extend2x(SEW, data3, data3Ext, true);

        // if(SEW == 64 && i == 6){
        //     printHex(SEW, data1, "vs1 before");
        //     printHex(SEW, data2, "vd  before");
        //     printHex(SEW, data3, "vs2 before");
        //     printHex(SEW*2, data3Ext, "vs2 before ext");
        // }

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        // if(SEW == 64 && i == 6){
        //     printHex(SEW/2, data1H, "data1H");
        //     printHex(SEW/2, data1L, "data1L");
        //     printHex(SEW/2, data2H, "data2H");
        //     printHex(SEW/2, data2L, "data2L");
        // }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        // if(SEW == 64 && i == 6){
        //     printHex(SEW, M0, "M0");
        //     printHex(SEW, M1, "M1");
        //     printHex(SEW, M2, "M2");
        //     printHex(SEW, M3, "M3");
        // }

        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);
        if(sgn){
            // printf("mulOut twos\n");
            twosComplement(SEW*2,mulOutBin);
            sgn = 0;
        }

        // if(SEW == 64 && i == 6){
        //     printHex(SEW*2, mulOutBin, "mulOut");
        // }


        if(!vdSgn){
            // printf("SUM\n");
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            // printf("SUB\n");            
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        // if(SEW == 64 && i == 6){
        //     printHex(SEW*2, resBin, "resBin");
        // }

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MADDVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    // printf("rs1 = %ld\n",rs1);
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vd , i, data2);
        buildDataBin(iss, SEW, vs2 , i, data3);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3[SEW-1]){
            twosComplement(SEW,data3);
            vdSgn = !vdSgn;            
        }

        extend2x(SEW, data3, data3Ext, true);


        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        if(!vdSgn){
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
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


    int t = sewCase(SEW);
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW, vd , i, data3);

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3[SEW-1]){
            twosComplement(SEW,data3);
            vdSgn = !vdSgn;            
        }
        extend2x(SEW, data3, data3Ext, true);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);
        if(!sgn){//compare with VMACC
            // printf("mulOutBin twos\n");
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        // printHex(SEW*2,mulOutBin,"muloutBin final");

        if(!vdSgn){
            // printf("SUM\n");
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            // printf("SUB\n");            
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        // printHex(SEW*2,resBin,"resBin final");


        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_NMSACVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW, vd , i, data3);

        // if(data1[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data1);
        // }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3[SEW-1]){
            twosComplement(SEW,data3);
            vdSgn = !vdSgn;            
        }
        extend2x(SEW, data3, data3Ext, true);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);
        if(!((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1))){//compare with VMACC
            // printf("mulOutBin twos\n");
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        // printHex(SEW*2,mulOutBin,"muloutBin final");

        if(!vdSgn){
            // printf("SUM\n");
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            // printf("SUB\n");            
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        // printHex(SEW*2,resBin,"resBin final");


        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_NMSUBVV  (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/

    int t = sewCase(SEW);
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vd , i, data2);
        buildDataBin(iss, SEW, vs2, i, data3);
        
        // if(SEW == 64){
        //     printHex(SEW, data1, "vs1 before");
        //     printHex(SEW, data2, "vd  before");
        //     printHex(SEW, data3, "vs2 before");
        // }

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3[SEW-1]){
            twosComplement(SEW,data3);
            vdSgn = !vdSgn;            
        }
        extend2x(SEW, data3, data3Ext, true);

        // if(SEW == 64 && i == 6){
        //     printHex(SEW, data1, "vs1 before");
        //     printHex(SEW, data2, "vd  before");
        //     printHex(SEW, data3, "vs2 before");
        //     printHex(SEW*2, data3Ext, "vs2 before ext");
        // }

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        // if(SEW == 64 && i == 6){
        //     printHex(SEW/2, data1H, "data1H");
        //     printHex(SEW/2, data1L, "data1L");
        //     printHex(SEW/2, data2H, "data2H");
        //     printHex(SEW/2, data2L, "data2L");
        // }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        // if(SEW == 64 && i == 6){
        //     printHex(SEW, M0, "M0");
        //     printHex(SEW, M1, "M1");
        //     printHex(SEW, M2, "M2");
        //     printHex(SEW, M3, "M3");
        // }

        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);
        if(!sgn){
            // printf("mulOut twos\n");
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        // if(SEW == 64 && i == 6){
        //     printHex(SEW*2, mulOutBin, "mulOut");
        // }


        if(!vdSgn){
            // printf("SUM\n");
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            // printf("SUB\n");            
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        // if(SEW == 64 && i == 6){
        //     printHex(SEW*2, resBin, "resBin");
        // }

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_NMSUBVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    // printf("rs1 = %ld\n",rs1);
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vd , i, data2);
        buildDataBin(iss, SEW, vs2 , i, data3);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3[SEW-1]){
            twosComplement(SEW,data3);
            vdSgn = !vdSgn;            
        }

        extend2x(SEW, data3, data3Ext, true);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

        if(!((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1))){
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        if(!vdSgn){
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCVV  (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/


    int t = sewCase(SEW);
    // bool data1[64], data2[64], data3[64];
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW  , vs1, i, data1);
        buildDataBin(iss, SEW  , vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3Ext[SEW*2-1]){
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;            
        }
        // extend2x(SEW, data3, data3Ext, true);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);
        if(sgn){
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        if(!vdSgn){
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    // bool data1[64], data2[64], data3[64];
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3Ext[SEW*2-1]){
            //printf("DATA3 is neg");
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;
        }

        //extend2x(SEW, data3, data3Ext, true);


        // printHex(SEW, data1, "data1");
        // printHex(SEW, data2, "data2");
        // printHex(SEW, data3, "data3");

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            // printf("MUL TWOS\n");
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        // printHex(SEW*2, mulOutBin, "mulOutFinal");

        if(!vdSgn){
            // printf("SUM\n");
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            // printf("SUB\n");            
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        // printHex(SEW*2, resBin, "resBin");

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCUVV (Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/


    int t = sewCase(SEW);
    // bool data1[64], data2[64], data3[64];
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW  , vs1, i, data1);
        buildDataBin(iss, SEW  , vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        // if(data1[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data1);
        // }
        // if(data2[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data2);
        // }
        // if(data3[SEW-1]){
        //     twosComplement(SEW,data3);
        //     vdSgn = !vdSgn;            
        // }
        // extend2x(SEW, data3, data3Ext, true);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);
        // if(sgn){
        //     twosComplement(SEW*2,mulOutBin);
        // }
        // sgn = 0;

        // if(!vdSgn){
        //     binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        // }else{
        //     binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        // }
        // vdSgn = 0;
            
        binSum2(SEW*2, mulOutBin, data3Ext, resBin);


        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCUVX (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    bool data1[64], data2[64];
    //bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    //bool sgn = 0;
    //bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        // if(data2[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data2);
        // }
        // if(data3[SEW-1]){
        //     //printf("DATA3 is neg");
        //     twosComplement(SEW,data3);
        //     vdSgn = !vdSgn;
        // }

        //extend2x(SEW, data3, data3Ext, true);


        // printHex(SEW, data1, "data1");
        // printHex(SEW, data2, "data2");
        // printHex(SEW, data3, "data3");

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

        // if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
        //     // printf("MUL TWOS\n");
        //     twosComplement(SEW*2,mulOutBin);
        // }
        // sgn = 0;

        // printHex(SEW*2, mulOutBin, "mulOutFinal");

        // if(!vdSgn){
        //     // printf("SUM\n");
        //     binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        // }else{
        //     // printf("SUB\n");            
        //     binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        // }
        // vdSgn = 0;
        
        binSum2(SEW*2, mulOutBin, data3Ext, resBin);

        // printHex(SEW*2, resBin, "resBin");

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCUSVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    // bool data1[64], data2[64], data3[64];
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3Ext[SEW*2-1]){
            //printf("DATA3 is neg");
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;
        }

        //extend2x(SEW, data3, data3Ext, true);


        // printHex(SEW, data1, "data1");
        // printHex(SEW, data2, "data2");
        // printHex(SEW, data3, "data3");

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

        if(sgn == 1){
        // if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            // printf("MUL TWOS\n");
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        // printHex(SEW*2, mulOutBin, "mulOutFinal");

        if(!vdSgn){
            // printf("SUM\n");
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            // printf("SUB\n");            
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        // printHex(SEW*2, resBin, "resBin");

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCSUVV(Iss *iss, int vs1, int vs2    , int vd, bool vm){
/*
        1H 1L
        2H 2L
    -------------
    0   0   M0H M0L
    0   M1H M1L 0   
    0   M2H M2L 0   
    M3H M3L 0   0
*/


    int t = sewCase(SEW);
    // bool data1[64], data2[64], data3[64];
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW  , vs1, i, data1);
        buildDataBin(iss, SEW  , vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        // if(data2[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data2);
        // }
        if(data3Ext[SEW*2-1]){
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;            
        }
        // extend2x(SEW, data3, data3Ext, true);

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);

        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);
        if(sgn){
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        if(!vdSgn){
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCSUVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);

    // bool data1[64], data2[64], data3[64];
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        // if(data2[SEW-1]){
        //     sgn = !sgn;
        //     twosComplement(SEW,data2);
        // }
        if(data3Ext[SEW*2-1]){
            //printf("DATA3 is neg");
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;
        }

        //extend2x(SEW, data3, data3Ext, true);


        // printHex(SEW, data1, "data1");
        // printHex(SEW, data2, "data2");
        // printHex(SEW, data3, "data3");

        for(int j = 0;j < SEW/2;j++){
            data1L[j] = data1[j];
            data2L[j] = data2[j];
            data1H[j] = data1[j+SEW/2];
            data2H[j] = data2[j+SEW/2];
        }

        binMul(SEW/2,data1L,data2L,M0);
        binMul(SEW/2,data1L,data2H,M1);
        binMul(SEW/2,data1H,data2L,M2);
        binMul(SEW/2,data1H,data2H,M3);
        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

        if((rs1 < 0 && sgn == 0) || (rs1 > 0 && sgn == 1)){
            // printf("MUL TWOS\n");
            twosComplement(SEW*2,mulOutBin);
        }
        sgn = 0;

        // printHex(SEW*2, mulOutBin, "mulOutFinal");

        if(!vdSgn){
            // printf("SUM\n");
            binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        }else{
            // printf("SUB\n");            
            binSub2(SEW*2, mulOutBin, data3Ext, resBin);
        }
        vdSgn = 0;

        // printHex(SEW*2, resBin, "resBin");

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_SLIDEUPVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){//VMA and VTA should be checked
    int t = sewCase(SEW);
    int64_t OFFSET;
    bool bin[8];
   
    OFFSET = rs1;
    int s = MAX(VSTART,OFFSET); 

    for (int i = s; i < VL*LMUL; i++){
        if(!((i-s)%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][(i-s)/8],bin);
        }
        
        if(!mask(vm,bin)){
            for(int j = 0; j <  t ; j++){
                iss->spatz.vregfile.vregs[vd][i*t+j] = iss->spatz.vregfile.vregs[vs2][(i-OFFSET)*t+j];
            }
        }
    }
}

static inline void lib_SLIDEUPVI(Iss *iss, int vs2, int64_t sim, int vd, bool vm){//VMA and VTA should be checked
    int t = sewCase(SEW);
    int64_t OFFSET;
    bool bin[8];
    OFFSET = sim;
    int s = MAX(VSTART,OFFSET); 

    for (int i = s; i < VL*LMUL; i++){
        if(!((i-s)%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][(i-s)/8],bin);
        }
        if(!mask(vm,bin)){
            for(int j = 0; j <  t ; j++){
                iss->spatz.vregfile.vregs[vd][i*t+j] = iss->spatz.vregfile.vregs[vs2][(i-OFFSET)*t+j];
            }
        }
    }
}

static inline void lib_SLIDEDWVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){//VMA and VTA should be checked
    int t = sewCase(SEW);
    int64_t OFFSET;
    bool bin[8];
    OFFSET = rs1;
    int s = MAX(VSTART,OFFSET); 

    for (int i = VSTART; i < VL*LMUL; i++){
        if(i+OFFSET >= VLMAX){
            for(int j = 0; j <  t ; j++){
                iss->spatz.vregfile.vregs[vd][i*t+j] = 0;
            }            
        }else{
            if(!((i-s)%8)){
                intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][(i-s)/8],bin);
            }
            
            if(!mask(vm,bin)){
                for(int j = 0; j <  t ; j++){
                    iss->spatz.vregfile.vregs[vd][i*t+j] = iss->spatz.vregfile.vregs[vs2][(i+OFFSET)*t+j];
                }
            }
        }
    }
}

static inline void lib_SLIDEDWVI(Iss *iss, int vs2, int64_t sim, int vd, bool vm){//VMA and VTA should be checked
    int t = sewCase(SEW);
    int64_t OFFSET;
    bool bin[8];
    OFFSET = sim;

    int s = MAX(VSTART,OFFSET); 

    for (int i = VSTART; i < VL*LMUL; i++){
        printf("i = %d \tOFFSET = %ld\t VLMAX = %d\n",i,OFFSET,VLMAX);
        if(i+OFFSET >= VLMAX){
            printf("GREATER THAN VLMAX\n");
            for(int j = 0; j <  t ; j++){
                iss->spatz.vregfile.vregs[vd][i*t+j] = 0;
            }            
        }else{
            if(!((i-s)%8)){
                intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][(i-s)/8],bin);
            }
            
            if(!mask(vm,bin)){
                for(int j = 0; j <  t ; j++){
                    iss->spatz.vregfile.vregs[vd][i*t+j] = iss->spatz.vregfile.vregs[vs2][(i+OFFSET)*t+j];
                }
            }
        }
    }
}

static inline void lib_SLIDE1UVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){//VMA and VTA should be checked
    int t = sewCase(SEW);
    bool bin[8];
    bool data1[64];
   
    int s = MAX(VSTART,1); 
    intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][0],bin);

    int i = 0;
    intToBin(64, abs((int64_t)rs1), data1);
    if(rs1 < 0){
        twosComplement(64,data1);
    }
    if(VSTART == 0){
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, 0, data1);
            // for(int j = 0; j <  t ; j++){
            //     iss->spatz.vregfile.vregs[vd][j] = iss->spatz.vregfile.vregs[vs2][(i-OFFSET)*t+j];
            // }
        }
    }
    for (int i = s; i < VL*LMUL; i++){
        if(!((i-s)%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][(i-s)/8],bin);
        }
        
        if(!mask(vm,bin)){
            for(int j = 0; j <  t ; j++){
                iss->spatz.vregfile.vregs[vd][i*t+j] = iss->spatz.vregfile.vregs[vs2][(i-1)*t+j];
            }
        }
    }
}

static inline void lib_SLIDE1DVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){//VMA and VTA should be checked
    int t = sewCase(SEW);
    bool bin[8];
    bool data1[64];
    
    intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][0],bin);

    int i = 0;
    intToBin(64, abs((int64_t)rs1), data1);
    if(rs1 < 0){
        twosComplement(64,data1);
    }

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            if(i == VL-1){
                writeToVReg(iss, SEW, vd, VL-1, data1);   
            }else{
                for(int j = 0; j <  t ; j++){
                    iss->spatz.vregfile.vregs[vd][i*t+j] = iss->spatz.vregfile.vregs[vs2][(i+1)*t+j];
                }
            }
        }
    }
}

static inline void lib_DIVVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        // res = data1/data2;
        res = data2/data1;

        printf("data1 = %ld\tdata2 = %ld\t res = %ld\n",data1,data2,res);

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_DIVVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data2/data1;


        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_DIVUVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);

        // res = data1/data2;
        res = data2/data1;

        printf("data1 = %ld\tdata2 = %ld\t res = %ld\n",data1,data2,res);

        // intToBin(SEW, abs(res), resBin);
        intToBin(SEW, res, resBin);
        // if(res < 0){
        //     twosComplement(SEW, resBin);
        // }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_DIVUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        
        res = data2/data1;


        // intToBin(SEW, abs(res), resBin);
        intToBin(SEW, res, resBin);
        // if(res < 0){
        //     twosComplement(SEW, resBin);
        // }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_REMVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs1, i, &data1);
        myAbs(iss, vs2, i, &data2);

        // res = data1/data2;
        res = data2%data1;

        printf("data1 = %ld\tdata2 = %ld\t res = %ld\n",data1,data2,res);

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_REMVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbs(iss, vs2, i, &data2);
        
        res = data2%data1;


        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_REMUVV   (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int t = sewCase(SEW);
    uint64_t data1, data2;
    int64_t res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);

        // res = data1/data2;
        res = data2%data1;

        printf("data1 = %lu\tdata2 = %lu\t res = %lu\n",data1,data2,res);

        // intToBin(SEW, abs(res), resBin);
        intToBin(SEW, res, resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_REMUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int t = sewCase(SEW);
    uint64_t data1, data2;
    int64_t res;
    bool bin[8];
    bool resBin[64];
    
    data1 = rs1;

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        
        res = data2%data1;


        // intToBin(SEW, abs(res), resBin);
        intToBin(SEW, res, resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FADDVV   (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(iss, ff_add, data1, data2, e, m, res);
        restoreFFRoundingMode(old);
        intToBinU(SEW, res, resBin);

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FADDVF   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(iss, ff_add, data1, data2, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);

        }        
    }
}

static inline void lib_FSUBVV   (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(iss, ff_sub, data2, data1, e, m, res);
        restoreFFRoundingMode(old);
        intToBinU(SEW, res, resBin);

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FSUBVF   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(iss, ff_sub, data2, data1, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FRSUBVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(iss, ff_sub, data1, data2, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FMINVV   (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(iss, ff_min, data2, data1, e, m, res);
        restoreFFRoundingMode(old);
        intToBinU(SEW, res, resBin);

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMINVF   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(iss, ff_min, data2, data1, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FMAXVV   (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(iss, ff_max, data2, data1, e, m, res);
        restoreFFRoundingMode(old);
        intToBinU(SEW, res, resBin);

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMAXVF   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(iss, ff_max, data2, data1, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FMULVV   (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(iss, ff_mul, data2, data1, e, m, res);
        restoreFFRoundingMode(old);
        intToBinU(SEW, res, resBin);

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMULVF   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(iss, ff_mul, data2, data1, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FMACCVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    unsigned long int res1,res2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_macc, data2, data1, data3, e, m, res);
            restoreFFRoundingMode(old);

            intToBinU(SEW, res, resBin);

            if(SEW == 64){
                printHex(SEW,resBin,"RES");
            }

            writeToVReg(iss, SEW, vd, i, resBin);

            // int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            // FLOAT_EXEC_2(iss, ff_mul, data2, data1, e, m, res1);

            // ff_init(&ff_a, env);                             
            // ff_init(&ff_b, env);                             
            // ff_init(&ff_res, env);                           
            // // flexfloat_set_bits(&ff_a, res1);                    
            // // flexfloat_set_bits(&ff_b, data3);
            // flexfloat_set_bits(&ff_a, data3);                    
            // flexfloat_set_bits(&ff_b, res1);            

            // feclearexcept(FE_ALL_EXCEPT);             
            // ff_add(&ff_res, &ff_a, &ff_b);               
            // update_fflags_fenv(iss);                     
            // res2 = flexfloat_get_bits(&ff_res);

            // restoreFFRoundingMode(old);

            // intToBinU(SEW, res2, resBin);
            // printf("***** i = %d\n",i);
            // printHex(SEW,resBin,"RESBIN");

            // writeToVReg(iss, SEW, vd, i, resBin);
            

            // int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // FLOAT_EXEC_3(iss, ff_macc, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res1 = %lx\n",res);        
            // restoreFFRoundingMode(old);
            // intToBinU(SEW, res, resBin);
            // writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMACCVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_macc, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res = %lx\n",res);        
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FNMACCVV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    unsigned long int res1,res2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_nmacc, data2, data1, data3, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNMACCVF (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_nmacc, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res = %lx\n",res);        
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FMSACVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    unsigned long int res1,res2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_msac, data2, data1, data3, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMSACVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_msac, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res = %lx\n",res);        
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FNMSACVV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    unsigned long int res1,res2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_nmsac, data2, data1, data3, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNMSACVF (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_nmsac, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res = %lx\n",res);        
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FMADDVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    unsigned long int res1,res2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_madd, data1, data2, data3, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMADDVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_madd, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res = %lx\n",res);        
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FNMADDVV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    unsigned long int res1,res2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_nmadd, data1, data2, data3, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNMADDVF (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_nmadd, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res = %lx\n",res);        
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FMSUBVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    unsigned long int res1,res2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_msub, data1, data2, data3, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMSUBVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_msub, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res = %lx\n",res);        
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FNMSUBVV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    unsigned long int res1,res2;
    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_nmsub, data1, data2, data3, e, m, res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNMSUBVF (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(iss, ff_nmsub, data1, data2, data3, e, m, res);
            // printf("data1 = %lx\tdata2 = %lx\tdata3 = %lx\n",data1,data2,data3);
            // printf("res = %lx\n",res);        
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }        
    }
}

static inline void lib_FWADDVV   (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data1);
            flexfloat_set_bits(&ff_b, data2);
            feclearexcept(FE_ALL_EXCEPT);
            ff_add(&ff_res, &ff_a, &ff_b);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWADDVF   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data1);
            flexfloat_set_bits(&ff_b, data2);
            feclearexcept(FE_ALL_EXCEPT);
            ff_add(&ff_res, &ff_a, &ff_b);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWSUBVV   (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data2);
            flexfloat_set_bits(&ff_b, data1);
            feclearexcept(FE_ALL_EXCEPT);
            ff_sub(&ff_res, &ff_a, &ff_b);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWSUBVF   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data2);
            flexfloat_set_bits(&ff_b, data1);
            feclearexcept(FE_ALL_EXCEPT);
            ff_sub(&ff_res, &ff_a, &ff_b);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWMULVV   (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data1);
            flexfloat_set_bits(&ff_b, data2);
            feclearexcept(FE_ALL_EXCEPT);
            ff_mul(&ff_res, &ff_a, &ff_b);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWMULVF   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data1);
            flexfloat_set_bits(&ff_b, data2);
            feclearexcept(FE_ALL_EXCEPT);
            ff_mul(&ff_res, &ff_a, &ff_b);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWMACCVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW*2, vd , i, &data3);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_c, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env);
            ff_init(&ff_c, env2);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data1);
            flexfloat_set_bits(&ff_b, data2);
            flexfloat_set_bits(&ff_c, data3);
            feclearexcept(FE_ALL_EXCEPT);
            ff_macc(iss, &ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWMACCVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL*LMUL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->spatz.vregfile.vregs[0][i/8],bin);
        }
        
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW*2, vd , i, &data3);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_c, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env);
            ff_init(&ff_c, env2);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data1);
            flexfloat_set_bits(&ff_b, data2);
            flexfloat_set_bits(&ff_c, data3);
            feclearexcept(FE_ALL_EXCEPT);
            ff_macc(iss, &ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR LOAD STORE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// ADD MASK TO LOAD AND STORE INSTRUCTIONS


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
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    for (int i = VSTART; i < VL*LMUL; i+=4){
 //       if(!i){
 //           printf("Vlsu_io_access\n");
        iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
 //       }else{
 //           printf("handle_pending_io_access\n");
 //           vlsu.handle_pending_io_access(iss);
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
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    // printf("VLEN/8 = %d\n",vlEN/8);
    // printf("VM = %d\n",vm);
    // printf("RS1 = %lu\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];

    for (int i = VSTART; i < VL*LMUL*2; i+=4){
        //if(!i){
            //vlsu.Vlsu_io_access(iss, rs1, vlEN/8, data, false);
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    vlsu.handle_pending_io_access(iss);
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
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    // printf("VLEN/8 = %d\n",vlEN/8);
    // printf("VM = %d\n",vm);
    // printf("RS1 = %lu\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];

    for (int i = VSTART; i < VL*LMUL*4; i+=4){
        //if(!i){
            //vlsu.Vlsu_io_access(iss, rs1, vlEN/8, data, false);
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    vlsu.handle_pending_io_access(iss);
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
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    // printf("VLEN/8 = %d\n",vlEN/8);
    // printf("VM = %d\n",vm);
    // printf("RS1 = %lu\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];

    for (int i = VSTART; i < VL*LMUL*8; i+=4){
        //if(!i){
            //vlsu.Vlsu_io_access(iss, rs1, vlEN/8, data, false);
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    vlsu.handle_pending_io_access(iss);
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
    // printf("vd  = %d\n",vs3);

    uint64_t start_add = rs1;

    for (int i = VSTART; i < VL*LMUL; i+=4){
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
        //    vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

static inline void lib_VSE16V(Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);

    uint64_t start_add = (uint64_t)rs1;    
    for (int i = VSTART; i < VL*LMUL*2; i+=4){
        /*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;

        // if(!i){
        //     vlsu.Vlsu_io_access(iss, rs1,vlEN/8,data,true);
        // }else{
        //     vlsu.handle_pending_io_access(iss);
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
        //    vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

static inline void lib_VSE32V(Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);

    uint64_t start_add = rs1;    
    for (int i = VSTART; i < VL*LMUL*4; i+=4){
        /*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;

        // if(!i){
        //     vlsu.Vlsu_io_access(iss, rs1,vlEN/8,data,true);
        // }else{
        //     vlsu.handle_pending_io_access(iss);
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
        //    vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

static inline void lib_VSE64V(Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    // uint8_t data[vl];
    // uint32_t temp;
    // for (int i = VSTART; i < vl*LMUL*2; i+=1){
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
    //         vlsu.Vlsu_io_access(iss, rs1,vlEN/8,data,true);
    //     }else{
    //         vlsu.handle_pending_io_access(iss);
    //     }
    // }
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);

    uint64_t start_add = rs1;    
    for (int i = VSTART; i < VL*LMUL*8; i+=4){
        /*
        data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->spatz.vregfile.vregs[0][i+0]%2)) ? iss->spatz.vregfile.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->spatz.vregfile.vregs[0][i+1]%2)) ? iss->spatz.vregfile.vregs[vs3][i+1] : 0;

        // if(!i){
        //     vlsu.Vlsu_io_access(iss, rs1,vlEN/8,data,true);
        // }else{
        //     vlsu.handle_pending_io_access(iss);
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
        //    vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            WHOLE REGISTER LOAD/STORE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void lib_VL1RV (Iss *iss, iss_reg_t rs1, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    // printf("VLE8\n");
    // printf("vd = %d\n",vd);
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    for (int k = 0; k < 1; k++){
        for (int i = 0; i < iss->csr.vlenb.value; i+=4){
                iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,false);


            // printf("data0 = %d\n",data[0]);
            // printf("data1 = %d\n",data[1]);
            // printf("data2 = %d\n",data[2]);
            // printf("data3 = %d\n",data[3]);

            iss->spatz.vregfile.vregs[vd+k][i+0] = data[0];
            iss->spatz.vregfile.vregs[vd+k][i+1] = data[1];
            iss->spatz.vregfile.vregs[vd+k][i+2] = data[2];
            iss->spatz.vregfile.vregs[vd+k][i+3] = data[3];

            start_add += 4;
        }
    }
}
static inline void lib_VL2RV (Iss *iss, iss_reg_t rs1, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    // printf("VLE8\n");
    // printf("vd = %d\n",vd);
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    for (int k = 0; k < 2; k++){
        for (int i = 0; i < iss->csr.vlenb.value; i+=4){
                iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,false);


            // printf("data0 = %d\n",data[0]);
            // printf("data1 = %d\n",data[1]);
            // printf("data2 = %d\n",data[2]);
            // printf("data3 = %d\n",data[3]);

            iss->spatz.vregfile.vregs[vd+k][i+0] = data[0];
            iss->spatz.vregfile.vregs[vd+k][i+1] = data[1];
            iss->spatz.vregfile.vregs[vd+k][i+2] = data[2];
            iss->spatz.vregfile.vregs[vd+k][i+3] = data[3];

            start_add += 4;
        }
    }
}
static inline void lib_VL4RV (Iss *iss, iss_reg_t rs1, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    // printf("VLE8\n");
    // printf("vd = %d\n",vd);
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    for (int k = 0; k < 4; k++){
        for (int i = 0; i < iss->csr.vlenb.value; i+=4){
                iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,false);


            // printf("data0 = %d\n",data[0]);
            // printf("data1 = %d\n",data[1]);
            // printf("data2 = %d\n",data[2]);
            // printf("data3 = %d\n",data[3]);

            iss->spatz.vregfile.vregs[vd+k][i+0] = data[0];
            iss->spatz.vregfile.vregs[vd+k][i+1] = data[1];
            iss->spatz.vregfile.vregs[vd+k][i+2] = data[2];
            iss->spatz.vregfile.vregs[vd+k][i+3] = data[3];

            start_add += 4;
        }
    }
}
static inline void lib_VL8RV (Iss *iss, iss_reg_t rs1, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    // printf("VLE8\n");
    // printf("vd = %d\n",vd);
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    for (int k = 0; k < 8; k++){
        for (int i = 0; i < iss->csr.vlenb.value; i+=4){
                iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,false);


            // printf("data0 = %d\n",data[0]);
            // printf("data1 = %d\n",data[1]);
            // printf("data2 = %d\n",data[2]);
            // printf("data3 = %d\n",data[3]);

            iss->spatz.vregfile.vregs[vd+k][i+0] = data[0];
            iss->spatz.vregfile.vregs[vd+k][i+1] = data[1];
            iss->spatz.vregfile.vregs[vd+k][i+2] = data[2];
            iss->spatz.vregfile.vregs[vd+k][i+3] = data[3];

            start_add += 4;
        }
    }
}


static inline void lib_VS1RV (Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);
    // printf("vd  = %d\n",vs3);

    uint64_t start_add = rs1;

    for(int k = 0; k < 1; k++){
        for (int i = 0; i < iss->csr.vlenb.value; i+=4){
            data[0]  = iss->spatz.vregfile.vregs[vs3+k][i+0];
            data[1]  = iss->spatz.vregfile.vregs[vs3+k][i+1];
            data[2]  = iss->spatz.vregfile.vregs[vs3+k][i+2];
            data[3]  = iss->spatz.vregfile.vregs[vs3+k][i+3];


            // printf("STORE8 \n");
            // printf("data0  = %d\n",data[0 ]);
            // printf("data1  = %d\n",data[1 ]);
            // printf("data2  = %d\n",data[2 ]);
            // printf("data3  = %d\n",data[3 ]);

            // printf("addr  = %lu\n",start_add);

            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
            start_add += 4;
        }
    }
}
static inline void lib_VS2RV (Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);
    // printf("vd  = %d\n",vs3);

    uint64_t start_add = rs1;

    for(int k = 0; k < 2; k++){
        for (int i = 0; i < iss->csr.vlenb.value; i+=4){
            data[0]  = iss->spatz.vregfile.vregs[vs3+k][i+0];
            data[1]  = iss->spatz.vregfile.vregs[vs3+k][i+1];
            data[2]  = iss->spatz.vregfile.vregs[vs3+k][i+2];
            data[3]  = iss->spatz.vregfile.vregs[vs3+k][i+3];


            // printf("STORE8 \n");
            // printf("data0  = %d\n",data[0 ]);
            // printf("data1  = %d\n",data[1 ]);
            // printf("data2  = %d\n",data[2 ]);
            // printf("data3  = %d\n",data[3 ]);

            // printf("addr  = %lu\n",start_add);

            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
            start_add += 4;
        }
    }
}
static inline void lib_VS4RV (Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);
    // printf("vd  = %d\n",vs3);

    uint64_t start_add = rs1;

    for(int k = 0; k < 4; k++){
        for (int i = 0; i < iss->csr.vlenb.value; i+=4){
            data[0]  = iss->spatz.vregfile.vregs[vs3+k][i+0];
            data[1]  = iss->spatz.vregfile.vregs[vs3+k][i+1];
            data[2]  = iss->spatz.vregfile.vregs[vs3+k][i+2];
            data[3]  = iss->spatz.vregfile.vregs[vs3+k][i+3];


            // printf("STORE8 \n");
            // printf("data0  = %d\n",data[0 ]);
            // printf("data1  = %d\n",data[1 ]);
            // printf("data2  = %d\n",data[2 ]);
            // printf("data3  = %d\n",data[3 ]);

            // printf("addr  = %lu\n",start_add);

            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
            start_add += 4;
        }
    }
}
static inline void lib_VS8RV (Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);
    // printf("vd  = %d\n",vs3);

    uint64_t start_add = rs1;

    for(int k = 0; k < 8; k++){
        for (int i = 0; i < iss->csr.vlenb.value; i+=4){
            data[0]  = iss->spatz.vregfile.vregs[vs3+k][i+0];
            data[1]  = iss->spatz.vregfile.vregs[vs3+k][i+1];
            data[2]  = iss->spatz.vregfile.vregs[vs3+k][i+2];
            data[3]  = iss->spatz.vregfile.vregs[vs3+k][i+3];


            // printf("STORE8 \n");
            // printf("data0  = %d\n",data[0 ]);
            // printf("data1  = %d\n",data[1 ]);
            // printf("data2  = %d\n",data[2 ]);
            // printf("data3  = %d\n",data[3 ]);

            // printf("addr  = %lu\n",start_add);

            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
            start_add += 4;
        }
    }
}



static inline void lib_VLSE8V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    // printf("VLE8\n");
    // printf("vd = %d\n",vd);
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    int t;
    switch (rs2){
    case (0): t = 4;break;
    case (1): t = 2;break;
    default : t = 1;break;
    }

    int i = VSTART;
    while(i < VL*LMUL){
    // for (int i = VSTART; i < vl*LMUL; i+=t){
 //       if(!i){
 //           printf("Vlsu_io_access\n");
            iss->spatz.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
 //       }else{
 //           printf("handle_pending_io_access\n");
 //           vlsu.handle_pending_io_access(iss);
 //       }

        // printf("data0 = %d\n",data[0]);
        // printf("data1 = %d\n",data[1]);
        // printf("data2 = %d\n",data[2]);
        // printf("data3 = %d\n",data[3]);
        
        if(rs2 == 0){
            iss->spatz.vregfile.vregs[vd][i+0] = data[0];
            iss->spatz.vregfile.vregs[vd][i+1] = data[1];
            iss->spatz.vregfile.vregs[vd][i+2] = data[2];
            iss->spatz.vregfile.vregs[vd][i+3] = data[3];
            start_add += 4;
            i+=4;
        }else if(rs2 == 1){
            iss->spatz.vregfile.vregs[vd][i+0] = data[0];
            iss->spatz.vregfile.vregs[vd][i+1] = data[2];
            start_add += 4;
            i+=2;
        }else if(rs2 == 2){
            iss->spatz.vregfile.vregs[vd][i+0] = data[0];
            iss->spatz.vregfile.vregs[vd][i+1] = data[3];
            start_add += 4;
            i+=2;        
        }else{
            iss->spatz.vregfile.vregs[vd][i+0] = data[0];
            start_add += rs2+1;
            i++;
        }
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

    iss->csr.vtype.value = vtype;

    //implement it like what is implemented in SV
    //if(vlEN*iss->spatz.LMUL_VALUES[lmul]/SEW_VALUES[sew] == VLMAX){
        VSTART = 0;
        SEW = iss->spatz.SEW_VALUES[sew];
        LMUL = iss->spatz.LMUL_VALUES[lmul];
        //iss->spatz.VMA = ma;
        //iss->spatz.VTA = ta;

        //  localparam int unsigned MAXVL  = VLEN; and VLEN = 256
        if(idxRs1){
            AVL = rs1;
            VL = MIN(AVL,VLMAX);//spec page 30 - part c and d
            //printf("CASE1\n");
        //}else if(VLMAX < rs1){
        }else if(!idxRs1 && idxRd){
            AVL = UINT32_MAX;
            VL  = VLMAX;
            //printf("CASE2\n");
            //vl = VLEN/SEW;
        }else{
            AVL = VL;
            //printf("CASE3\n");
        }
    //} else {
        // vtype for invalid situation
        //vtype_d = '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0};
        //vl_d    = '0;
    //}
    // printf("SEW = %d \n",SEW);
    // printf("LMUL = %f \n",iss->spatz.LMUL);
    // printf("VL = %ld \n",vl);
    // printf("AVL = %d \n",AVL);
    // printf("rs1 = %d \n",rs1);
    return VL;
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
    // printf("vsetvl\n");
    uint32_t AVL;

    // SET NEW VTYPE
    // spatz_req.vtype = {1'b0, decoder_req_i.instr[27:20]};

    iss->csr.vtype.value = rs2;
    int sew = (rs2/4)%8;
    int lmul = rs2%4;
    //implement it like what is implemented in SV
    //if(vlEN*iss->spatz.LMUL_VALUES[lmul]/SEW_VALUES[sew] == VLMAX){
        VSTART = 0;
        SEW = iss->spatz.SEW_VALUES[sew];
        LMUL = iss->spatz.LMUL_VALUES[lmul];
        //iss->spatz.VMA = ma;
        //iss->spatz.VTA = ta;

        //  localparam int unsigned MAXVL  = VLEN; and VLEN = 256
        if(idxRs1){
            AVL = rs1;
            VL = MIN(AVL,VLMAX);//spec page 30 - part c and d
            //printf("CASE1\n");
        //}else if(VLMAX < rs1){
        }else if(!idxRs1 && idxRd){
            AVL = UINT32_MAX;
            VL  = VLMAX;
            //printf("CASE2\n");
            //vl = VLEN/SEW;
        }else{
            AVL = VL;
            //printf("CASE3\n");
        }
    //} else {
        // vtype for invalid situation
        //vtype_d = '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0};
        //vl_d    = '0;
    //}
    // printf("SEW = %d \n",SEW);
    // printf("LMUL = %f \n",iss->spatz.LMUL);
    // printf("VL = %ld \n",vl);
    // printf("AVL = %d \n",AVL);
    // printf("rs1 = %d \n",rs1);
    return VL;
    //Not sure about the write back procedure
}


#endif 