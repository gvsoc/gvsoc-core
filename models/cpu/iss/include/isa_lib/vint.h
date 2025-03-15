/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VINT_H
#define VINT_H

#include "cpu/iss/include/spatz.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR REGISTER FILE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "cpu/iss/flexfloat/flexfloat.h"
#include "int.h"
#include <stdint.h>
#include <math.h>
#include <fenv.h>
#include "assert.h"


#pragma STDC FENV_ACCESS ON


#define FLOAT_INIT_1(a, e, m)                        \
    flexfloat_t ff_a, ff_res;                        \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);

#define FLOAT_INIT_2(a, b, e, m)                     \
    flexfloat_t ff_a, ff_b, ff_res;                  \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_b, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);                    \
    flexfloat_set_bits(&ff_b, b);

#define FLOAT_INIT_3(a, b, c, e, m)                  \
    flexfloat_t ff_a, ff_b, ff_c, ff_res;            \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_b, env);                             \
    ff_init(&ff_c, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);                    \
    flexfloat_set_bits(&ff_b, b);                    \
    flexfloat_set_bits(&ff_c, c);



#define FLOAT_EXEC_1(name, a, e, m ,res)            \
    FLOAT_INIT_1(a, e, m)                           \
    feclearexcept(FE_ALL_EXCEPT);                   \
    name(&ff_res, &ff_a);                           \
    update_fflags_fenv(iss);                        \
    res = flexfloat_get_bits(&ff_res);

#define FLOAT_EXEC_2(name, a, b, e, m ,res)         \
    FLOAT_INIT_2(a, b, e, m)                        \
    feclearexcept(FE_ALL_EXCEPT);                   \
    name(&ff_res, &ff_a, &ff_b);                    \
    update_fflags_fenv(iss);                        \
    res = flexfloat_get_bits(&ff_res);

#define FLOAT_EXEC_3(name, a, b, c, e, m ,res)      \
    FLOAT_INIT_3(a, b, c, e, m)                     \
    feclearexcept(FE_ALL_EXCEPT);                   \
    name(&ff_res, &ff_a, &ff_b, &ff_c);             \
    update_fflags_fenv(iss);                        \
    res = flexfloat_get_bits(&ff_res);



#define mask(vm,bin) (!(vm) && !bin[i%8])

#define SEW iss->vector.SEW_t
#define LMUL iss->vector.LMUL_t
#define VL iss->csr.vl.value
#define VSTART iss->csr.vstart.value


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
    for(int i = size-1; i >= 3  ; i-=4){
        hexVal = a[i]*8 + a[i-1]*4 + a[i-2]*2 + a[i-3];
        printf("%x",hexVal);
    }
    printf("\n");
}

static inline int  bin8ToChar(bool *bin,int s, int e){
    int c = 0;
    for(int i = s; i < e;i++){
        c += bin[i]*pow(2,i-s);
    }
    return c;
}

static inline int  sewCase(int sew){ //sew/8
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
    }
}

static inline void doubleToBin(int size, double dataIn, bool* res, int *point){
    bool temp[size];
    int dec = floor(abs(dataIn));
    double frac = abs(dataIn) - dec;
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
    *res = 0;
    bool flag = 0;
    if(a[size-1]){
        twosComplement(size,a);
        flag = 1;
    }
    int i = 0;
    while(i < point){
        *res += a[size-point+i] * pow(2,i);
        i++;
    }
    while(i < size){
        *res += a[size-1-i] * pow(2,(point-i-1));
        i++;
    }
    if(flag){
        *res = -(*res);
    }
}

static inline void buildDataInt(Iss *iss, int vs, int i, int64_t* data){
    uint8_t temp[8];
    int iteration = SEW/8;
    for(int j = 0;j < iteration;j++){
        temp[j] = iss->vector.vregs[vs][i*iteration+j];
    }
    *data = 0;
    for(int j = 0;j < iteration;j++){
        *data += temp[j]*pow(2,8*j);
    }
}

static inline void buildDataBin(Iss *iss, int size, int vs, int i, bool* dataBin){
    int iteration = size/8;
    for(int j = 0;j < iteration;j++){
        intToBinU(8,(uint64_t)abs(iss->vector.vregs[vs][i*iteration+j]), &dataBin[j*8]);
    }
}

static inline void myAbs(Iss *iss, int size, int vs, int i, int64_t* data){
    uint8_t temp[8];
    int cin = 0;
    int iteration = size/8;

    if(iss->vector.vregs[vs][i*iteration+iteration-1] > 127){
        cin = 1;
        for(int j = 0;j < iteration;j++){
            temp[j] = 255 - iss->vector.vregs[vs][i*iteration+j];
        }
    }else{
        cin = 0;
        for(int j = 0;j < iteration;j++){
            temp[j] = iss->vector.vregs[vs][i*iteration+j];
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
    uint8_t temp[8];
    int iteration = size/8;

    for(int j = 0;j < iteration;j++){
        temp[j] = iss->vector.vregs[vs][i*iteration+j];
    }

    *data = 0;
    for(int j = 0;j < iteration;j++){
        *data += temp[j]*(uint64_t)pow(2,8*j);
    }
}

static inline void writeToVReg(Iss *iss, int size, int vd, int i, bool *bin){
    int iteration = size/8;
    for(int j = 0; j < iteration; j++){
        iss->vector.vregs[vd][i*iteration+j] = bin8ToChar(bin,8*j,8*(j+1));
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
}

static inline void binMul(int size, bool *a, bool *b, bool *res){
    int64_t aDec,bDec;
    uint64_t temp;
    binToInt(size, a, &aDec);
    binToInt(size, b, &bDec);
    temp = aDec * bDec;
    intToBinU(size*2, temp, res);

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

    binShift(size*2, 0     , M02x, temp0, false, isSigned);
    binShift(size*2, size/2, M12x, temp1, false, isSigned);
    binShift(size*2, size/2, M22x, temp2, false, isSigned);
    binShift(size*2, size  , M32x, temp3, false, isSigned);

    binSum4(size*2, temp0, temp1, temp2, temp3, res);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                      FLOATING POINT FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// check the iss

INLINE void ff_macc(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {

    dest->value = (a->value * b->value) + c->value;

    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = (a->exact_value * b->exact_value) + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_nmacc(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {

    dest->value = -(a->value * b->value) - c->value;
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = -(a->exact_value * b->exact_value) - c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    // if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_msac(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    // assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
    //        (dest->desc.exp_bits == b->desc.exp_bits) && (dest->desc.frac_bits == b->desc.frac_bits) &&
    //        (dest->desc.exp_bits == c->desc.exp_bits) && (dest->desc.frac_bits == c->desc.frac_bits));

    dest->value = (a->value * b->value) - c->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = (a->exact_value * b->exact_value) - c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    // if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_nmsac(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    // assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
    //        (dest->desc.exp_bits == b->desc.exp_bits) && (dest->desc.frac_bits == b->desc.frac_bits) &&
    //        (dest->desc.exp_bits == c->desc.exp_bits) && (dest->desc.frac_bits == c->desc.frac_bits));

    dest->value = -(a->value * b->value) + c->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = -(a->exact_value * b->exact_value) + c->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    // if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_madd(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    // assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
    //        (dest->desc.exp_bits == b->desc.exp_bits) && (dest->desc.frac_bits == b->desc.frac_bits) &&
    //        (dest->desc.exp_bits == c->desc.exp_bits) && (dest->desc.frac_bits == c->desc.frac_bits));

    dest->value = (a->value * c->value) + b->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = (a->exact_value * c->exact_value) + b->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    // if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_nmadd(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    // assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
    //        (dest->desc.exp_bits == b->desc.exp_bits) && (dest->desc.frac_bits == b->desc.frac_bits) &&
    //        (dest->desc.exp_bits == c->desc.exp_bits) && (dest->desc.frac_bits == c->desc.frac_bits));

    dest->value = -(a->value * c->value) - b->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = -(a->exact_value * c->exact_value) - b->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    // if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_msub(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    // assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
    //        (dest->desc.exp_bits == b->desc.exp_bits) && (dest->desc.frac_bits == b->desc.frac_bits) &&
    //        (dest->desc.exp_bits == c->desc.exp_bits) && (dest->desc.frac_bits == c->desc.frac_bits));

    dest->value = (a->value * c->value) - b->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = (a->exact_value * c->exact_value) - b->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    // if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_nmsub(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    // assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
    //        (dest->desc.exp_bits == b->desc.exp_bits) && (dest->desc.frac_bits == b->desc.frac_bits) &&
    //        (dest->desc.exp_bits == c->desc.exp_bits) && (dest->desc.frac_bits == c->desc.frac_bits));


    dest->value = -(a->value * c->value) + b->value;
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = -(a->exact_value * c->exact_value) + b->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    // if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

INLINE void ff_sgnj(flexfloat_t *dest, const flexfloat_t *a,const flexfloat_t *b) {
    // assert((dest->desc.exp_bits == a->desc.exp_bits)&&(dest->desc.frac_bits == a->desc.frac_bits));
    // printf("a val = %.20f\tres val = %.20f\n",a->value,dest->value);
    dest->value = (b->desc.exp_bits) ? abs(a->value):-abs(a->value);
    // printf("a val = %.20f\tb val = %.20f\tc val = %.20f\tres val = %.20f\n",a->value,b->value,c->value,dest->value);
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = (dest->exact_value >= 0) ? abs(a->exact_value):-abs(a->exact_value);
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    // if(StatsEnabled) getOpStats(dest->desc)->macc += 1;
    #endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





static inline void lib_ADDVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);
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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = sim;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = sim;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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

static inline void lib_ANDVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = sim;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = sim;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = sim;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);

        res = (data1 > data2)?data2:data1;

        intToBinU(SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MINUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);

        res = (data1 > data2)?data2:data1;

        intToBinU(SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);

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
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);

        res = (data1 > data2)?data1:data2;

        intToBinU(SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MAXUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);

        res = (data1 > data2)?data1:data2;

        intToBinU(SEW, res, resBin);

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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MULVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
        }
    }
}

static inline void lib_MULHVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
        }
    }
}

