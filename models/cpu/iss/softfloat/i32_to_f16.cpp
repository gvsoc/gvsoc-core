
/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of
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
#include "softfloat.h"

float16_t i32_to_f16( int32_t a )
{
    bool sign;
    uint_fast32_t absA;
    int_fast8_t shiftDist;
    union ui16_f16 u;
    uint_fast16_t sig;

    sign = (a < 0);
    absA = sign ? -(uint_fast32_t) a : (uint_fast32_t) a;
    shiftDist = softfloat_countLeadingZeros32( absA ) - 21;	// 21 = 32-(M+1)
    if ( 0 <= shiftDist ) {
	// 0x18 = 24 = B:15+M:10-1=24
        u.ui =
            a ? packToF16UI(
                    sign, 0x18 - shiftDist, (uint_fast16_t) absA<<shiftDist )
                : 0;
        return u.f;
    } else {
        shiftDist += 4;
        sig =
            (shiftDist < 0)
                ? absA>>(-shiftDist)
                      | ((uint32_t) (absA<<(shiftDist & 31)) != 0)
                : (uint_fast16_t) absA<<shiftDist;
	// 0x1C = 0x18+4
        return softfloat_roundPackToF16( sign, 0x1C - shiftDist, sig );
    }

}

bfloat16_t i32_to_bf16( int32_t a )
{
    bool sign;
    uint_fast32_t absA;
    int_fast16_t shiftDist;
    union ui16_bf16 u;
    uint_fast16_t sig;

    sign = (a < 0);
    absA = sign ? -(uint_fast32_t) a : (uint_fast32_t) a;
    shiftDist = softfloat_countLeadingZeros32( absA ) - 24;	// 24=32-(M+1)
    // Max shiftDist=31-24=7
    if ( 0 <= shiftDist ) {
	// 133 = B:127 + (M-1) = 127 + 6
        u.ui =
            a ? packToBF16UI(
                    sign, 133 - shiftDist, (uint_fast16_t) absA<<shiftDist )
                : 0;
        return u.f;
    } else {
        shiftDist += 7;
        sig =
            (shiftDist < 0)
                ? softfloat_shiftRightJam32( absA, -shiftDist )
                : (uint_fast16_t) absA<<shiftDist;
	// 140=133+7
        return softfloat_roundPackToBF16( sign, 140 - shiftDist, sig );
    }
}
