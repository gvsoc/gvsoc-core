#include <stdio.h>
#include <stdint.h>

inline uint32_t snrt_mcycle() {
    uint32_t register r;
    asm volatile("csrr %0, mcycle" : "=r"(r) : : "memory");
    return r;
}

#define SYNC_BASE_ADDR 0x80210000
#define ASYNC_BASE_ADDR 0x90210000
#define ASYNC2_BASE_ADDR 0xA0210000

static __attribute__((noinline)) void bench(const uint32_t base, uint32_t *result, uint32_t *diff_0, uint32_t *diff_1)
{
    // __atomic_fetch_add((uint32_t *)BASE_ADDR, 1, __ATOMIC_RELAXED);

    volatile uint32_t *ptr = (volatile uint32_t *)base;

    // Snitch has 8 outstanding request, and each is 1000 cycles
    // The first 8 should not block but the 8 following should block
    // Then the final reads should block on all accesses,
    // So we should get around 1000 cycles for first step and 2000 for last step
    uint32_t start = snrt_mcycle();
    uint32_t a = ptr[0];
    uint32_t b = ptr[1];
    uint32_t c = ptr[2];
    uint32_t d = ptr[3];
    uint32_t e = ptr[4];
    uint32_t f = ptr[5];
    uint32_t g = ptr[6];
    uint32_t h = ptr[7];
    uint32_t i = ptr[8];
    uint32_t j = ptr[9];
    uint32_t k = ptr[10];
    uint32_t l = ptr[11];
    uint32_t m = ptr[12];
    uint32_t n = ptr[13];
    uint32_t o = ptr[14];
    uint32_t p = ptr[15];

    *diff_0 = snrt_mcycle() - start;

    *result = a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p;

    *diff_1 = snrt_mcycle() - start;
}

static __attribute__((noinline)) void bench_atomic(const uint32_t base, uint32_t *result, uint32_t *diff_0, uint32_t *diff_1)
{
    volatile uint32_t *ptr = (volatile uint32_t *)base;

    // Snitch has 8 outstanding request, and each is 1000 cycles
    // The first 8 should not block but the 8 following should block
    // Then the final reads should block on all accesses,
    // So we should get around 1000 cycles for first step and 2000 for last step
    uint32_t start = snrt_mcycle();

    uint32_t a = __atomic_fetch_add((uint32_t *)(base + 0) , 1, __ATOMIC_RELAXED);
    uint32_t b = __atomic_fetch_add((uint32_t *)(base + 4) , 1, __ATOMIC_RELAXED);
    uint32_t c = __atomic_fetch_add((uint32_t *)(base + 8) , 1, __ATOMIC_RELAXED);
    uint32_t d = __atomic_fetch_add((uint32_t *)(base + 12), 1, __ATOMIC_RELAXED);
    uint32_t e = __atomic_fetch_add((uint32_t *)(base + 16), 1, __ATOMIC_RELAXED);
    uint32_t f = __atomic_fetch_add((uint32_t *)(base + 20), 1, __ATOMIC_RELAXED);
    uint32_t g = __atomic_fetch_add((uint32_t *)(base + 24), 1, __ATOMIC_RELAXED);
    uint32_t h = __atomic_fetch_add((uint32_t *)(base + 28), 1, __ATOMIC_RELAXED);
    uint32_t i = __atomic_fetch_add((uint32_t *)(base + 32), 1, __ATOMIC_RELAXED);
    uint32_t j = __atomic_fetch_add((uint32_t *)(base + 36), 1, __ATOMIC_RELAXED);
    uint32_t k = __atomic_fetch_add((uint32_t *)(base + 40), 1, __ATOMIC_RELAXED);
    uint32_t l = __atomic_fetch_add((uint32_t *)(base + 44), 1, __ATOMIC_RELAXED);
    uint32_t m = __atomic_fetch_add((uint32_t *)(base + 48), 1, __ATOMIC_RELAXED);
    uint32_t n = __atomic_fetch_add((uint32_t *)(base + 52), 1, __ATOMIC_RELAXED);
    uint32_t o = __atomic_fetch_add((uint32_t *)(base + 56), 1, __ATOMIC_RELAXED);
    uint32_t p = __atomic_fetch_add((uint32_t *)(base + 60), 1, __ATOMIC_RELAXED);

    *diff_0 = snrt_mcycle() - start;

    *result = a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p;

    *diff_1 = snrt_mcycle() - start;
}

static int bench_from_base(uint32_t base)
{
    uint32_t diff_0, diff_1;
    uint32_t result;
    // Iterate a bit to avoid cache issues
    for (int i=0; i<4; i++)
    {
        bench(base, &result, &diff_0, &diff_1);
    }

    printf("The first step took %d cycles\n", diff_0);
    printf("The second step took %d cycles\n", diff_1);

    return result != 480;
}

static int bench_atomic_from_base(uint32_t base)
{
    uint32_t diff_0, diff_1;
    uint32_t result;
    // Iterate a bit to avoid cache issues
    for (int i=0; i<4; i++)
    {
        for (int i=0; i<= 60; i+= 4)
        {
            *(volatile int *)(base + i) = i;
        }

        for (volatile int i=0; i<1000; i++);

        bench_atomic(base, &result, &diff_0, &diff_1);
    }

    printf("The first step took %d cycles\n", diff_0);
    printf("The second step took %d cycles\n", diff_1);

    return result != 480;
}

int main()
{
    for (int i=0; i<= 60; i+= 4)
    {
        *(volatile int *)(SYNC_BASE_ADDR + i) = i;
    }

    for (int i=0; i<= 60; i+= 4)
    {
        *(volatile int *)(ASYNC_BASE_ADDR + i) = i;
    }

    for (int i=0; i<= 60; i+= 4)
    {
        *(volatile int *)(SYNC_BASE_ADDR + i) = i;
    }

    for (int i=0; i<= 60; i+= 4)
    {
        *(volatile int *)(ASYNC2_BASE_ADDR + i) = i;
    }

    for (volatile int i=0; i<1000; i++);

    if (bench_from_base(SYNC_BASE_ADDR)) return -1;
    if (bench_from_base(ASYNC_BASE_ADDR)) return -1;
    if (bench_from_base(ASYNC2_BASE_ADDR)) return -1;

    if (bench_atomic_from_base(SYNC_BASE_ADDR)) return -1;
    if (bench_atomic_from_base(ASYNC_BASE_ADDR)) return -1;
    if (bench_atomic_from_base(ASYNC2_BASE_ADDR)) return -1;

    return 0;
}
