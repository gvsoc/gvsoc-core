
/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the
University of California.  All rights reserved.

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

int_fast64_t f16_to_i64( float16_t a, uint_fast8_t roundingMode, bool exact )
{
    union ui16_f16 uA;
    uint_fast16_t uiA;
    bool sign;
    int_fast8_t exp;
    uint_fast16_t frac;
    int_fast32_t sig32;
    int_fast8_t shiftDist;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uA.f = a;
    uiA = uA.ui;
    sign = signF16UI( uiA );
    exp  = expF16UI( uiA );
    frac = fracF16UI( uiA );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( exp == 0x1F ) {
        softfloat_raiseFlags( iss, softfloat_flag_invalid );
        return
            frac ? i64_fromNaN
                : sign ? i64_fromNegOverflow : i64_fromPosOverflow;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    sig32 = frac;
    if ( exp ) {
        sig32 |= 0x0400;
        shiftDist = exp - 0x19;
        if ( 0 <= shiftDist ) {
            sig32 <<= shiftDist;
            return sign ? -sig32 : sig32;
        }
        shiftDist = exp - 0x0D;
        if ( 0 < shiftDist ) sig32 <<= shiftDist;
    }
    return
        softfloat_roundToI32(
            sign, (uint_fast32_t) sig32, roundingMode, exact );

}

int_fast64_t f16_to_i64_alt( float16_t a, uint_fast8_t roundingMode, bool exact )

{
    union ui16_f16 uA;
    uint_fast16_t uiA;
    bool sign;
    int_fast8_t exp;
    uint_fast32_t sig;
    uint_fast64_t sig64, extra;
    struct uint64_extra sig64Extra;
    int_fast16_t shiftDist;

    // for f32 to i64 127+23+40=150+40=190	40=64-M:23-1
    // for bf16 to i64 127+7+56=134+56=190	56=64-M:7-1
    // for f16 to i64 15+10+53=25+53=78		53=64-M:10-1
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uA.f = a;
    uiA = uA.ui;
    sign = signF16UI( uiA );
    exp  = expF16UI( uiA );
    sig = fracF16UI( uiA );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    shiftDist = (15+10+53) - exp;
    if ( shiftDist < 0 ) {
        softfloat_raiseFlags( iss, softfloat_flag_invalid );
        return
            (exp == 0x1F) && sig ? i64_fromNaN
                : sign ? i64_fromNegOverflow : i64_fromPosOverflow;
    }

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( exp ) sig |= 0x00000400;
    sig64 = (uint_fast64_t) sig<<53;
    extra = 0;
    if ( shiftDist ) {
        sig64Extra = softfloat_shiftRightJam64Extra( sig64, 0, shiftDist );
        sig64 = sig64Extra.v;
        extra = sig64Extra.extra;
    }
    return softfloat_roundToI64( sign, sig64, extra, roundingMode, exact );
}

int_fast64_t bf16_to_i64( bfloat16_t a, uint_fast8_t roundingMode, bool exact )
{
    union ui16_bf16 uA;
    uint_fast16_t uiA;
    bool sign;
    int_fast16_t exp;
    uint_fast32_t sig;
    uint_fast64_t sig64, extra;
    struct uint64_extra sig64Extra;
    int_fast16_t shiftDist;

    // for f32 to i64 127+23+40=150+40=190	40=64-M:23-1
    // for bf16 to i64 127+7+56=134+56=190	56=64-M:7-1
    // for f16 to i64 15+10+53=25+53=78		53=64-M:10-1
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uA.f = a;
    uiA = uA.ui;
    sign = signBF16UI( uiA );
    exp  = expBF16UI( uiA );
    sig = fracBF16UI( uiA );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    shiftDist = (127+7+56) - exp;
    if ( shiftDist < 0 ) {
        softfloat_raiseFlags( iss, softfloat_flag_invalid );
        return
            (exp == 0xFF) && sig ? i64_fromNaN
                : sign ? i64_fromNegOverflow : i64_fromPosOverflow;
    }

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( exp ) sig |= 0x00000080;
    sig64 = (uint_fast64_t) sig<<56;
    extra = 0;
    if ( shiftDist ) {
        sig64Extra = softfloat_shiftRightJam64Extra( sig64, 0, shiftDist );
        sig64 = sig64Extra.v;
        extra = sig64Extra.extra;
    }
    return softfloat_roundToI64( sign, sig64, extra, roundingMode, exact );
}