static inline void lib_MULHUVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];

    intToBin(64, rs1, data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            writeToVReg(iss, SEW, vd, i, &resBin[SEW]);
        }
    }
}

static inline void lib_MULHSUVX (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbs(iss, SEW, vs1, i, &data1);

        res = data1;

        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_MVVX     (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int64_t res;
    bool bin[8];
    bool resBin[64];
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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

static inline void lib_MVSX     (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int64_t res;
    bool resBin[64];
    res = rs1;
    if(VSTART < VL){
        intToBin(SEW, abs(res), resBin);
        if(res < 0){
            twosComplement(SEW, resBin);
        }
        if(vm){
            writeToVReg(iss, SEW, vd, 0, resBin);
        }else{
            printf("MVSX VM=0 is RESERVED\n");
        }
    }
}

static inline iss_reg_t lib_MVXS     (Iss *iss, int vs2, bool vm){

    int64_t data1;

    myAbs(iss, SEW, vs2, 0, &data1);

    return iss_reg_t(data1);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];

    intToBin(64, rs1, data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128];
    bool sgn = 0;

    intToBin(64, rs1, data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW, vd , i, data3);

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
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vd , i, data2);
        buildDataBin(iss, SEW, vs2, i, data3);

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
            sgn = 0;
        }

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

static inline void lib_MADDVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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

static inline void lib_NMSACVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW, vd , i, data3);

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
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vd , i, data2);
        buildDataBin(iss, SEW, vs2, i, data3);

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
        if(!sgn){
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

static inline void lib_NMSUBVX  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64], data3[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3Ext[SEW*2-1]){
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        buildDataBin(iss, SEW  , vs1, i, data1);
        buildDataBin(iss, SEW  , vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

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
        binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCUVX (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);
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
        binSum2(SEW*2, mulOutBin, data3Ext, resBin);
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_WMACCUSVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        if(data2[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data2);
        }
        if(data3Ext[SEW*2-1]){
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;
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
        binMulSumUp(SEW, M0, M1, M2, M3, mulOutBin, false);

        if(sgn == 1){
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW  , vs1, i, data1);
        buildDataBin(iss, SEW  , vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);

        if(data1[SEW-1]){
            sgn = !sgn;
            twosComplement(SEW,data1);
        }
        if(data3Ext[SEW*2-1]){
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;
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
    bool data1[64], data2[64];
    bool data1H[32], data1L[32], data2H[32], data2L[32];
    bool M0[64], M1[64], M2[64], M3[64];
    bool bin[8];
    bool resBin[128], mulOutBin[128], data3Ext[128];
    bool sgn = 0;
    bool vdSgn = 0;
    intToBin(64, abs((int64_t)rs1), data1);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);
        buildDataBin(iss, SEW*2, vd , i, data3Ext);
        if(data3Ext[SEW*2-1]){
            twosComplement(SEW*2,data3Ext);
            vdSgn = !vdSgn;
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
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_REDSUMVS (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    myAbs(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            myAbs(iss, SEW, vs2, i, &data2);
            res += data2;
        }
    }
    intToBin(SEW, abs(res), resBin);
    if(res < 0){
        twosComplement(SEW, resBin);
    }
    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_REDANDVS (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    myAbs(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            myAbs(iss, SEW, vs2, i, &data2);
            res &= data2;
        }
    }
    intToBin(SEW, abs(res), resBin);
    if(res < 0){
        twosComplement(SEW, resBin);
    }
    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_REDORVS  (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    myAbs(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        if(!mask(vm,bin)){
            myAbs(iss, SEW, vs2, i, &data2);
            res |= data2;
        }
    }
    intToBin(SEW, abs(res), resBin);
    if(res < 0){
        twosComplement(SEW, resBin);
    }
    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_REDXORVS (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    myAbs(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            myAbs(iss, SEW, vs2, i, &data2);
            res ^= data2;
        }
    }
    intToBin(SEW, abs(res), resBin);
    if(res < 0){
        twosComplement(SEW, resBin);
    }
    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_REDMINVS (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    myAbs(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            myAbs(iss, SEW, vs2, i, &data2);
            res = (res < data2)?res:data2;
        }
    }
    intToBin(SEW, abs(res), resBin);
    if(res < 0){
        twosComplement(SEW, resBin);
    }
    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_REDMINUVS(Iss *iss, int vs1, int vs2    , int vd, bool vm){
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    myAbsU(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            myAbsU(iss, SEW, vs2, i, &data2);
            res = (res < data2)?res:data2;
        }
    }
    intToBinU(SEW, res, resBin);
    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_REDMAXVS (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    myAbs(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            myAbs(iss, SEW, vs2, i, &data2);
            res = (res > data2)?res:data2;
        }
    }
    intToBin(SEW, abs(res), resBin);
    if(res < 0){
        twosComplement(SEW, resBin);
    }
    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_REDMAXUVS(Iss *iss, int vs1, int vs2    , int vd, bool vm){
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];
    myAbsU(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            myAbsU(iss, SEW, vs2, i, &data2);
            res = (res > data2)?res:data2;
        }
    }
    intToBinU(SEW, res, resBin);
    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_SLIDEUPVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){//VMA and VTA should be checked
    int t = SEW/8;
    int64_t OFFSET;
    bool bin[8];

    OFFSET = rs1;
    int s = MAX(VSTART,OFFSET);

    for (int i = s; i < VL; i++){
        intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);

        if(!mask(vm,bin)){
            for(int j = 0; j <  t ; j++){
                iss->vector.vregs[vd][i*t+j] = iss->vector.vregs[vs2][(i-OFFSET)*t+j];
            }
        }
    }
}

static inline void lib_SLIDEUPVI(Iss *iss, int vs2, int64_t sim, int vd, bool vm){//VMA and VTA should be checked
    int t = SEW/8;
    int64_t OFFSET;
    bool bin[8];
    OFFSET = sim;
    int s = MAX(VSTART,OFFSET);

    for (int i = s; i < VL; i++){
        intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        if(!mask(vm,bin)){
            for(int j = 0; j <  t ; j++){
                iss->vector.vregs[vd][i*t+j] = iss->vector.vregs[vs2][(i-OFFSET)*t+j];
            }
        }
    }
}

static inline void lib_SLIDEDWVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){//VMA and VTA should be checked
    int t = SEW/8;
    int64_t OFFSET;
    bool bin[8];
    OFFSET = rs1;
    int s = MAX(VSTART,OFFSET);

    for (int i = VSTART; i < VL; i++){
        if(i+OFFSET >= VLMAX){
            for(int j = 0; j <  t ; j++){
                iss->vector.vregs[vd][i*t+j] = 0;
            }
        }else{
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);

            if(!mask(vm,bin)){
                for(int j = 0; j <  t ; j++){
                    iss->vector.vregs[vd][i*t+j] = iss->vector.vregs[vs2][(i+OFFSET)*t+j];
                }
            }
        }
    }
}

