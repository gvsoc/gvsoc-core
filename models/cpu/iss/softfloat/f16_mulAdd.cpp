
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

#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "softfloat.h"

float16_t f16_mulAdd( Iss *iss, float16_t a, float16_t b, float16_t c )
{
    union ui16_f16 uA;
    uint_fast16_t uiA;
    union ui16_f16 uB;
    uint_fast16_t uiB;
    union ui16_f16 uC;
    uint_fast16_t uiC;

    uA.f = a;
    uiA = uA.ui;
    uB.f = b;
    uiB = uB.ui;
    uC.f = c;
    uiC = uC.ui;
    return softfloat_mulAddF16( iss, uiA, uiB, uiC, 0 );
}

float16_t f16_mulSub( Iss *iss, float16_t a, float16_t b, float16_t c )
{
    union ui16_f16 uA;
    uint_fast16_t uiA;
    union ui16_f16 uB;
    uint_fast16_t uiB;
    union ui16_f16 uC;
    uint_fast16_t uiC;

    uA.f = a;
    uiA = uA.ui;
    uB.f = b;
    uiB = uB.ui;
    uC.f = c;
    uiC = uC.ui;
    return softfloat_mulAddF16( iss, uiA, uiB, uiC, 1 );
}

float16_t f16_NmulAdd( Iss *iss, float16_t a, float16_t b, float16_t c )
{
    union ui16_f16 uA;
    uint_fast16_t uiA;
    union ui16_f16 uB;
    uint_fast16_t uiB;
    union ui16_f16 uC;
    uint_fast16_t uiC;

    uA.f = a;
    uiA = uA.ui;
    uB.f = b;
    uiB = uB.ui;
    uC.f = c;
    uiC = uC.ui;
    return softfloat_mulAddF16( iss, uiA, uiB, uiC, 2 );
}

float16_t f16_NmulSub( Iss *iss, float16_t a, float16_t b, float16_t c )
{
    union ui16_f16 uA;
    uint_fast16_t uiA;
    union ui16_f16 uB;
    uint_fast16_t uiB;
    union ui16_f16 uC;
    uint_fast16_t uiC;

    uA.f = a;
    uiA = uA.ui;
    uB.f = b;
    uiB = uB.ui;
    uC.f = c;
    uiC = uC.ui;
    return softfloat_mulAddF16( iss, uiA, uiB, uiC, 3 );
}

bfloat16_t bf16_mulAdd( Iss *iss, bfloat16_t a, bfloat16_t b, bfloat16_t c )
{
    union ui16_bf16 uA;
    uint_fast16_t uiA;
    union ui16_bf16 uB;
    uint_fast16_t uiB;
    union ui16_bf16 uC;
    uint_fast16_t uiC;

    uA.f = a;
    uiA = uA.ui;
    uB.f = b;
    uiB = uB.ui;
    uC.f = c;
    uiC = uC.ui;
    return softfloat_mulAddBF16( iss, uiA, uiB, uiC, 0 );
}

bfloat16_t bf16_mulSub( Iss *iss, bfloat16_t a, bfloat16_t b, bfloat16_t c )
{
    union ui16_bf16 uA;
    uint_fast16_t uiA;
    union ui16_bf16 uB;
    uint_fast16_t uiB;
    union ui16_bf16 uC;
    uint_fast16_t uiC;

    uA.f = a;
    uiA = uA.ui;
    uB.f = b;
    uiB = uB.ui;
    uC.f = c;
    uiC = uC.ui;
    return softfloat_mulAddBF16( iss, uiA, uiB, uiC, 1 );
}

bfloat16_t bf16_NmulAdd( Iss *iss, bfloat16_t a, bfloat16_t b, bfloat16_t c )
{
    union ui16_bf16 uA;
    uint_fast16_t uiA;
    union ui16_bf16 uB;
    uint_fast16_t uiB;
    union ui16_bf16 uC;
    uint_fast16_t uiC;

    uA.f = a;
    uiA = uA.ui;
    uB.f = b;
    uiB = uB.ui;
    uC.f = c;
    uiC = uC.ui;
    return softfloat_mulAddBF16( iss, uiA, uiB, uiC, 2 );
}

bfloat16_t bf16_NmulSub( Iss *iss, bfloat16_t a, bfloat16_t b, bfloat16_t c )
{
    union ui16_bf16 uA;
    uint_fast16_t uiA;
    union ui16_bf16 uB;
    uint_fast16_t uiB;
    union ui16_bf16 uC;
    uint_fast16_t uiC;

    uA.f = a;
    uiA = uA.ui;
    uB.f = b;
    uiB = uB.ui;
    uC.f = c;
    uiC = uC.ui;
    return softfloat_mulAddBF16( iss, uiA, uiB, uiC, 3 );
}

