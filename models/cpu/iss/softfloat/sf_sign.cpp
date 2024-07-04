#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "internals.h"
#include "softfloat.h"

bool f16_sign_get(float16_t a)

{
	return signF16UI(a.v);
}

float16_t f16_sign_set(float16_t a, bool s)

{
	float16_t r;
	r.v = (((uint16_t)s)<<15) | (a.v&0x7FFF);
	return r;
}

float16_t f16_neg(float16_t a)

{
	float16_t r;
	bool s = !signF16UI(a.v);
	r.v = (((uint16_t)s)<<15) | (a.v&0x7FFF);
	return r;
}

bool bf16_sign_get(bfloat16_t a)

{
	return signBF16UI(a.v);
}

bfloat16_t bf16_sign_set(bfloat16_t a, bool s)

{
	bfloat16_t r;
	r.v = (((uint16_t)s)<<15) | (a.v&0x7FFF);
	return r;
}

bfloat16_t bf16_neg(bfloat16_t a)

{
	bfloat16_t r;
	bool s = !signBF16UI(a.v);
	r.v = (((uint16_t)s)<<15) | (a.v&0x7FFF);
	return r;
}

bool f32_sign_get(float32_t a)

{
	return signF32UI(a.v);
}

float32_t f32_sign_set(float32_t a, bool s)

{
	float32_t r;
	r.v = (((uint32_t)s)<<31) | (a.v&0x7FFFFFFF);
	return r;
}

float32_t f32_neg(float32_t a)

{
	float32_t r;
	bool s = !signF32UI(a.v);
	r.v = (((uint32_t)s)<<15) | (a.v&0x7FFFFFFF);
	return r;
}

bool f64_sign_get(float64_t a)

{
	return signF64UI(a.v);
}

float64_t f64_sign_set(float64_t a, bool s)

{
	float64_t r;
	r.v = (((uint64_t)s)<<63) | (a.v&UINT64_C(0x7FFFFFFFFFFFFFFF));
	return r;
}

float64_t f64_neg(float64_t a)

{
	float64_t r;
	bool s = !signF64UI(a.v);
	r.v = (((uint64_t)s)<<63) | (a.v&UINT64_C(0x7FFFFFFFFFFFFFFF));
	return r;
}