static inline void lib_SLIDEDWVI(Iss *iss, int vs2, int64_t sim, int vd, bool vm){//VMA and VTA should be checked
    int t = SEW/8;
    int64_t OFFSET;
    bool bin[8];
    OFFSET = sim;

    int s = MAX(VSTART,OFFSET);

    for (int i = VSTART; i < VL; i++){
        if(i+OFFSET >= VLMAX){
            for(int j = 0; j <  t ; j++){
                iss->vector.vregs[vd][i*t+j] = 0;
            }
        }else{
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);

            if(!mask(vm,bin)){
                for(int j = 0; j <  t ; j++){
                    iss->vector.vregs[vd][i*t+j] = iss->vector.vregs[vs2][(i+OFFSET)*t+j];
                }
            }
        }
    }
}

static inline void lib_SLIDE1UVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){//VMA and VTA should be checked
    int t = SEW/8;
    bool bin[8];
    bool data1[64];

    int s = MAX(VSTART,1);
    intToBin(8,(int64_t) iss->vector.vregs[0][0],bin);

    int i = 0;
    intToBin(64, abs((int64_t)rs1), data1);
    if(rs1 < 0){
        twosComplement(64,data1);
    }
    if(VSTART == 0){
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, 0, data1);
        }
    }
    for (int i = s; i < VL; i++){
        if(!((i-s)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i-s)/8],bin);
        }

        if(!mask(vm,bin)){
            for(int j = 0; j <  t ; j++){
                iss->vector.vregs[vd][i*t+j] = iss->vector.vregs[vs2][(i-1)*t+j];
            }
        }
    }
}

static inline void lib_SLIDE1DVX(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){//VMA and VTA should be checked
    int t = SEW/8;
    bool bin[8];
    bool data1[64];

    intToBin(8,(int64_t) iss->vector.vregs[0][0],bin);

    int i = 0;
    intToBin(64, abs((int64_t)rs1), data1);
    if(rs1 < 0){
        twosComplement(64,data1);
    }
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        if(!mask(vm,bin)){
            if(i == VL-1){
                writeToVReg(iss, SEW, vd, VL-1, data1);
            }else{
                for(int j = 0; j <  t ; j++){
                    iss->vector.vregs[vd][i*t+j] = iss->vector.vregs[vs2][(i+1)*t+j];
                }
            }
        }
    }
}

static inline void lib_DIVVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);

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

static inline void lib_DIVVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);

        res = data2/data1;

        intToBin(SEW, res, resBin);
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_DIVUVX   (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    uint64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);

        res = data2/data1;


        intToBin(SEW, res, resBin);

        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_REMVV    (Iss *iss, int vs1, int vs2    , int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs1, i, &data1);
        myAbs(iss, SEW, vs2, i, &data2);

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

