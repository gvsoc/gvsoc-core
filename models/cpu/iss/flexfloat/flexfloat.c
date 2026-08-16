/*
   Copyright 2018 - The OPRECOMP Project Consortium, Alma Mater Studiorum
   Università di Bologna. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "flexfloat.h"
// To avoid manually discerning backend-type for calls from math.h
#include <tgmath.h>

#if defined(FLEXFLOAT_ROUNDING)|| defined(FLEXFLOAT_FLAGS)
#include <fenv.h>
#pragma STDC FENV_ACCESS ON
#endif

#include "assert.h"

int_fast16_t flexfloat_exp(const flexfloat_t *a)
{
    int_fast16_t a_exp   = EXPONENT(CAST_TO_INT(a->value));

    int_fast16_t bias    = flexfloat_bias(a->desc);

    if(a_exp == 0 || a_exp == INF_EXP)
        return a_exp;
    else
        return (a_exp - BIAS) + bias;
}

uint_t flexfloat_frac(const flexfloat_t *a)
{
    return (CAST_TO_INT(a->value) & MASK_FRAC) >> (NUM_BITS_FRAC - a->desc.frac_bits);
}

uint_t flexfloat_denorm_frac(const flexfloat_t *a, int_fast16_t exp)
{
    if(EXPONENT(CAST_TO_INT(a->value)) == 0) // Denormalized backend value
    {
        return (CAST_TO_INT(a->value) & MASK_FRAC) >> (NUM_BITS_FRAC - a->desc.frac_bits);
    }
    else // Denormalized target value (in normalized backend value)
    {
        unsigned short shift = NUM_BITS_FRAC - a->desc.frac_bits - exp + 1;
        if(shift >= NUM_BITS) return 0;
        return (((CAST_TO_INT(a->value) & MASK_FRAC) | MASK_FRAC_MSB) >> shift);
    }
}

// Pack normalized desc-fraction with desc-relative exponent to backend float
uint_t flexfloat_pack(flexfloat_desc_t desc, bool sign, int_fast16_t exp, uint_t frac)
{
    int_fast16_t bias    = flexfloat_bias(desc);
    int_fast16_t inf_exp = flexfloat_inf_exp(desc);

    if(exp == inf_exp)   // Inf or NaN
    {
        exp = INF_EXP;
    }
    else
    {
        exp = (exp - bias) + BIAS;
    }
    return PACK(sign, exp, frac << (NUM_BITS_FRAC - desc.frac_bits));
}

uint_t flexfloat_denorm_pack(flexfloat_desc_t desc, bool sign, uint_t frac)
{
    int_fast16_t bias    = flexfloat_bias(desc);
    return PACK(sign, 0, frac << (NUM_BITS_FRAC - desc.frac_bits));
}

uint_t flexfloat_pack_bits(flexfloat_desc_t desc, uint_t bits)
{
    bool sign = (bits >> (desc.exp_bits + desc.frac_bits)) & 0x1;
    int_fast16_t exp = (bits >> desc.frac_bits) & ((0x1<<desc.exp_bits) - 1);
    uint_t frac = bits & ((UINT_C(1)<<desc.frac_bits) - 1);

    if(exp == 0 && frac == 0)
    {
        return PACK(sign, 0, 0);
    }
    else if(exp <= 0) // denormal
    {
        // printf("[ff_pack_bits] normalizing 0x%016lx, exp %d\n", frac, exp);
        while (frac && !((frac <<= 1) & (UINT_C(1) << desc.frac_bits))) // normalize
            exp--;
        frac &= ((UINT_C(1) << desc.frac_bits) - 1); // remove implicit bit
        // printf("[ff_pack_bits] done normalizing 0x%016lx, exp %d\n", frac, exp);

        return flexfloat_pack(desc, sign, exp, frac);
    }
    else
    {
        return flexfloat_pack(desc, sign, exp, frac);
    }
}

void flexfloat_set_bits(flexfloat_t *a, uint_t bits)
{
    CAST_TO_INT(a->value) = flexfloat_pack_bits(a->desc, bits);
}


uint_t flexfloat_get_bits(flexfloat_t *a)
{
    int_fast16_t exp = flexfloat_exp(a);
    uint_t frac = flexfloat_frac(a);

    if(exp == INF_EXP) exp = flexfloat_inf_exp(a->desc);
    else  if (exp<0 && frac == 0) {
            /* We have a subnormal here since we cannot represent exp (too small), set frac to 2^(frac_bits-exp+1) */
            if ((a->desc.frac_bits-exp-1) >= 0)
                frac = 1<<(a->desc.frac_bits+exp-1);
            else frac = 0;
            exp = 0;
    } else if(exp <= 0) {
        frac = flexfloat_denorm_frac(a, exp);
        exp = 0;
    }

