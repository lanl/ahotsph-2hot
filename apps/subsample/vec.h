#ifdef __AVX__
#define NSSE 8 /* Number of floats in an AVX register */
#define Arch(a) a##_avx8
typedef float v8sf __attribute__ ((vector_size (32)));
typedef float vsf __attribute__ ((vector_size (32)));
typedef int v8si __attribute__ ((vector_size (32)));
typedef int vsi __attribute__ ((vector_size (32)));
#define vsf_hsum(a) (a[0]+a[1]+a[2]+a[3]+a[4]+a[5]+a[6]+a[7])
#define vsf_scalar(a) {a, a, a, a, a, a, a, a}
#define vsf_rsqrt(_r2) __builtin_ia32_rsqrtps256(_r2)
#define vsf_cmple(_eps2, _r2) __builtin_ia32_cmpps256(_eps2, _r2, 0x12)
#define vsf_and(_a, _b) (vsf)((vsi)_a & (vsi)_b);
#define vsf_count(_a) __builtin_popcount(__builtin_ia32_movmskps256(_a));
#else
#define NSSE 4 /* Number of floats in an SSE register */
#define Arch(a) a##_sse4
typedef float v4sf __attribute__ ((vector_size (16)));
typedef float vsf __attribute__ ((vector_size (16)));
typedef int v4si __attribute__ ((vector_size (16)));
typedef int vsi __attribute__ ((vector_size (16)));
#define vsf_hsum(a) (a[0]+a[1]+a[2]+a[3])
#define vsf_scalar(a) {a, a, a, a}
#define vsf_rsqrt(_r2) __builtin_ia32_rsqrtps(_r2)
#define vsf_cmple(_eps2, _r2) __builtin_ia32_cmpleps(_eps2, _r2)
#define vsf_and(_a, _b) __builtin_ia32_andps(_a, _b)
#define vsf_count(_a) __builtin_popcount(__builtin_ia32_movmskps(_a));
#endif

/* Transpose the 4x4 matrix composed of row[0-3].  */
#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3)			\
do {									\
  v4sf __r0 = (row0), __r1 = (row1), __r2 = (row2), __r3 = (row3);	\
  v4sf __t0 = __builtin_ia32_unpcklps (__r0, __r1);			\
  v4sf __t1 = __builtin_ia32_unpcklps (__r2, __r3);			\
  v4sf __t2 = __builtin_ia32_unpckhps (__r0, __r1);			\
  v4sf __t3 = __builtin_ia32_unpckhps (__r2, __r3);			\
  (row0) = __builtin_ia32_movlhps (__t0, __t1);				\
  (row1) = __builtin_ia32_movhlps (__t1, __t0);				\
  (row2) = __builtin_ia32_movlhps (__t2, __t3);				\
  (row3) = __builtin_ia32_movhlps (__t3, __t2);				\
} while (0)