static inline void lib_REMVX    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    int64_t data1, data2, res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data2);

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
    uint64_t data1, data2;
    int64_t res;
    bool bin[8];
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);

        res = data2%data1;
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
    uint64_t data1, data2;
    int64_t res;
    bool bin[8];
    bool resBin[64];

    data1 = rs1;

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);

        res = data2%data1;


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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(ff_add, data1, data2, e, m, res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(ff_add, data1, data2, e, m, res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(ff_sub, data2, data1, e, m, res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(ff_sub, data2, data1, e, m, res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(ff_sub, data1, data2, e, m, res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(ff_min, data2, data1, e, m, res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(ff_min, data2, data1, e, m, res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(ff_max, data2, data1, e, m, res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(ff_max, data2, data1, e, m, res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(ff_mul, data2, data1, e, m, res);


            // printf("data1 = %f\tdata2 =%f\tres = %f\n",ff_a.value,ff_b.value,ff_res.value);

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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_2(ff_mul, data2, data1, e, m, res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(ff_fma, data2, data1, data3, e, m, res);
            restoreFFRoundingMode(old);

            intToBinU(SEW, res, resBin);

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMACCVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_EXEC_3(ff_fma, data1, data2, data3, e, m, res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);

            FLOAT_INIT_3(data2, data1, data3, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_a.value = -ff_a.value;
            ff_c.value = -ff_c.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);

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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);

            FLOAT_INIT_3(data1, data2, data3, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_a.value = -ff_a.value;
            ff_c.value = -ff_c.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);

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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data2, data1, data3, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_c.value = -ff_c.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data2, data3, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_c.value = -ff_c.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data2, data3, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_a.value = -ff_a.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data2, data3, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_a.value = -ff_a.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data3, data2, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data3, data2, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data3, data2, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_a.value = -ff_a.value;
            ff_c.value = -ff_c.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data3, data2, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_a.value = -ff_a.value;
            ff_c.value = -ff_c.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data3, data2, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_c.value = -ff_c.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data3, data2, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_c.value = -ff_c.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){

            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data3, data2, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_a.value = -ff_a.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
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
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data2);
        myAbsU(iss, SEW, vd , i, &data3);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            FLOAT_INIT_3(data1, data3, data2, e, m)
            feclearexcept(FE_ALL_EXCEPT);
            ff_a.value = -ff_a.value;
            ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FREDMAXVS(Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    myAbsU(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(ff_max, data2, res, e, m, res);
        restoreFFRoundingMode(old);
        }
    }
    intToBinU(SEW, res, resBin);

    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_FREDMINVS(Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    myAbsU(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(ff_min, data2, res, e, m, res);
        restoreFFRoundingMode(old);
        }
    }
    intToBinU(SEW, res, resBin);

    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_FREDSUMVS(Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    bool resBin[64];
    myAbsU(iss, SEW, vs1, 0, &data1);
    res = data1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        myAbsU(iss, SEW, vs2, i, &data2);
        EMCase(SEW, &m, &e);

        if(!mask(vm,bin)){
        int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
        FLOAT_EXEC_2(ff_add, data2, res, e, m, res);
        // printf("res = %f\n",ff_res.value);
        restoreFFRoundingMode(old);
        }
    }
    intToBinU(SEW, res, resBin);

    writeToVReg(iss, SEW, vd, 0, resBin);
}

static inline void lib_FWADDVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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

static inline void lib_FWADDVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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

static inline void lib_FWADDWV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW*2, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env2);
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

static inline void lib_FWADDWF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW*2, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);
        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_b, env2);
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

static inline void lib_FWSUBVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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

static inline void lib_FWSUBVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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

static inline void lib_FWSUBWV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs1, i, &data1);
        myAbsU(iss, SEW*2, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env2);
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

static inline void lib_FWSUBWF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW*2, vs2, i, &data2);
        EMCase(SEW, &m, &e);
        EMCase(SEW*2, &m2, &e2);

        if(!mask(vm,bin)){
            int old = setFFRoundingMode(iss, iss->csr.fcsr.frm);
            flexfloat_t ff_a, ff_b, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env2);
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

static inline void lib_FWMULVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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

static inline void lib_FWMULVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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

static inline void lib_FWMACCVV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            ff_macc(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWMACCVF (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            ff_macc(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWMSACVV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            ff_msac(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWMSACVF (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            ff_msac(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWNMSACVV(Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            ff_nmsac(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FWNMSACVF(Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];
    unsigned long int res, data1, data2, data3;
    uint8_t e, m;
    uint8_t e2, m2;
    bool resBin[64];
    data1 = rs1;
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
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
            ff_nmsac(&ff_res, &ff_a, &ff_b, &ff_c);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            restoreFFRoundingMode(old);
            intToBinU(SEW*2, res, resBin);
            writeToVReg(iss, SEW*2, vd, i, resBin);
        }
    }
}

static inline void lib_FSGNJVV  (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    bool data1[64], data2[64], data3[64];

    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        if(!mask(vm,bin)){
            resBin[SEW-1] = data1[SEW-1];
            for(int j = 0; j < SEW-1; j++){
                resBin[j] = data2[j];
            }

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FSGNJVF  (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];

    bool data1[64], data2[64], data3[64];

    uint8_t e, m;
    bool resBin[64];
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        rs1 = (SEW == 64)?rs1:rs1 & (long int)pow(2,SEW)-1;

        intToBin(SEW, abs((int64_t)rs1), data1);

        if(!mask(vm,bin)){
            resBin[SEW-1] = (SEW == 64)? (rs1 < 0) : (data1[SEW-1]);

            for(int j = 0; j < SEW-1; j++){
                resBin[j] = data2[j];
            }

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FSGNJNVV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    bool data1[64], data2[64], data3[64];

    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        if(!mask(vm,bin)){
            resBin[SEW-1] = !data1[SEW-1];
            for(int j = 0; j < SEW-1; j++){
                resBin[j] = data2[j];
            }

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FSGNJNVF (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];

    bool data1[64], data2[64], data3[64];

    uint8_t e, m;
    bool resBin[64];
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        rs1 = (SEW == 64)?rs1:rs1 & (long int)pow(2,SEW)-1;

        intToBin(SEW, abs((int64_t)rs1), data1);

        if(!mask(vm,bin)){
            resBin[SEW-1] = (SEW == 64)? !(rs1 < 0) : !(data1[SEW-1]);

            for(int j = 0; j < SEW-1; j++){
                resBin[j] = data2[j];
            }

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FSGNJXVV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    bool data1[64], data2[64], data3[64];

    uint8_t e, m;
    bool resBin[64];

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs1, i, data1);
        buildDataBin(iss, SEW, vs2, i, data2);

        if(!mask(vm,bin)){
            resBin[SEW-1] = data1[SEW-1] ^ data2[SEW-1];
            for(int j = 0; j < SEW-1; j++){
                resBin[j] = data2[j];
            }

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FSGNJXVF (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool bin[8];

    bool data1[64], data2[64], data3[64];

    uint8_t e, m;
    bool resBin[64];
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        buildDataBin(iss, SEW, vs2, i, data2);

        rs1 = (SEW == 64)?rs1:rs1 & (long int)pow(2,SEW)-1;

        intToBin(SEW, rs1, data1);

        if(!mask(vm,bin)){
            resBin[SEW-1] = (SEW == 64)? ((rs1 < 0) ^ (data2[SEW-1])):(data1[SEW-1]) ^ (data2[SEW-1]);

            for(int j = 0; j < SEW-1; j++){
                resBin[j] = data2[j];
            }
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FCVTXUFV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    uint64_t res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            flexfloat_set_bits(&ff_a, data1);
            ff_a.value = (ff_a.value > 0)?(double)ff_a.value:0;
            res = round(ff_a.value);
            intToBin(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FCVTXFV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    int64_t res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            flexfloat_set_bits(&ff_a, data1);
            res = round(ff_a.value);
            intToBin(SEW, abs(res), resBin);
            if(res < 0){
                twosComplement(SEW, resBin);
            }

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FCVTFXUV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    uint64_t data1;
    unsigned long int res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            ff_a.value = (data1 > 0)?(data1):0;
            flexfloat_sanitize(&ff_a);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_a);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FCVTFXV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    unsigned long int res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            ff_a.value = data1;
            flexfloat_sanitize(&ff_a);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_a);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FCVTRTZXUFV(Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    uint64_t res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            flexfloat_set_bits(&ff_a, data1);
            ff_a.value = (ff_a.value > 0)?(double)ff_a.value:0;
            res = trunc(ff_a.value);
            intToBin(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FCVTRTZXFV (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    int64_t res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            flexfloat_set_bits(&ff_a, data1);
            res = trunc(ff_a.value);
            intToBin(SEW, abs(res), resBin);
            if(res < 0){
                twosComplement(SEW, resBin);
            }

            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNCVTXUFW (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    uint64_t data1;
    uint64_t res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(2*SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, 2*SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            flexfloat_set_bits(&ff_a, data1);
            ff_a.value = (ff_a.value > 0)?(double)ff_a.value:0;
            res = round(ff_a.value);
            if(res > pow(2,SEW)){
                res = pow(2,SEW)-1;
            }
            intToBin(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNCVTXFW (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    int64_t res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(2*SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, 2*SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            flexfloat_set_bits(&ff_a, data1);
            res = round(ff_a.value);
            if(res >= pow(2,SEW-1)){
                res = pow(2,SEW-1)-1;
            }else if(abs(res) > pow(2,SEW-1)){
                res = -pow(2,SEW-1);
            }
            intToBin(SEW, abs(res), resBin);
            if(res < 0){
                twosComplement(SEW, resBin);
            }
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNCVTFXUW (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    uint64_t data1;
    unsigned long int res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, 2*SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            ff_a.value = (data1 > 0)?(data1):0;
            flexfloat_sanitize(&ff_a);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_a);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNCVTFXW (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    unsigned long int res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, 2*SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            ff_a.value = data1;
            flexfloat_sanitize(&ff_a);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_a);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNCVTFFW (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    unsigned long int res;
    uint8_t e, m;
    uint8_t e2, m2;

    bool resBin[64];
    EMCase(SEW, &m, &e);
    EMCase(2*SEW, &m2, &e2);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, 2*SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env2);
            ff_init(&ff_res, env);
            flexfloat_set_bits(&ff_a, data1);
            ff_res.value = ff_a.value;
            flexfloat_sanitize(&ff_res);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            intToBinU(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNCVTRODFFW (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    int64_t dataG;
    int64_t dataL;
    unsigned long int res;
    uint8_t e, m;
    uint8_t e2, m2;

    bool resBinG[64];
    bool resBinL[64];
    bool resBin[64];
    EMCase(SEW, &m, &e);
    EMCase(2*SEW, &m2, &e2);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, 2*SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env2);
            ff_init(&ff_res, env);
            flexfloat_set_bits(&ff_a, data1);

            ff_res.value = ff_a.value;
            flexfloat_sanitize(&ff_res);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);

            intToBinU(SEW, res, resBinG);
            intToBinU(SEW, res, resBinL);
            intToBinU(SEW, res, resBin);

            if(resBinG[0] == 0){
                resBinG[0] = 1;
            }
            if(resBinL[0] == 0){
                for(int j = 0; j < m+e; j++){
                    if(resBinL[j] == 0){
                        resBinL[j] = 1;
                    }else{
                        resBinL[j] = 0;
                        break;
                    }
                }
            }
            binToInt(SEW, resBinG, &dataG);
            binToInt(SEW, resBinL, &dataL);
            flexfloat_t ff_G, ff_L;
            ff_init(&ff_G, env);
            ff_init(&ff_L, env);
            flexfloat_set_bits(&ff_G, dataG);
            flexfloat_set_bits(&ff_L, dataL);

            // printf("res = %lx\ta = %lx\n",ff_res.value,ff_a.value);
            // printf("res = %f\ta = %f\n",ff_res.value,ff_a.value);

            if(ff_res.value == ff_a.value){
                writeToVReg(iss, SEW, vd, i, resBin);
            }else{

                if(abs(ff_a.value - ff_G.value) > abs(ff_a.value - ff_L.value)){
                    writeToVReg(iss, SEW, vd, i, resBinL);
                }else{
                    writeToVReg(iss, SEW, vd, i, resBinG);
                }
            }
        }
    }
}

static inline void lib_FNCVTRTZXUFW(Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    int64_t data1;
    uint64_t res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(2*SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbs(iss, 2*SEW, vs2, i, &data1);
        data1 = (data1 > 0)?data1:0;
        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            flexfloat_set_bits(&ff_a, data1);
            res = trunc(ff_a.value);
            intToBin(SEW, res, resBin);
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FNCVTRTZXFW (Iss *iss, int vs1,     int vs2, int vd, bool vm){
    bool bin[8];
    uint64_t data1;
    int64_t res;
    uint8_t e, m;

    bool resBin[64];
    EMCase(2*SEW, &m, &e);

    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        myAbsU(iss, 2*SEW, vs2, i, &data1);

        if(!mask(vm,bin)){
            flexfloat_t ff_a;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            ff_init(&ff_a, env);
            flexfloat_set_bits(&ff_a, data1);
            res = trunc(ff_a.value);
            intToBin(SEW, abs(res), resBin);
            if(res < 0){
                twosComplement(SEW, resBin);
            }
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMVVF    (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    unsigned long int res;
    bool bin[8];
    bool resBin[64];
    for (int i = VSTART; i < VL; i++){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        res = rs1;
        intToBin(SEW, res, resBin);
        if(rs1 < 0){
            twosComplement(SEW, resBin);
        }
        if(!mask(vm,bin)){
            writeToVReg(iss, SEW, vd, i, resBin);
        }
    }
}

static inline void lib_FMVSF     (Iss *iss, int vs2, int64_t rs1, int vd, bool vm){
    bool resBin[64];
    unsigned long int res;
    res = rs1;
    if(VSTART < VL){
        intToBin(SEW, res, resBin);
        if(rs1 < 0){
            twosComplement(SEW, resBin);
        }
        if(vm){
            writeToVReg(iss, SEW, vd, 0, resBin);
        }else{
            printf("MVSX VM=0 is RESERVED\n");
        }
    }
}


static inline iss_freg_t lib_FMVFS     (Iss *iss, int vs2, bool vm){
    unsigned int res;
    int64_t data1;
    myAbs(iss, SEW, vs2, 0, &data1);
    // printf("data1 = %lx\n",data1);

    uint8_t e, m;
    uint8_t e2, m2;
    EMCase(64, &m, &e);
    EMCase(32, &m2, &e2);


    if(sizeof(iss_freg_t) == 4){
        printf("REG is 32\n");
            flexfloat_t ff_a, ff_res;
            flexfloat_desc_t env = (flexfloat_desc_t){e, m};
            flexfloat_desc_t env2 = (flexfloat_desc_t){e2, m2};
            ff_init(&ff_a, env);
            ff_init(&ff_res, env2);
            flexfloat_set_bits(&ff_a, data1);
            ff_res.value = ff_a.value;
            flexfloat_sanitize(&ff_res);
            update_fflags_fenv(iss);
            res = flexfloat_get_bits(&ff_res);
            return iss_freg_t(res);
    }else{
        printf("REG is 64\n");
        return iss_freg_t(data1);
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
        vp::IoReq *req = &this->io_req;

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

static inline void lib_VLE8V (Iss *iss, iss_reg_t rs1, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    // printf("VLE8\n");
    // printf("vd = %d\n",vd);
    // printf("VSTART = %ld\n",VSTART);
    // printf("vl = %ld\n",VL);
    // printf("RS1 = %lx\n",rs1);


    int align = 0;
    if(start_add%4 == 1){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];
        iss->vector.vregs[vd][VSTART+1] = data[1];
        iss->vector.vregs[vd][VSTART+2] = data[2];

        start_add += 3;
        align = 3;
    }else if(start_add%4 == 2){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];
        iss->vector.vregs[vd][VSTART+1] = data[1];

        start_add += 2;
        align = 2;
    }else if(start_add%4 == 3){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];

        start_add += 1;
        align = 1;
    }else{
        align = 0;
    }







    for (int i = VSTART+align; i < VL; i+=4){
 //       if(!i){
 //           printf("Vlsu_io_access\n");
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
 //       }else{
 //           printf("handle_pending_io_access\n");
 //           vlsu.handle_pending_io_access(iss);
 //       }

        // printf("data0 = %x\n",data[0]);
        // printf("data1 = %x\n",data[1]);
        // printf("data2 = %x\n",data[2]);
        // printf("data3 = %x\n",data[3]);

        iss->vector.vregs[vd][i+0] = data[0];
        iss->vector.vregs[vd][i+1] = data[1];
        iss->vector.vregs[vd][i+2] = data[2];
        iss->vector.vregs[vd][i+3] = data[3];

        start_add += 4;
    }
}

static inline void lib_VLE16V(Iss *iss, iss_reg_t rs1, int vd , bool vm){
    // printf("LIB_VLE16V\n");
    // printf("VSTART = %d\n",VSTART);
    // printf("vl = %ld\n",vl);
    // printf("VLEN/8 = %d\n",vlEN/8);
    // printf("VM = %d\n",vm);
    // printf("RS1 = %lx\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];



    int align = 0;
    if(start_add%4 == 1){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];
        iss->vector.vregs[vd][VSTART+1] = data[1];
        iss->vector.vregs[vd][VSTART+2] = data[2];

        start_add += 3;
        align = 3;
    }else if(start_add%4 == 2){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];
        iss->vector.vregs[vd][VSTART+1] = data[1];

        start_add += 2;
        align = 2;
    }else if(start_add%4 == 3){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];

        start_add += 1;
        align = 1;
    }else{
        align = 0;
    }


    for (int i = VSTART+align; i < VL*2; i+=4){
        //if(!i){
            //vlsu.Vlsu_io_access(iss, rs1, vlEN/8, data, false);
            iss->vector.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    vlsu.handle_pending_io_access(iss);
        //}

        // printf("data0 = %d\n",data[0]);
        // printf("data1 = %d\n",data[1]);
        // printf("data2 = %d\n",data[2]);
        // printf("data3 = %d\n",data[3]);

        // printf("vd = %d\n",vd);

        //iss->vector.vregs[vd][i+0] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? (data[1]*pow(2,8) + data[0]) : iss->vector.vregs[vd][i+0];
        //iss->vector.vregs[vd][i+1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? (data[3]*pow(2,8) + data[2]) : iss->vector.vregs[vd][i+1];

        // iss->vector.vregs[vd][i+0] = (data[1]*pow(2,8) + data[0]);
        // iss->vector.vregs[vd][i+1] = (data[3]*pow(2,8) + data[2]);

        iss->vector.vregs[vd][i+0] = data[0];
        iss->vector.vregs[vd][i+1] = data[1];
        iss->vector.vregs[vd][i+2] = data[2];
        iss->vector.vregs[vd][i+3] = data[3];

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
    // printf("RS1 = %lx\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];

    int align = 0;
    if(start_add%4 == 1){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];
        iss->vector.vregs[vd][VSTART+1] = data[1];
        iss->vector.vregs[vd][VSTART+2] = data[2];

        start_add += 3;
        align = 3;
    }else if(start_add%4 == 2){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];
        iss->vector.vregs[vd][VSTART+1] = data[1];

        start_add += 2;
        align = 2;
    }else if(start_add%4 == 3){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];

        start_add += 1;
        align = 1;
    }else{
        align = 0;
    }





    for (int i = VSTART+align; i < VL*4; i+=4){
        //if(!i){
            //vlsu.Vlsu_io_access(iss, rs1, vlEN/8, data, false);
            iss->vector.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    vlsu.handle_pending_io_access(iss);
        //}

        // printf("data0 = %d\n",data[0]);
        // printf("data1 = %d\n",data[1]);
        // printf("data2 = %d\n",data[2]);
        // printf("data3 = %d\n",data[3]);

        //printf("vd = %d\n",vd);

        //iss->vector.vregs[vd][i+0] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? (data[1]*pow(2,8) + data[0]) : iss->vector.vregs[vd][i+0];
        //iss->vector.vregs[vd][i+1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? (data[3]*pow(2,8) + data[2]) : iss->vector.vregs[vd][i+1];

        // iss->vector.vregs[vd][i+0] = (data[1]*pow(2,8) + data[0]);
        // iss->vector.vregs[vd][i+1] = (data[3]*pow(2,8) + data[2]);

        iss->vector.vregs[vd][i+0] = data[0];
        iss->vector.vregs[vd][i+1] = data[1];
        iss->vector.vregs[vd][i+2] = data[2];
        iss->vector.vregs[vd][i+3] = data[3];

        // printf("vd0 = %d\n",iss->vector.vregs[vd][i+0]);
        // printf("vd1 = %d\n",iss->vector.vregs[vd][i+1]);
        // printf("vd2 = %d\n",iss->vector.vregs[vd][i+2]);
        // printf("vd3 = %d\n",iss->vector.vregs[vd][i+3]);


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
    // printf("RS1 = %lx\n",rs1);
    // printf("vd = %d\n",vd);

    uint64_t start_add = rs1;
    uint8_t data[4];

    int align = 0;
    if(start_add%4 == 1){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];
        iss->vector.vregs[vd][VSTART+1] = data[1];
        iss->vector.vregs[vd][VSTART+2] = data[2];

        start_add += 3;
        align = 3;
    }else if(start_add%4 == 2){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];
        iss->vector.vregs[vd][VSTART+1] = data[1];

        start_add += 2;
        align = 2;
    }else if(start_add%4 == 3){
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
        iss->vector.vregs[vd][VSTART+0] = data[0];

        start_add += 1;
        align = 1;
    }else{
        align = 0;
    }


    for (int i = VSTART+align; i < VL*8; i+=4){
        //if(!i){
            //vlsu.Vlsu_io_access(iss, rs1, vlEN/8, data, false);
            iss->vector.vlsu.Vlsu_io_access(iss, start_add, 4, data, false);
        //}else{
        //    vlsu.handle_pending_io_access(iss);
        //}
        // printf("LIB_VLE64V\n");

        // printf("data0 = %d\n",data[0]);
        // printf("data1 = %d\n",data[1]);
        // printf("data2 = %d\n",data[2]);
        // printf("data3 = %d\n",data[3]);

        //printf("vd = %d\n",vd);

        //iss->vector.vregs[vd][i+0] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? (data[1]*pow(2,8) + data[0]) : iss->vector.vregs[vd][i+0];
        //iss->vector.vregs[vd][i+1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? (data[3]*pow(2,8) + data[2]) : iss->vector.vregs[vd][i+1];

        // iss->vector.vregs[vd][i+0] = (data[1]*pow(2,8) + data[0]);
        // iss->vector.vregs[vd][i+1] = (data[3]*pow(2,8) + data[2]);

        iss->vector.vregs[vd][i+0] = data[0];
        iss->vector.vregs[vd][i+1] = data[1];
        iss->vector.vregs[vd][i+2] = data[2];
        iss->vector.vregs[vd][i+3] = data[3];

        // printf("vd0 = %d\n",iss->vector.vregs[vd][i+0]);
        // printf("vd1 = %d\n",iss->vector.vregs[vd][i+1]);
        // printf("vd2 = %d\n",iss->vector.vregs[vd][i+2]);
        // printf("vd3 = %d\n",iss->vector.vregs[vd][i+3]);


        // printf("vd_vali = %d\n",(int)(data[3]*pow(2,8*3) + data[2]*pow(2,8*2) + data[1]*pow(2,8) + data[0]));
        start_add += 4;
    }
}

static inline void lib_VSE8V (Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    uint8_t data[4];
    //printf("rs1  = %lu\n",rs1);
    // printf("vd  = %d\n",vs3);

    uint64_t start_add = rs1;



    int align = 0;
    if(start_add%4 == 1){

        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        data[1]  = iss->vector.vregs[vs3][VSTART+1];
        data[2]  = iss->vector.vregs[vs3][VSTART+2];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 3;
        align = 3;
    }else if(start_add%4 == 2){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        data[1]  = iss->vector.vregs[vs3][VSTART+1];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 2;
        align = 2;
    }else if(start_add%4 == 3){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 1;
        align = 1;
    }else{
        align = 0;
    }




    for (int i = VSTART+align; i < VL; i+=4){
/*
        data[0] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1] : 0;
        data[2] = (vm || !(iss->vector.vregs[0][i+2]%2)) ? iss->vector.vregs[vs3][i+2] : 0;
        data[3] = (vm || !(iss->vector.vregs[0][i+3]%2)) ? iss->vector.vregs[vs3][i+3] : 0;
*/
        // data[3] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0] : 0;
        // data[2] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1] : 0;
        // data[1] = (vm || !(iss->vector.vregs[0][i+2]%2)) ? iss->vector.vregs[vs3][i+2] : 0;
        // data[0] = (vm || !(iss->vector.vregs[0][i+3]%2)) ? iss->vector.vregs[vs3][i+3] : 0;

        data[0]  = iss->vector.vregs[vs3][i+0];
        data[1]  = iss->vector.vregs[vs3][i+1];
        data[2]  = iss->vector.vregs[vs3][i+2];
        data[3]  = iss->vector.vregs[vs3][i+3];


        // printf("STORE8 \n");
        // printf("data0  = %d\n",data[0 ]);
        // printf("data1  = %d\n",data[1 ]);
        // printf("data2  = %d\n",data[2 ]);
        // printf("data3  = %d\n",data[3 ]);

        // printf("addr  = %lu\n",start_add);

        //if(!i){
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
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

    int align = 0;
    if(start_add%4 == 1){

        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        data[1]  = iss->vector.vregs[vs3][VSTART+1];
        data[2]  = iss->vector.vregs[vs3][VSTART+2];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 3;
        align = 3;
    }else if(start_add%4 == 2){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        data[1]  = iss->vector.vregs[vs3][VSTART+1];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 2;
        align = 2;
    }else if(start_add%4 == 3){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 1;
        align = 1;
    }else{
        align = 0;
    }

    for (int i = VSTART+align; i < VL*2; i+=4){
        /*
        data[0] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1] : 0;

        // if(!i){
        //     vlsu.Vlsu_io_access(iss, rs1,vlEN/8,data,true);
        // }else{
        //     vlsu.handle_pending_io_access(iss);
        // }

        data[0]  = iss->vector.vregs[vs3][i+0];
        data[1]  = iss->vector.vregs[vs3][i+1];
        data[2]  = iss->vector.vregs[vs3][i+2];
        data[3]  = iss->vector.vregs[vs3][i+3];
        // printf("i = %d\t,vd = %d\n",i,vs3);

        // printf("STORE16 \n");
        // printf("data0  = %d\n",data[0]);
        // printf("data1  = %d\n",data[1]);
        // printf("data2  = %d\n",data[2]);
        // printf("data3  = %d\n",data[3]);

        // printf("addr  = %lu\n",start_add);

        //if(!i){
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
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

    int align = 0;
    if(start_add%4 == 1){

        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        data[1]  = iss->vector.vregs[vs3][VSTART+1];
        data[2]  = iss->vector.vregs[vs3][VSTART+2];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 3;
        align = 3;
    }else if(start_add%4 == 2){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        data[1]  = iss->vector.vregs[vs3][VSTART+1];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 2;
        align = 2;
    }else if(start_add%4 == 3){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 1;
        align = 1;
    }else{
        align = 0;
    }



    for (int i = VSTART+align; i < VL*4; i+=4){
        /*
        data[0] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1] : 0;

        // if(!i){
        //     vlsu.Vlsu_io_access(iss, rs1,vlEN/8,data,true);
        // }else{
        //     vlsu.handle_pending_io_access(iss);
        // }

        data[0]  = iss->vector.vregs[vs3][i+0];
        data[1]  = iss->vector.vregs[vs3][i+1];
        data[2]  = iss->vector.vregs[vs3][i+2];
        data[3]  = iss->vector.vregs[vs3][i+3];


        // printf("STORE32 \n");
        // printf("data0  = %d\n",data[0]);
        // printf("data1  = %d\n",data[1]);
        // printf("data2  = %d\n",data[2]);
        // printf("data3  = %d\n",data[3]);

        // printf("addr  = %lu\n",start_add);

        //if(!i){
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
        //}else{
        //    vlsu.handle_pending_io_access(iss);
        //}
        start_add += 4;
    }
}

static inline void lib_VSE64V(Iss *iss, iss_reg_t rs1, int vs3, bool vm){
    // uint8_t data[vl];
    // uint32_t temp;
    // for (int i = VSTART; i < VL*2; i+=1){
    //     if(i%2){
    //         temp = iss->vector.vregs[vs3][i+0]/pow(2,8*4);
    //         data[3] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? temp/pow(2,8*3) : 0;
    //         data[2] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? temp/pow(2,8*2) : 0;
    //         data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? temp/pow(2,8*1) : 0;
    //         data[0] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? temp/pow(2,8*0) : 0;
    //     }else{
    //         temp = iss->vector.vregs[vs3][i+0];
    //         data[3] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? temp/pow(2,8*3) : 0;
    //         data[2] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? temp/pow(2,8*2) : 0;
    //         data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? temp/pow(2,8*1) : 0;
    //         data[0] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? temp/pow(2,8*0) : 0;
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


    int align = 0;
    if(start_add%4 == 1){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        data[1]  = iss->vector.vregs[vs3][VSTART+1];
        data[2]  = iss->vector.vregs[vs3][VSTART+2];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 3;
        align = 3;
    }else if(start_add%4 == 2){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        data[1]  = iss->vector.vregs[vs3][VSTART+1];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 2;
        align = 2;
    }else if(start_add%4 == 3){
        data[0]  = iss->vector.vregs[vs3][VSTART+0];
        iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);

        start_add += 1;
        align = 1;
    }else{
        align = 0;
    }

    for (int i = VSTART+align; i < VL*8; i+=4){
        /*
        data[0] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0] : 0;
        data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1] : 0;
        */

        // data[3] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0]/pow(2,8) : 0;
        // data[2] = (vm || !(iss->vector.vregs[0][i+0]%2)) ? iss->vector.vregs[vs3][i+0] : 0;
        // data[1] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1]/pow(2,8) : 0;
        // data[0] = (vm || !(iss->vector.vregs[0][i+1]%2)) ? iss->vector.vregs[vs3][i+1] : 0;

        // if(!i){
        //     vlsu.Vlsu_io_access(iss, rs1,vlEN/8,data,true);
        // }else{
        //     vlsu.handle_pending_io_access(iss);
        // }

        data[0]  = iss->vector.vregs[vs3][i+0];
        data[1]  = iss->vector.vregs[vs3][i+1];
        data[2]  = iss->vector.vregs[vs3][i+2];
        data[3]  = iss->vector.vregs[vs3][i+3];


        // printf("STORE64 \n");
        // printf("data0  = %d\n",data[0]);
        // printf("data1  = %d\n",data[1]);
        // printf("data2  = %d\n",data[2]);
        // printf("data3  = %d\n",data[3]);

        // printf("addr  = %lu\n",start_add);

        //if(!i){
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
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
                iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);


            // printf("data0 = %d\n",data[0]);
            // printf("data1 = %d\n",data[1]);
            // printf("data2 = %d\n",data[2]);
            // printf("data3 = %d\n",data[3]);

            iss->vector.vregs[vd+k][i+0] = data[0];
            iss->vector.vregs[vd+k][i+1] = data[1];
            iss->vector.vregs[vd+k][i+2] = data[2];
            iss->vector.vregs[vd+k][i+3] = data[3];

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
                iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);


            // printf("data0 = %d\n",data[0]);
            // printf("data1 = %d\n",data[1]);
            // printf("data2 = %d\n",data[2]);
            // printf("data3 = %d\n",data[3]);

            iss->vector.vregs[vd+k][i+0] = data[0];
            iss->vector.vregs[vd+k][i+1] = data[1];
            iss->vector.vregs[vd+k][i+2] = data[2];
            iss->vector.vregs[vd+k][i+3] = data[3];

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
                iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);


            // printf("data0 = %d\n",data[0]);
            // printf("data1 = %d\n",data[1]);
            // printf("data2 = %d\n",data[2]);
            // printf("data3 = %d\n",data[3]);

            iss->vector.vregs[vd+k][i+0] = data[0];
            iss->vector.vregs[vd+k][i+1] = data[1];
            iss->vector.vregs[vd+k][i+2] = data[2];
            iss->vector.vregs[vd+k][i+3] = data[3];

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
                iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);


            // printf("data0 = %d\n",data[0]);
            // printf("data1 = %d\n",data[1]);
            // printf("data2 = %d\n",data[2]);
            // printf("data3 = %d\n",data[3]);

            iss->vector.vregs[vd+k][i+0] = data[0];
            iss->vector.vregs[vd+k][i+1] = data[1];
            iss->vector.vregs[vd+k][i+2] = data[2];
            iss->vector.vregs[vd+k][i+3] = data[3];

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
            data[0]  = iss->vector.vregs[vs3+k][i+0];
            data[1]  = iss->vector.vregs[vs3+k][i+1];
            data[2]  = iss->vector.vregs[vs3+k][i+2];
            data[3]  = iss->vector.vregs[vs3+k][i+3];


            // printf("STORE8 \n");
            // printf("data0  = %d\n",data[0 ]);
            // printf("data1  = %d\n",data[1 ]);
            // printf("data2  = %d\n",data[2 ]);
            // printf("data3  = %d\n",data[3 ]);

            // printf("addr  = %lu\n",start_add);

            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
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
            data[0]  = iss->vector.vregs[vs3+k][i+0];
            data[1]  = iss->vector.vregs[vs3+k][i+1];
            data[2]  = iss->vector.vregs[vs3+k][i+2];
            data[3]  = iss->vector.vregs[vs3+k][i+3];


            // printf("STORE8 \n");
            // printf("data0  = %d\n",data[0 ]);
            // printf("data1  = %d\n",data[1 ]);
            // printf("data2  = %d\n",data[2 ]);
            // printf("data3  = %d\n",data[3 ]);

            // printf("addr  = %lu\n",start_add);

            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
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
            data[0]  = iss->vector.vregs[vs3+k][i+0];
            data[1]  = iss->vector.vregs[vs3+k][i+1];
            data[2]  = iss->vector.vregs[vs3+k][i+2];
            data[3]  = iss->vector.vregs[vs3+k][i+3];


            // printf("STORE8 \n");
            // printf("data0  = %d\n",data[0 ]);
            // printf("data1  = %d\n",data[1 ]);
            // printf("data2  = %d\n",data[2 ]);
            // printf("data3  = %d\n",data[3 ]);

            // printf("addr  = %lu\n",start_add);

            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
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
            data[0]  = iss->vector.vregs[vs3+k][i+0];
            data[1]  = iss->vector.vregs[vs3+k][i+1];
            data[2]  = iss->vector.vregs[vs3+k][i+2];
            data[3]  = iss->vector.vregs[vs3+k][i+3];


            // printf("STORE8 \n");
            // printf("data0  = %d\n",data[0 ]);
            // printf("data1  = %d\n",data[1 ]);
            // printf("data2  = %d\n",data[2 ]);
            // printf("data3  = %d\n",data[3 ]);

            // printf("addr  = %lu\n",start_add);

            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
            start_add += 4;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            STRIDED LOAD/STORE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline void lib_VLSE8V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    int i = VSTART;
    while(i < VL){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        if(!(!(vm) && !bin[(i+0)%8])){
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,1,data,false);
            iss->vector.vregs[vd][i+0] = data[0];
        }
        start_add += rs2;
        i++;
    }
}

static inline void lib_VLSE16V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    int i = VSTART;
    while(i < VL*2){
        if(!((i/2)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/2)/8],bin);
        }
        if(!(!(vm) && !bin[((i/2)+0)%8])){
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,2,data,false);
            iss->vector.vregs[vd][i+0] = data[0];
            iss->vector.vregs[vd][i+1] = data[1];
        }
        start_add += rs2;
        i+=2;
    }
}

static inline void lib_VLSE32V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    int i = VSTART;
    while(i < VL*4){
        if(!((i/4)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/4)/8],bin);
        }
        if(!(!(vm) && !bin[((i/4)+0)%8])){
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
            iss->vector.vregs[vd][i+0] = data[0];
            iss->vector.vregs[vd][i+1] = data[1];
            iss->vector.vregs[vd][i+2] = data[2];
            iss->vector.vregs[vd][i+3] = data[3];
        }
        start_add += rs2;
        i+=4;
    }
}

static inline void lib_VLSE64V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    int flag = 0;
    int i = VSTART;
    while(i < VL*8){
        if(!((i/8)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/8)/8],bin);
        }
        if(!(!(vm) && !bin[((i/8)+0)%8])){
            if(flag){
                flag = 0;
                iss->vector.vlsu.Vlsu_io_access(iss, (start_add+4),4,data,false);
                start_add += rs2;
            }else{
                flag = 1;
                iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,false);
            }
            iss->vector.vregs[vd][i+0] = data[0];
            iss->vector.vregs[vd][i+1] = data[1];
            iss->vector.vregs[vd][i+2] = data[2];
            iss->vector.vregs[vd][i+3] = data[3];
        }else{
            if(flag){
                start_add += rs2;
                flag = 0;
            }else{
                flag = 1;
            }
        }
        i+=4;
    }
}


static inline void lib_VSSE8V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    int i = VSTART;
    while(i < VL){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        if(!(!(vm) && !bin[(i+0)%8])){
            data[0] = iss->vector.vregs[vd][i+0];
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,1,data,true);
        }
        start_add += rs2;
        i++;
    }
}