#ifdef OLD
    return ((uint_t)flexfloat_sign(a) << (a->desc.exp_bits + a->desc.frac_bits))
           + ((uint_t)exp << a->desc.frac_bits)
           + frac;
#else
    uint_t Mask = ((uint_t)flexfloat_sign(a))?((uint_t)(-1)<<(a->desc.exp_bits + a->desc.frac_bits)):0;
    return ((((uint_t)flexfloat_sign(a) << (a->desc.exp_bits + a->desc.frac_bits))
           + ((uint_t)exp << a->desc.frac_bits)
           + frac) | Mask);
#endif
}


#ifdef FLEXFLOAT_ROUNDING

// get rounding bit from backend value (first bit after represented LSB)
bool flexfloat_round_bit(const flexfloat_t *a, int_fast16_t exp)
{
    if(exp <= 0 && EXPONENT(CAST_TO_INT(a->value)) != 0)
    {
#ifdef OLD
        int shift = (- exp + 1);
        uint_t denorm = 0;
        if(shift < NUM_BITS)
          denorm = ((CAST_TO_INT(a->value) & MASK_FRAC | MASK_FRAC_MSB)) >> shift;
        return denorm & (UINT_C(0x1) << (NUM_BITS_FRAC - a->desc.frac_bits - 1));
#else
        unsigned short shift = NUM_BITS_FRAC - a->desc.frac_bits - exp + 1;
	if (shift>=NUM_BITS) return 0;
	uint_t Frac = (CAST_TO_INT(a->value) & MASK_FRAC) | MASK_FRAC_MSB;
        int RndB = ((Frac & ((uint_t)1<<(shift-1)))!=0);
        return (RndB!=0);
#endif
    }
    else
    {
        return CAST_TO_INT(a->value) & (UINT_C(0x1) << (NUM_BITS_FRAC - a->desc.frac_bits - 1));
    }
}

// get sticky bit from backend value (logic OR of all bits after represented LSB except the round bit)
bool flexfloat_sticky_bit(const flexfloat_t *a, int_fast16_t exp)
{
    if(exp <= 0 && EXPONENT(CAST_TO_INT(a->value)) != 0)
    {
#ifdef OLD
        int shift = (- exp + 1);
        uint_t denorm = 0;
        if(shift < NUM_BITS)
            denorm = ((CAST_TO_INT(a->value) & MASK_FRAC) | MASK_FRAC_MSB) >> shift;
        return (denorm & (MASK_FRAC >> (a->desc.frac_bits + 1))) ||
               ( ((denorm & MASK_FRAC) == 0)  && (CAST_TO_INT(a->value)!=0) );
#else
        unsigned short shift = NUM_BITS_FRAC - a->desc.frac_bits - exp + 1;
	// Every significant bit (the value is normal in the backend, hence
	// nonzero) sits below the sticky window: the sticky bit is set. A
	// zero here would lose inexact/underflow and directed rounding to
	// the smallest subnormal for deeply tiny values.
	if (shift>=NUM_BITS) return 1;
	uint_t Frac = (CAST_TO_INT(a->value) & MASK_FRAC) | MASK_FRAC_MSB;
        int StiB = ((Frac & (((uint_t)1<<(shift-1))-1)) != 0);
        return (StiB!=0);

#endif
    }
    else
    {
        return CAST_TO_INT(a->value) & (MASK_FRAC >> (a->desc.frac_bits + 1));
    }
}

// check if rounding to nearest is required (the most significant bit of the discarded ones is 1)
bool flexfloat_nearest_rounding(const flexfloat_t *a, int_fast16_t exp)
{
    if (flexfloat_round_bit(a, exp))
    {
        if (flexfloat_sticky_bit(a, exp)) // > ulp/2 away
        {
            return 1;
        }
        else// = ulp/2 away, round towards even result, decided by LSB of mantissa
        {
            if (exp <= 0) // denormal
                return flexfloat_denorm_frac(a, exp) & 0x1;
            return flexfloat_frac(a) & 0x1;
        }
    }
    return 0; // < ulp/2 away
}

// check if rounding to +inf/-inf is required (at least one bit of the discarded ones is 1)
bool flexfloat_inf_rounding(const flexfloat_t *a, int_fast16_t exp, bool sign, bool plus)
{
    if (flexfloat_round_bit(a, exp) || flexfloat_sticky_bit(a, exp))
        return (plus ^ sign);
    return 0;
}

