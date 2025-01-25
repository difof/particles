#ifndef __MATH_HPP
#define __MATH_HPP

// Cross-platform SIMD support detection
#if defined(USE_X86_SSE) && defined(ARCH_X64) &&                               \
    (defined(PLATFORM_WINDOWS) || defined(PLATFORM_MACOS) ||                   \
     defined(PLATFORM_LINUX))
#include <xmmintrin.h>
#elif defined(__ARM_NEON) && defined(ARCH_ARM64) &&                            \
    (defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX))
#include <arm_neon.h>
#endif

inline float rsqrt_nr_once(float x, float y0) {
    // One Newton–Raphson refinement: y_{n+1} = y_n * (1.5 - 0.5*x*y_n^2)
    return y0 * (1.5f - 0.5f * x * y0 * y0);
}

inline float rsqrt_fast(float x) {
#if defined(USE_X86_SSE) && defined(ARCH_X64) &&                               \
    (defined(PLATFORM_WINDOWS) || defined(PLATFORM_MACOS) ||                   \
     defined(PLATFORM_LINUX))
    __m128 vx = _mm_set_ss(x);
    __m128 y = _mm_rsqrt_ss(vx); // ~12-bit accurate
    float y0 = _mm_cvtss_f32(y);
    // One NR step → ~1e-4 relative error
    return rsqrt_nr_once(x, y0);
#elif defined(__ARM_NEON) && defined(ARCH_ARM64) &&                            \
    (defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX))
    float32x2_t vx = vdup_n_f32(x);
    float32x2_t y = vrsqrte_f32(vx); // initial approx
    // One NR step using NEON recip-sqrt iterations
    // y = y * (1.5 - 0.5*x*y*y)
    float32x2_t y2 = vmul_f32(y, y);
    float32x2_t halfx = vmul_n_f32(vx, 0.5f);
    float32x2_t threehalves = vdup_n_f32(1.5f);
    y = vmul_f32(y, vsub_f32(threehalves, vmul_f32(halfx, y2)));
    return vget_lane_f32(y, 0);
#else
    // Scalar fallback: Quake-style bit hack + one NR step
    // Good speedup on -O3; accurate enough for forces
    float xhalf = 0.5f * x;
    int i = *(int *)&x; // type-pun (OK on most compilers; or use std::bit_cast
                        // in C++20)
    i = 0x5f3759df - (i >> 1);
    float y = *(float *)&i;
    y = y * (1.5f - xhalf * y * y); // one NR step
    return y;
#endif
}

#endif