static inline void lib_VSSE16V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    int i = VSTART;
    while(i < VL*2){
        if(!((i/2)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/2)/8],bin);
        }
        if(!(!(vm) && !bin[((i/2)+0)%8])){
            data[0] = iss->vector.vregs[vd][i+0];
            data[1] = iss->vector.vregs[vd][i+1];
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,2,data,true);
        }
        start_add += rs2;
        i+=2;
    }
}

static inline void lib_VSSE32V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    int i = VSTART;
    while(i < VL*4){
        if(!((i/4)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/4)/8],bin);
        }
        if(!(!(vm) && !bin[((i/4)+0)%8])){
            data[0] = iss->vector.vregs[vd][i+0];
            data[1] = iss->vector.vregs[vd][i+1];
            data[2] = iss->vector.vregs[vd][i+2];
            data[3] = iss->vector.vregs[vd][i+3];
            iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
        }
        start_add += rs2;
        i+=4;
    }
}

static inline void lib_VSSE64V (Iss *iss, iss_reg_t rs1, iss_reg_t rs2, int vd , bool vm){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    int flag = 0;
    int i = VSTART;
    while(i < VL*8){
        if(!((i/8)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/8)/8],bin);
        }
        if(!(!(vm) && !bin[((i/8)+0)%8])){
            data[0] = iss->vector.vregs[vd][i+0];
            data[1] = iss->vector.vregs[vd][i+1];
            data[2] = iss->vector.vregs[vd][i+2];
            data[3] = iss->vector.vregs[vd][i+3];
            if(flag){
                flag = 0;
                iss->vector.vlsu.Vlsu_io_access(iss, (start_add+4),4,data,true);
                start_add += rs2;
            }else{
                flag = 1;
                iss->vector.vlsu.Vlsu_io_access(iss, start_add,4,data,true);
            }
        }else{
            if(flag){
                start_add += rs2;
                flag = 0;
            }else{
                flag = 1;
            }
        }
        i+=4;
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            INDEXED LOAD/STORE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline void lib_VLUXEI8V  (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, float r){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    uint8_t index;
    int i = VSTART;
    while(i < VL){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }

        index = iss->vector.vregs[vs2][int(i/r) - VSTART];
        iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index),4,data,false);

        if(!(!(vm) && !bin[(i+0)%8])){
            iss->vector.vregs[vd][i+0] = data[0];
        }
        i++;
    }
}