// return a value to sum in order to apply rounding
int_t flexfloat_rounding_value(const flexfloat_t *a, int_fast16_t exp, bool sign)
{
    if(EXPONENT(CAST_TO_INT(a->value)) == 0) // Denorm backend format
    {
        return flexfloat_denorm_pack(a->desc, sign, 0x1);
    }
    else if(exp <= 0) // Denorm target format
    {
        return flexfloat_pack(a->desc, sign, - a->desc.frac_bits + 1 , 0);
    }
    else
    {
        return flexfloat_pack(a->desc, sign, exp - a->desc.frac_bits , 0);
    }

}

// apply a one-ulp (target grid) increment to the backend value
static void flexfloat_apply_rounding(flexfloat_t *a, int_fast16_t exp, bool sign)
{
    int_t rounding_value = flexfloat_rounding_value(a, exp, sign);
    /* Truncate the discarded bits first (integer, exact): both addends are
     * then on the target grid and the sum is exact. Adding the ulp to the
     * raw backend value rounds in the backend format and can overshoot the
     * target grid by one ulp when the discarded bits extend to the bottom
     * of the backend mantissa. */
    if (EXPONENT(CAST_TO_INT(a->value)) != 0)
    {
        int shift = NUM_BITS_FRAC - a->desc.frac_bits + ((exp <= 0) ? - exp + 1 : 0);
        if (shift <= NUM_BITS_FRAC)
            CAST_TO_INT(a->value) &= ~((UINT_C(1) << shift) - UINT_C(1));
        else // magnitude entirely below one target ulp: truncate to (signed) zero
            CAST_TO_INT(a->value) &= UINT_C(1) << (NUM_BITS - 1);
    }
    a->value += CAST_TO_FP(rounding_value);
}

#endif // FLEXFLOAT_ROUNDING

// RISC-V RMM support: with FE_TONEAREST set, a tie (round bit 1, sticky 0) is
// resolved away from zero instead of to even. See flexfloat.h.
int flexfloat_rmm = 0;

