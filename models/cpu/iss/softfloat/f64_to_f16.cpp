
/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of
California.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "specialize.h"
#include "softfloat.h"

float16_t f64_to_f16( float64_t a )
{
    union ui64_f64 uA;
    uint_fast64_t uiA;
    bool sign;
    int_fast16_t exp;
    uint_fast64_t frac;
    struct commonNaN commonNaN;
    uint_fast16_t uiZ, frac16;
    union ui16_f16 uZ;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uA.f = a;
    uiA = uA.ui;
    sign = signF64UI( uiA );
    exp  = expF64UI( uiA );
    frac = fracF64UI( uiA );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( exp == 0x7FF ) {
        if ( frac ) {
            softfloat_f64UIToCommonNaN( uiA, &commonNaN );
            uiZ = softfloat_commonNaNToF16UI( &commonNaN );
        } else {
            uiZ = packToF16UI( sign, 0x1F, 0 );
        }
        goto uiZ;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    // MF64:52-MF16:10-(EF16:5-1=4)=38
    frac16 = softfloat_shortShiftRightJam64( frac, 38 );
    if ( ! (exp | frac16) ) {
        uiZ = packToF16UI( sign, 0, 0 );
        goto uiZ;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    // 1023-15+1=1008=0x3F1
    return softfloat_roundPackToF16( sign, exp - 0x3F1, frac16 | 0x4000 );
 uiZ:
    uZ.ui = uiZ;
    return uZ.f;
}

bfloat16_t f64_to_bf16( float64_t a )
{
    union ui64_f64 uA;
    uint_fast64_t uiA;
    bool sign;
    int_fast16_t exp;
    uint_fast64_t frac;
    struct commonNaN commonNaN;
    uint_fast16_t uiZ, frac16;
    union ui16_bf16 uZ;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uA.f = a;
    uiA = uA.ui;
    sign = signF64UI( uiA );
    exp  = expF64UI( uiA );
    frac = fracF64UI( uiA );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( exp == 0x7FF ) {
        if ( frac ) {
            softfloat_f64UIToCommonNaN( uiA, &commonNaN );
            uiZ = softfloat_commonNaNToF16UI( &commonNaN );
        } else {
            uiZ = packToF16UI( sign, 0xFF, 0 );
        }
        goto uiZ;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    // MF64:52-MBF16:7-(EBF16:8-1=7)=38
    frac16 = softfloat_shortShiftRightJam64( frac, 38 );
    if ( ! (exp | frac16) ) {
        uiZ = packToF16UI( sign, 0, 0 );
        goto uiZ;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    // BF64:1023-BBF16:127+1=897=0x381, SilentBitBF16:80<<(EBF16:8-1) = 0x4000
    return softfloat_roundPackToBF16( sign, exp - 0x381, frac16 | 0x4000 );
 uiZ:
    uZ.ui = uiZ;
    return uZ.f;
}