static inline void lib_VLUXEI16V (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm , float r){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    uint8_t index;

    int i = VSTART;

    while(i < VL*2){
        if(!((i/2)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/2)/8],bin);
        }
        index = iss->vector.vregs[vs2][int(i/r) - VSTART];
        iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index),4,data,false);
        if(!(!(vm) && !bin[(i/2+0)%8])){
            iss->vector.vregs[vd][i+0] = data[0];
            iss->vector.vregs[vd][i+1] = data[1];
        }
        i+=2;
    }
}

static inline void lib_VLUXEI32V (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, float r){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    uint8_t index;

    int i = VSTART;

    while(i < VL*4){
        if(!((i/4)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/4)/8],bin);
        }

        index = iss->vector.vregs[vs2][int(i/r) - VSTART];

        iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index),4,data,false);

        if(!(!(vm) && !bin[(i/4+0)%8])){
            iss->vector.vregs[vd][i+0] = data[0];
            iss->vector.vregs[vd][i+1] = data[1];
            iss->vector.vregs[vd][i+2] = data[2];
            iss->vector.vregs[vd][i+3] = data[3];
        }
        i+=4;
    }
}

static inline void lib_VLUXEI64V (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, float r){

    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    uint8_t index;
    int flag = 0;
    int i = VSTART;

    while(i < VL*8){
        if(!((i/8)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/8)/8],bin);
        }

        if(flag){
            flag = 0;
            iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index+4),4,data,false);

        }else{
            flag = 1;
            index = iss->vector.vregs[vs2][int(i/r) - VSTART];
            iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index),4,data,false);
        }
        if(!(!(vm) && !bin[(i/8+0)%8])){
            iss->vector.vregs[vd][i+0] = data[0];
            iss->vector.vregs[vd][i+1] = data[1];
            iss->vector.vregs[vd][i+2] = data[2];
            iss->vector.vregs[vd][i+3] = data[3];
        }
        i+=4;
    }
}