void flexfloat_sanitize(flexfloat_t *a)
{
    bool sign;
    int_fast16_t exp;
    int_fast16_t inf_exp;
    uint_t frac;
#ifdef FLEXFLOAT_FLAGS
    bool inexact = false;
#endif

    if (isnan(a->value))
    {
        a->value = NAN;
        return;
    }

    // This case does not require to be sanitized
    if(a->desc.exp_bits  == NUM_BITS_EXP  &&
       a->desc.frac_bits == NUM_BITS_FRAC)
        return;

    // Sign
    sign = flexfloat_sign(a);

    // Exponent
    exp = flexfloat_exp(a);

#ifdef FLEXFLOAT_ROUNDING
    // In these cases no rounding is needed
    if (!(exp == INF_EXP  || a->desc.frac_bits == NUM_BITS_FRAC))
    {
#ifdef FLEXFLOAT_FLAGS
        // Inexact results raise an exception
        inexact = flexfloat_round_bit(a, exp) || flexfloat_sticky_bit(a, exp);
        if(inexact)
            feraiseexcept(FE_INEXACT);
        if (inexact && exp <= 0 && EXPONENT(CAST_TO_INT(a->value)) != 0)
        {
            /* Underflow = tiny AND inexact (IEEE 754 §7.5). */
            bool escapes = false;
#ifdef FLEXFLOAT_TININESS_AFTER_ROUNDING
            /* IEEE 754 leaves the tininess detection point implementation-
             * defined. This opt-in per-core c-flag selects detection after
             * rounding with unbounded exponent range: a subnormal-range
             * value escapes tininess only when, at full target precision,
             * it rounds up across the smallest normal (probed with the
             * helpers' normal branch, exp arg 1). Default: the historical
             * before-rounding detection. */
            if (exp == 0)
            {
                uint_t frac_all = (UINT_C(1) << a->desc.frac_bits) - 1;
                bool all_ones = (((CAST_TO_INT(a->value) & MASK_FRAC)
                                  >> (NUM_BITS_FRAC - a->desc.frac_bits)) == frac_all);
                int m = fegetround();
                bool up = (m == FE_TONEAREST && (flexfloat_rmm ? flexfloat_round_bit(a, 1)
                                                               : flexfloat_nearest_rounding(a, 1)))
                       || (m == FE_UPWARD   && flexfloat_inf_rounding(a, 1, sign, 1))
                       || (m == FE_DOWNWARD && flexfloat_inf_rounding(a, 1, sign, 0));
                escapes = all_ones && up;
            }
#endif
            if (!escapes)
                feraiseexcept(FE_UNDERFLOW);
        }
        // As rounding uses FP operations, we don't want to tarnish the accrued flags
        fexcept_t flags;
        fegetexceptflag(&flags, FE_ALL_EXCEPT);
#endif
        // Rounding mode
        int mode = fegetround();
        if(mode == FE_TONEAREST && (flexfloat_rmm ? flexfloat_round_bit(a, exp)
                                                  : flexfloat_nearest_rounding(a, exp)))
        {
            flexfloat_apply_rounding(a, exp, sign);
        }
        else if(mode == FE_UPWARD && flexfloat_inf_rounding(a, exp, sign, 1))
        {
            flexfloat_apply_rounding(a, exp, sign);
        }
        else if(mode == FE_DOWNWARD && flexfloat_inf_rounding(a, exp, sign, 0))
        {
            flexfloat_apply_rounding(a, exp, sign);
        }
#ifdef FLEXFLOAT_FLAGS
        // Restore flags from before
        fesetexceptflag(&flags, FE_ALL_EXCEPT);
#endif
        //a->value = a->value;
        __asm__ __volatile__ ("" ::: "memory");

        // Recompute exponent value after rounding
        exp = flexfloat_exp(a);
    }
#endif

    // Exponent of NaN and Inf (target format)
    inf_exp = flexfloat_inf_exp(a->desc);

    // Mantissa
    frac = flexfloat_frac(a);

    if(EXPONENT(CAST_TO_INT(a->value)) == 0) // Denorm backend format - represented format also denormal
    {
        CAST_TO_INT(a->value) = flexfloat_denorm_pack(a->desc, sign, frac);
        return;
    }

   if(exp <= 0) // Denormalized value in the target format (saved in normalized format in the backend value)
    {
#if defined(FLEXFLOAT_FLAGS) && !defined(FLEXFLOAT_ROUNDING)
        /* With rounding enabled, underflow is raised in the rounding block
         * above (tiny and inexact, tininess after rounding with unbounded
         * exponent range). Keep the legacy unconditional raise for
         * flag-only builds. */
        feraiseexcept(FE_UNDERFLOW);
#endif
        uint_t denorm = flexfloat_denorm_frac(a, exp);
        if(denorm == 0) // value too low to be represented, return zero
        {
            CAST_TO_INT(a->value) = PACK(sign, 0, 0);
            return;
        }
        else if(a->desc.frac_bits < NUM_BITS_FRAC) // Remove additional precision
        {
            int shift = - exp + 1;
            if(shift < NUM_BITS_FRAC)
            {
              frac >>= shift;
              frac <<= shift;
            }
            else
            {
              frac = UINT_C(0);
            }
        }
    }
    else if(exp == INF_EXP && (CAST_TO_INT(a->value) & MASK_FRAC)) // NaN
    {
        exp  = inf_exp;
        // Sanitize to canonical NaN (positive sign, quiet bit set)
        sign = 0;
        frac = UINT_C(1) << (a->desc.frac_bits-1);
    }
    else if(exp == INF_EXP) // Inf
    {
        /* No overflow here. IEEE 754 7.4 raises OF only when the result
         * rounded with an unbounded exponent range exceeds the destination
         * range; a backend value that is already Inf is an infinity
         * propagated from an operand (or produced by a division by zero,
         * which raises its own flag) and is exact. A genuine destination
         * overflow leaves the backend finite and is handled by the
         * (exp >= inf_exp) branch below. */
        exp = inf_exp;
    }
    else if(exp >= inf_exp) // Out of bounds for target format: overflow
    {
#ifdef FLEXFLOAT_FLAGS
        // Raise the proper overflow exception
        feraiseexcept(FE_OVERFLOW | FE_INEXACT);
#endif
        // IEEE 754 overflow: directed roundings that point away from the
        // overflow direction clamp to the largest finite value instead of
        // infinity (toward-zero always; downward for positive, upward for
        // negative). Nearest (even or ties-away) overflows to infinity.
        int ovf_mode = fegetround();
        if (ovf_mode == FE_TOWARDZERO ||
            (ovf_mode == FE_DOWNWARD && !sign) ||
            (ovf_mode == FE_UPWARD && sign))
        {
            exp = inf_exp - 1;
            frac = (UINT_C(1) << a->desc.frac_bits) - 1;
        }
        else
        {
            exp = inf_exp;
            frac = UINT_C(0);
        }
    }

    // printf("ENCODING: %d %d %lu\n", sign, exp, frac);
    CAST_TO_INT(a->value) = flexfloat_pack(a->desc, sign, exp, frac);
}

// Constructors

INLINE void ff_init(flexfloat_t *obj, flexfloat_desc_t desc) {
    obj->value = 0.0;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = 0.0;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc = desc;
}

