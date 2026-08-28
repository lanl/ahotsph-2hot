/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

/* These are some hacks to try to speed up some common floating */
/* point operations. */
#ifndef _FastFlptDOTh
#define _FastFlptDOTh

/* These are the generic definitions. */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
float recip8bit(float);
float recipsqrt8bit(float);
float recipsqrtf(float);
float recipf(float);
float sqrtf_fast(float);
#ifndef __convex__
double sqrt(double);
#endif
#ifdef __cplusplus
}
#endif /* __cplusplus */

#define recipsqrt8bit recipsqrtf
#define recip8bit recipf
#define recipsqrtf(x) (recipf(sqrtf_fast(x)))
#define recipf(x) (((float)1.0) / (x))
#define sqrtf_fast(x) (sqrt(x))

/* Now we conditionally redefine some of them. */
#if defined(sparc) && defined(__GNUC__) && !defined(__INSIGHT__)
#undef sqrtf_fast
#define sqrtf_fast(x)                                     \
    ({                                                    \
        float __value, __arg = (x);                       \
        asm("fsqrts %1,%0" : "=f"(__value) : "f"(__arg)); \
        __value;                                          \
    })
#endif

#if defined(mips)
#undef sqrtf_fast
#if defined(__GNUC__)
/* I'm not positive the sqrt.s instruction is right (msw) */
#define sqrtf_fast(x)                                      \
    ({                                                     \
        float __value, __arg = (x);                        \
        asm("sqrt.s %1, %0" : "=f"(__value) : "f"(__arg)); \
        __value;                                           \
    })
#else
/* Compilation with -OPT:fast_sqrt -OPT:IEEE_arithmetic=3 seems to
   make a big difference on an R8000.  The first enables use of the fast
   sqrt instruction, and the second enables the compiler to play fast-and-
   loose with expressions involving division.  -- johns Oct 9, 1995 */
#include <math.h>
#define sqrtf_fast sqrtf
#endif
#endif

#if defined(__INTEL_SSD__)
#undef recipsqrtf
#undef sqrtf_fast
/* #define sqrtf(x) (x*recipsqrtf(x)) A very bad macro */

#ifdef __GNUC__
#undef recipsqrt8bit
#undef recip8bit
/* These tell the "user" that the values returned by */
/* recipsqrt8bit and recip8bit are really only approximations. */
#define HAS_FAST_APPROX_SQRT
#define HAS_FAST_APPROX_RECIP

#define recipsqrt8bit(x)                                    \
    ({                                                      \
        float __value, __arg = (x);                         \
        asm("frsqr.ss %1,%0" : "=f"(__value) : "f"(__arg)); \
        __value;                                            \
    })
#define recip8bit(x)                                       \
    ({                                                     \
        float __value, __arg = (x);                        \
        asm("frcp.ss %1,%0" : "=f"(__value) : "f"(__arg)); \
        __value;                                           \
    })
#endif /* __GNUC__ */

#endif /* __INTEL_SSD__ */

/* Karp Testing */
#ifdef _IBMR2
#include "karp.h"
#undef recipsqrt8bit
#define recipsqrt8bit(x) ((x)*recip3o2_8bit(x))
#undef recipsqrtf
#define recipsqrtf(x) ((x)*recip3o2f(x))
#endif

/* Is this the same as -ffast-math -mcpu=rios2 ? */
#if defined(_ARCH_PWR2) && defined(__GNUC__)
#undef sqrtf_fast
#define sqrtf_fast(x)                                    \
    ({                                                   \
        float __value, __arg = (x);                      \
        asm("fsqrt %0,%1" : "=f"(__value) : "f"(__arg)); \
        __value;                                         \
    })
#endif

#if defined(_ARCH_PPC64) && defined(__GNUC__)
#undef sqrtf_fast
#undef recip8bit
#undef recipsqrt8bit
#define HAS_FAST_APPROX_SQRT
#define HAS_FAST_APPROX_RECIP

#define sqrtf_fast(x)                                     \
    ({                                                    \
        float __value, __arg = (x);                       \
        asm("fsqrts %0,%1" : "=f"(__value) : "f"(__arg)); \
        __value;                                          \
    })

#define recip8bit(x)                                    \
    ({                                                  \
        float __value, __arg = (x);                     \
        asm("fres %0,%1" : "=f"(__value) : "f"(__arg)); \
        __value;                                        \
    })

/* Really 5bit */
#define recipsqrt8bit(x)                                   \
    ({                                                     \
        float __value, __arg = (x);                        \
        asm("frsqrte %0,%1" : "=f"(__value) : "f"(__arg)); \
        __value;                                           \
    })
#endif

#if defined(__alpha) && !defined(__linux)
#undef sqrtf_fast
#define sqrtf_fast(x) F_sqrtf(x)
extern float F_sqrtf(float);
#endif /* __alpha */

#if defined(__hppa__)
#undef sqrtf_fast
#ifdef __GNUC__
#define sqrtf_fast(x)                                          \
    ({                                                         \
        float __value, __arg = (x);                            \
        asm("fsqrt,sgl %1,%0" : "=fx"(__value) : "fx"(__arg)); \
        __value;                                               \
    })
#else
#define sqrtf_fast sqrtf
extern float sqrtf(float);
#endif
#endif

#if defined(__ncube__)
#undef sqrt_fast
#define sqrtf_fast sqrtf
extern float sqrtf(float);
#endif

#endif /* _FastFlptDOTh */