static inline void lib_VLUXEIV  (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, int EEW){
    float temp = float(SEW)/float(EEW);
    if(SEW == 8){
        lib_VLUXEI8V   (iss, rs1, vs2, vd , vm, temp);
    }else if(SEW == 16){
        lib_VLUXEI16V  (iss, rs1, vs2, vd , vm, temp);
    }else if(SEW == 32){
        lib_VLUXEI32V  (iss, rs1, vs2, vd , vm, temp);
    }else if(SEW == 64){
        lib_VLUXEI64V  (iss, rs1, vs2, vd , vm, temp);
    }
}


static inline void lib_VSUXEI8V  (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, float r){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    uint8_t index;
    int i = VSTART;
    while(i < VL){
        if(!(i%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][i/8],bin);
        }
        index = iss->vector.vregs[vs2][int(i/r) - VSTART];
        if(!(!(vm) && !bin[(i+0)%8])){
            data[0] = iss->vector.vregs[vd][i+0];
            iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index),1,data,true);
        }
        i++;
    }
}

static inline void lib_VSUXEI16V (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, float r){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    uint8_t index;
    int i = VSTART;
    while(i < VL*2){
        if(!((i/2)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/2)/8],bin);
        }
        index = iss->vector.vregs[vs2][int(i/r) - VSTART];
        if(!(!(vm) && !bin[(i/2+0)%8])){
            data[0] = iss->vector.vregs[vd][i+0];
            data[1] = iss->vector.vregs[vd][i+1];
            iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index),2,data,true);
        }
        i+=2;
    }
}