INLINE void ff_init_float(flexfloat_t *obj, float value, flexfloat_desc_t desc) {
    obj->value = (fp_t)value;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = (fp_t)value;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc = desc;
    flexfloat_sanitize(obj);
}

INLINE void ff_init_double(flexfloat_t *obj, double value, flexfloat_desc_t desc) {
    obj->value = (fp_t)value;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = (fp_t)value;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc = desc;
    flexfloat_sanitize(obj);
}


INLINE void ff_init_longdouble(flexfloat_t *obj, long double value, flexfloat_desc_t desc) {
    obj->value = (fp_t)value;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = (fp_t)value;;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc = desc;
    flexfloat_sanitize(obj);
}

#ifdef FLEXFLOAT_ON_QUAD
INLINE void ff_init_float128(flexfloat_t *obj, __float128 value, flexfloat_desc_t desc) {
    obj->value = (fp_t)value;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = (fp_t)value;;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc = desc;
    flexfloat_sanitize(obj);
}
#endif

INLINE void ff_init_int(flexfloat_t *obj, int value, flexfloat_desc_t desc) {
    obj->value = (fp_t)value;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = (fp_t)value;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc = desc;
    flexfloat_sanitize(obj);
}


INLINE void ff_init_long(flexfloat_t *obj, long value, flexfloat_desc_t desc) {
    obj->value = (fp_t)value;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = (fp_t)value;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc = desc;
    flexfloat_sanitize(obj);
}

INLINE void ff_init_long_long_unsigned(flexfloat_t *obj, unsigned long long value, flexfloat_desc_t desc) {
    obj->value = (fp_t)value;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = (fp_t)value;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc = desc;
    flexfloat_sanitize(obj);
}



INLINE void ff_cast(flexfloat_t *obj, const flexfloat_t *source, flexfloat_desc_t desc ) {
    obj->value = source->value;
    #ifdef FLEXFLOAT_TRACKING
    obj->exact_value = source->exact_value;
    obj->tracking_fn = 0;
    obj->tracking_arg = 0;
    #endif
    obj->desc  = desc;
    if(desc.exp_bits != source->desc.exp_bits || desc.frac_bits != source->desc.frac_bits)
        flexfloat_sanitize(obj);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getCastStats(source->desc, desc)->total += 1;
    #endif
}


// Casts

INLINE float ff_get_float(const flexfloat_t *obj) {
    return (float)(*((const fp_t *)(&(obj->value))));
}

INLINE double ff_get_double(const flexfloat_t *obj) {
    return (double)(*((const fp_t *)(&(obj->value))));
}

INLINE long double ff_get_longdouble(const flexfloat_t *obj) {
    return (long double)(*((const fp_t *)(&(obj->value))));
}

#ifdef FLEXFLOAT_ON_QUAD
INLINE __float128 ff_get_float128(const flexfloat_t *obj) {
    return (__float128)(*((const fp_t *)(&(obj->value))));
}
#endif


// Arithmetics

INLINE void ff_inverse(flexfloat_t *dest, const flexfloat_t *a) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits));
    dest->value = - a->value;
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = - a->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->minus += 1;
    #endif
}


INLINE void ff_add(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    dest->value = a->value + b->value;
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value + b->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->add += 1;
    #endif
}

INLINE void ff_sub(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    dest->value = a->value - b->value;
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value - b->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->sub += 1;
    #endif
}

INLINE void ff_mul(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    dest->value = a->value * b->value;
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value * b->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->mul += 1;
    #endif
}

INLINE void ff_div(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    dest->value = a->value / b->value;
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = a->exact_value / b->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->div += 1;
    #endif
}

INLINE void ff_acc(flexfloat_t *dest, const flexfloat_t *a) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits));
    dest->value += a->value;
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value += a->exact_value;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->minus += 1;
    #endif
}

INLINE void ff_min(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    dest->value = fmin(a->value,b->value);
    // fmin's zero sign handling is implementation defined! Check for 0 cases to ensure right result
    if ((a->value == 0) && (a->value == b->value))
    {
        if (flexfloat_sign(a) || flexfloat_sign(b))
            CAST_TO_INT(dest->value) = (UINT_C(0x1) << (NUM_BITS-1));
        else
            CAST_TO_INT(dest->value) = 0;
    }

    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = fmin(a->exact_value,b->exact_value);
    if ((a->exact_value == 0) && (a->exact_value == b->exact_value))
        CAST_TO_INT(dest->exact_value) = (UINT_C(0x1) << NUM_BITS-1);
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->minmax += 1;
    #endif
}

INLINE void ff_max(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    dest->value = fmax(a->value,b->value);
    // fmin's zero sign handling is implementation defined! Check for 0 cases to ensure right result
    if ((a->value == 0) && (a->value == b->value))
    {
        if (flexfloat_sign(a) && flexfloat_sign(b))
            CAST_TO_INT(dest->value) = (UINT_C(0x1) << (NUM_BITS-1));
        else
            CAST_TO_INT(dest->value) = 0;
    }
    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = fmax(a->exact_value,b->exact_value);
    if ((a->exact_value == 0) && (a->exact_value == b->exact_value))
        CAST_TO_INT(dest->exact_value) = 0;
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->minmax += 1;
    #endif
}

INLINE void ff_fma(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) &&
           (b->desc.exp_bits == c->desc.exp_bits) && (b->desc.frac_bits == c->desc.frac_bits));
    #ifdef FLEXFLOAT_FLAGS
    /* inf*0 signals INVALID even when the addend is a quiet NaN: IEEE 754
     * 7.2 leaves that sub-case implementation-defined and the host FMA
     * propagates the NaN silently, but RISC-V mandates the flag (unpriv F).
     * Idempotent when the addend is not a NaN: the host raises it too. */
    if ((a->value == 0.0 && isinf(b->value)) ||
        (isinf(a->value) && b->value == 0.0))
        feraiseexcept(FE_INVALID);
    #endif
    #ifdef FLEXFLOAT_ROUNDING
    if (a->desc.frac_bits < NUM_BITS_FRAC)
    {
        /* Round-to-odd intermediate (Boldo-Muller): truncate the fused op
         * toward zero and, if inexact, force the mantissa LSB to 1. An odd
         * binary64 intermediate keeps faithful round AND sticky information
         * for any narrower destination, so the software rounding in
         * flexfloat_sanitize (every mode, RMM included) yields the
         * single-rounding fused result - the double-rounding artefact is
         * structurally gone. The binary64 op cannot overflow or
         * underflow on narrow-format inputs; INVALID is re-raised, its
         * INEXACT is dropped - sanitize re-derives the destination
         * inexactness from the odd/round/sticky bits. */
        fexcept_t accrued;
        fegetexceptflag(&accrued, FE_ALL_EXCEPT);
        feclearexcept(FE_ALL_EXCEPT);
        int mode = fegetround();
        fesetround(FE_TOWARDZERO);
        double r = fma(a->value, b->value, c->value);
        int raised = fetestexcept(FE_ALL_EXCEPT);
        fesetround(mode);
        fesetexceptflag(&accrued, FE_ALL_EXCEPT);
        if (raised & FE_INVALID)
            feraiseexcept(FE_INVALID);
        if ((raised & FE_INEXACT) && !isnan(r) && !isinf(r))
            CAST_TO_INT(r) |= 1;
        else if (r == 0.0)
            /* Exact zero: its sign depends on the actual rounding mode
             * (-0 on exact cancellation under RDN), which the toward-zero
             * run hides. Recompute exactly, no flags raised. */
            r = fma(a->value, b->value, c->value);
        dest->value = r;
    }
    else
    #endif
    dest->value = fma(a->value, b->value, c->value); // full-width destination: plain fused op

    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = fma(a->exact_value, b->exact_value, c->exact_value);
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->fma += 1;
    #endif
}