static inline void lib_VSUXEI32V (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, float r){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    uint8_t index;
    int i = VSTART;
    while(i < VL*4){
        if(!((i/4)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/4)/8],bin);
        }
        index = iss->vector.vregs[vs2][int(i/r) - VSTART];
        if(!(!(vm) && !bin[(i/4+0)%8])){
            data[0] = iss->vector.vregs[vd][i+0];
            data[1] = iss->vector.vregs[vd][i+1];
            data[2] = iss->vector.vregs[vd][i+2];
            data[3] = iss->vector.vregs[vd][i+3];
            iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index),4,data,true);
        }
        i+=4;
    }
}

static inline void lib_VSUXEI64V (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, float r){
    uint64_t start_add = rs1;
    uint8_t data[4];
    bool bin[8];
    uint8_t index;
    int i = VSTART;
    int flag = 0;
    while(i < VL*8){
        if(!((i/8)%8)){
            intToBin(8,(int64_t) iss->vector.vregs[0][(i/8)/8],bin);
        }

        if(!(!(vm) && !bin[(i/8+0)%8])){
            data[0] = iss->vector.vregs[vd][i+0];
            data[1] = iss->vector.vregs[vd][i+1];
            data[2] = iss->vector.vregs[vd][i+2];
            data[3] = iss->vector.vregs[vd][i+3];

            if(flag){
                flag = 0;
                iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index+4),4,data,true);

            }else{
                flag = 1;
                index = iss->vector.vregs[vs2][int(i/r) - VSTART];
                iss->vector.vlsu.Vlsu_io_access(iss, (start_add+index),4,data,true);
            }
        }
        i+=4;
    }
}

static inline void lib_VSUXEIV  (Iss *iss, iss_reg_t rs1, int vs2, int vd , bool vm, int EEW){
    float temp = float(SEW)/float(EEW);

    if(SEW == 8){
        lib_VSUXEI8V   (iss, rs1, vs2, vd , vm, temp);
    }else if(SEW == 16){
        lib_VSUXEI16V  (iss, rs1, vs2, vd , vm, temp);
    }else if(SEW == 32){
        lib_VSUXEI32V  (iss, rs1, vs2, vd , vm, temp);
    }else if(SEW == 64){
        lib_VSUXEI64V  (iss, rs1, vs2, vd , vm, temp);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                            VECTOR CONFIGURATION SETTING
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline iss_reg_t lib_VSETVLI(Iss *iss, int idxRs1, int idxRd, int rs1, iss_uim_t lmul, iss_uim_t sew, iss_uim_t vtype){
    uint32_t AVL;

    if((int)(vtype/pow(2,31))){
        iss->csr.vtype.value = 0x8000000000000000;
        VL = 0;
        AVL = 0;
        return 0;
    }else if((lmul==5 && (sew == 1 || sew == 2 || sew ==3)) || (lmul==6 && (sew == 2 || sew ==3)) || (lmul==7 && sew==3)){
        iss->csr.vtype.value = 0x8000000000000000;
        VL = 0;
        AVL = 0;
        return 0;
    }else{
        iss->csr.vtype.value = vtype;
    }


        VSTART = 0;
        SEW = iss->vector.SEW_VALUES[sew];
        LMUL = iss->vector.LMUL_VALUES[lmul];

        if(idxRs1){
            AVL = rs1;
            VL = MIN(AVL,VLMAX);
        }else if(!idxRs1 && idxRd){
            AVL = UINT32_MAX;
            VL  = VLMAX;
        }else{
            AVL = VL;
        }
    return VL;
}

static inline iss_reg_t lib_VSETVL(Iss *iss, int idxRs1, int idxRd, int rs1, int rs2){

    uint32_t AVL;


    iss->csr.vtype.value = rs2;
    int sew = (rs2/8)%8;
    int lmul = rs2%8;
    if((int)(rs2/pow(2,31))){
        iss->csr.vtype.value = 0x8000000000000000;
        VL = 0;
        AVL = 0;
        return 0;
    }else if((lmul==5 && (sew == 1 || sew == 2 || sew ==3)) || (lmul==6 && (sew == 2 || sew ==3)) || (lmul==7 && sew==3)){
        iss->csr.vtype.value = 0x8000000000000000;
        VL = 0;
        AVL = 0;
        return 0;
    }else{
        iss->csr.vtype.value = rs2;
        VSTART = 0;
        SEW = iss->vector.SEW_VALUES[sew];
        LMUL = iss->vector.LMUL_VALUES[lmul];

        if(idxRs1){
            AVL = rs1;
            VL = MIN(AVL,VLMAX);
        }else if(!idxRs1 && idxRd){
            AVL = UINT32_MAX;
            VL  = VLMAX;
        }else{
            AVL = VL;
        }
    return VL;
    }
}


#endif