INLINE void ff_fnma(flexfloat_t *dest, const flexfloat_t *a, const flexfloat_t *b, const flexfloat_t *c) {
    assert((dest->desc.exp_bits == a->desc.exp_bits) && (dest->desc.frac_bits == a->desc.frac_bits) &&
           (a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits) &&
           (b->desc.exp_bits == c->desc.exp_bits) && (b->desc.frac_bits == c->desc.frac_bits));
    #ifdef FLEXFLOAT_FLAGS
    /* inf*0 signals INVALID even when the addend is a quiet NaN: IEEE 754
     * 7.2 leaves that sub-case implementation-defined and the host FMA
     * propagates the NaN silently, but RISC-V mandates the flag (unpriv F).
     * Idempotent when the addend is not a NaN: the host raises it too. */
    if ((a->value == 0.0 && isinf(b->value)) ||
        (isinf(a->value) && b->value == 0.0))
        feraiseexcept(FE_INVALID);
    #endif
    #ifdef FLEXFLOAT_ROUNDING
    if (a->desc.frac_bits < NUM_BITS_FRAC)
    {
        /* Round-to-odd intermediate, then negate: truncation toward zero
         * and the odd bit are sign-symmetric, so -round_to_odd(a*b+c) is
         * the odd-rounded -(a*b+c). See ff_fma for the full rationale. */
        fexcept_t accrued;
        fegetexceptflag(&accrued, FE_ALL_EXCEPT);
        feclearexcept(FE_ALL_EXCEPT);
        int mode = fegetround();
        fesetround(FE_TOWARDZERO);
        double r = fma(a->value, b->value, c->value);
        int raised = fetestexcept(FE_ALL_EXCEPT);
        fesetround(mode);
        fesetexceptflag(&accrued, FE_ALL_EXCEPT);
        if (raised & FE_INVALID)
            feraiseexcept(FE_INVALID);
        if ((raised & FE_INEXACT) && !isnan(r) && !isinf(r))
        {
            CAST_TO_INT(r) |= 1;
            dest->value = -r;
        }
        else if (r == 0.0)
            /* Exact zero: the sign must be derived on the negated fusion
             * in the actual rounding mode (-0 on exact cancellation under
             * RDN) - negating the toward-zero result would flip it. */
            dest->value = fma(-a->value, b->value, -c->value);
        else
            dest->value = -r;
    }
    else
    #endif
    dest->value = -fma(a->value, b->value, c->value);

    #ifdef FLEXFLOAT_TRACKING
    dest->exact_value = fma(a->exact_value, b->exact_value, c->exact_value);
    if(dest->tracking_fn) (dest->tracking_fn)(dest, dest->tracking_arg);
    #endif
    flexfloat_sanitize(dest);
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(dest->desc)->fma += 1;
    #endif
}

// Relational operators

INLINE bool ff_eq(const flexfloat_t *a, const flexfloat_t *b) {
    assert((a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(a->desc)->cmp += 1;
    #endif
    return a->value == b->value;
}

INLINE bool ff_neq(const flexfloat_t *a, const flexfloat_t *b) {
        assert((a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(a->desc)->cmp += 1;
    #endif
    return a->value != b->value;
}

INLINE bool ff_le(const flexfloat_t *a, const flexfloat_t *b) {
    assert((a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    #if defined(FLEXFLOAT_FLAGS) && !defined(FLEXFLOAT_CORRECT_CMP_FLAGS)
    if (isnan(a->value) || isnan(b->value))
        feraiseexcept(FE_INVALID);
    #endif
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(a->desc)->cmp += 1;
    #endif
    return (a->value <= b->value);
}

INLINE bool ff_lt(const flexfloat_t *a, const flexfloat_t *b) {
    assert((a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    #if defined(FLEXFLOAT_FLAGS) && !defined(FLEXFLOAT_CORRECT_CMP_FLAGS)
    if (isnan(a->value) || isnan(b->value))
        feraiseexcept(FE_INVALID);
    #endif
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(a->desc)->cmp += 1;
    #endif
    return (a->value < b->value);
}

INLINE bool ff_ge(const flexfloat_t *a, const flexfloat_t *b) {
    assert((a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    #if defined(FLEXFLOAT_FLAGS) && !defined(FLEXFLOAT_CORRECT_CMP_FLAGS)
    if (isnan(a->value) || isnan(b->value))
        feraiseexcept(FE_INVALID);
    #endif
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(a->desc)->cmp += 1;
    #endif
    return (a->value >= b->value);
}

INLINE bool ff_gt(const flexfloat_t *a, const flexfloat_t *b) {
    assert((a->desc.exp_bits == b->desc.exp_bits) && (a->desc.frac_bits == b->desc.frac_bits));
    #if defined(FLEXFLOAT_FLAGS) && !defined(FLEXFLOAT_CORRECT_CMP_FLAGS)
    if (isnan(a->value) || isnan(b->value))
        feraiseexcept(FE_INVALID);
    #endif
    #ifdef FLEXFLOAT_STATS
    if(StatsEnabled) getOpStats(a->desc)->cmp += 1;
    #endif
    return (a->value > b->value);
}

// Collection of statistics
#ifdef FLEXFLOAT_STATS
#include <stdlib.h>
#include <string.h>

bool StatsEnabled = 1;
HashSlot   op_stats[FLEXFLOAT_STATS_MAX_TYPES];
HashSlot cast_stats[FLEXFLOAT_STATS_MAX_TYPES*FLEXFLOAT_STATS_MAX_TYPES];

void * ht_search(HashSlot* hashArray, uint32_t hashIndex, uint32_t key, uint32_t arraySize) {
   hashIndex %= arraySize;
   while(hashArray[hashIndex].key != 0) {
      // look for the key
      if(hashArray[hashIndex].key == key)
         return hashArray[hashIndex].value;
      // not found? try the next slot!
      ++hashIndex;
      hashIndex %= arraySize;
   }
   return 0;
}
void ht_insert(HashSlot* hashArray, uint32_t hashIndex, uint32_t key, void *value, uint32_t arraySize) {
    hashIndex %= arraySize;
    // look for a free slot
    while(hashArray[hashIndex].key != 0) {
        ++hashIndex;
        hashIndex %= arraySize;
        assert(hashIndex != key); // No free slots after a full iteration
   }
   hashArray[hashIndex].key = key;
   hashArray[hashIndex].value = value;
}

OpStats * getOpStats(const flexfloat_desc_t desc)
{
    uint32_t hashIndex = precision_hash(desc);
    void * result  = ht_search(op_stats, hashIndex, hashIndex, FLEXFLOAT_STATS_MAX_TYPES);
    if(result == 0) {
        result = malloc(sizeof(OpStats));
        memset(result, 0, sizeof(OpStats));
        ht_insert(op_stats, hashIndex, hashIndex, result, FLEXFLOAT_STATS_MAX_TYPES);
    }
    return (OpStats *) result;
}

CastStats * getCastStats(const flexfloat_desc_t desc1, const flexfloat_desc_t desc2)
{
    uint32_t hashIndex = precision_hash2(desc1, desc2);
    void * result  = ht_search(cast_stats, hashIndex, hashIndex, FLEXFLOAT_STATS_MAX_TYPES*FLEXFLOAT_STATS_MAX_TYPES);
    if(result == 0) {
        result = malloc(sizeof(CastStats));
        memset(result, 0, sizeof(CastStats));
        ht_insert(cast_stats, hashIndex, hashIndex, result, FLEXFLOAT_STATS_MAX_TYPES*FLEXFLOAT_STATS_MAX_TYPES);
    }
    return (CastStats *) result;
}

INLINE void ff_start_stats() {
    StatsEnabled = 1;
}

INLINE void ff_stop_stats() {
    StatsEnabled = 0;
}

void ff_clear_stats() {
    int i;
    for(i=0; i<FLEXFLOAT_STATS_MAX_TYPES; ++i)
        if(op_stats[i].key != 0) free(op_stats[i].value);
    memset(op_stats, 0, sizeof(HashSlot) * FLEXFLOAT_STATS_MAX_TYPES);
    for(i=0; i<FLEXFLOAT_STATS_MAX_TYPES*FLEXFLOAT_STATS_MAX_TYPES; ++i)
        if(cast_stats[i].key != 0) free(cast_stats[i].value);
    memset(cast_stats, 0, sizeof(HashSlot) * FLEXFLOAT_STATS_MAX_TYPES*FLEXFLOAT_STATS_MAX_TYPES);
}

void ff_print_stats() {
    int i;
    printf("-- OPERATIONS -- \n");
    for(i=0; i<FLEXFLOAT_STATS_MAX_TYPES; ++i) {
        uint32_t key = op_stats[i].key;
        if(key != 0) {
            KeyStruct skey = *(KeyStruct*)&key;
            uint8_t exp_bits = skey.exp_bits1;
            uint8_t frac_bits = skey.frac_bits1;
            OpStats * stats = (OpStats *) op_stats[i].value;

            printf("flexfloat<%hhu,%hhu>\n", exp_bits, frac_bits);
            printf("    INV    \t%lu\n", stats->minus);
            printf("    ADD    \t%lu\n", stats->add);
            printf("    SUB    \t%lu\n", stats->sub);
            printf("    MUL    \t%lu\n", stats->mul);
            printf("    DIV    \t%lu\n", stats->div);
            printf("  MIN/MAX  \t%lu\n", stats->minmax);
            printf("    FMA    \t%lu\n", stats->fma);
            printf("    CMP    \t%lu\n", stats->cmp);
        }
    }
    printf("-- CASTS -- \n");
    for(i=0; i<FLEXFLOAT_STATS_MAX_TYPES*FLEXFLOAT_STATS_MAX_TYPES; ++i) {
        uint32_t key = cast_stats[i].key;
        if(key != 0) {
            KeyStruct skey = *(KeyStruct*)&key;
            uint8_t exp_bits1 = skey.exp_bits1;
            uint8_t frac_bits1 = skey.frac_bits1;
            uint8_t exp_bits2 = skey.exp_bits2;
            uint8_t frac_bits2 = skey.frac_bits2;
            CastStats * stats = (CastStats *) cast_stats[i].value;

            printf("flexfloat<%hhu,%hhu> -> flexfloat<%hhu,%hhu>\n", exp_bits1, frac_bits1, exp_bits2, frac_bits2);
            printf("    TOTAL    \t%lu\n", stats->total);
        }
    }
}

#endif /* FLEXFLOAT_STATS */
