/*
 * Copyright 1991 Michael S. Warren and John K. Salmon.  All Rights Reserved.
 */

#ifndef NDIM
 # error NDIM must be defined before reading this file.
#endif

#if (NDIM!=3) && (NDIM!=2)
 #error NDIM must be either 2 or 3
#endif

#ifdef __STDC__
#define GLUE(a,b) a##b
#else
#define GLUE(a,b) a/**/b
#endif

/* UGLY, but you can use these for parentheses inside the VV macros! */
#define LPAREN (
#define RPAREN )
#define COMMA ,
#define Dot(a, b) (VVinfix(a, *b, +))

#if (NDIM==3)

#define Sinfix(s, op) s op s op s
#define Vinfix(a, op) a[0] op a[1] op a[2]
#define VVinfix(a, b, op) a[0] b[0] op a[1] b[1] op a[2] b[2]
/* Usage: printf(Sinfix("%g", " "), Vinfix(pos, COMMA)) */

#define VS(a, b) do { \
	a[0] b; \
	a[1] b; \
	a[2] b; \
      } while(0)

#define VV(a, b) do { \
        a[0] b[0]; \
        a[1] b[1]; \
        a[2] b[2]; \
      } while(0)

#define VVS(a, b, s) do { \
        a[0] b[0] s; \
        a[1] b[1] s; \
        a[2] b[2] s; \
      } while(0)

#define VVV(a, b, c) do { \
        a[0] b[0] c[0]; \
        a[1] b[1] c[1]; \
        a[2] b[2] c[2]; \
      } while(0)

#define VVVV(a, b, c, d) do { \
        a[0] b[0] c[0] d[0]; \
        a[1] b[1] c[1] d[1]; \
        a[2] b[2] c[2] d[2]; \
      } while(0)


#define VVVS(a, b, c, s) do { \
        a[0] b[0] c[0] s; \
        a[1] b[1] c[1] s; \
        a[2] b[2] c[2] s; \
      } while(0)

#define VVVVS(a, b, c, d, s) do { \
        a[0] b[0] c[0] d[0] s; \
        a[1] b[1] c[1] d[1] s; \
        a[2] b[2] c[2] d[2] s; \
      } while(0)


#define VVVVV(a, b, c, d, e) do { \
        a[0] b[0] c[0] d[0] e[0]; \
        a[1] b[1] c[1] d[1] e[1]; \
        a[2] b[2] c[2] d[2] e[2]; \
      } while(0)

#define VVVVVS(a, b, c, d, e, s) do { \
        a[0] b[0] c[0] d[0] e[0] s; \
        a[1] b[1] c[1] d[1] e[1] s; \
        a[2] b[2] c[2] d[2] e[2] s; \
      } while(0)

#define VVVVVV(a, b, c, d, e, f) do { \
        a[0] b[0] c[0] d[0] e[0] f[0]; \
        a[1] b[1] c[1] d[1] e[1] f[1]; \
        a[2] b[2] c[2] d[2] e[2] f[2]; \
      } while(0)

/* Eleven args! */
#define Velv(a, b, c, d, e, f, g, h, i, j, k) do { \
        a[0] b[0] c[0] d[0] e[0] f[0] g[0] h[0] i[0] j[0] k[0]; \
        a[1] b[1] c[1] d[1] e[1] f[1] g[1] h[1] i[1] j[1] k[1]; \
        a[2] b[2] c[2] d[2] e[2] f[2] g[2] h[2] i[2] j[2] k[2]; \
      } while(0)

/* And a couple of matrix operations to support gsw. */
#define MS(a, b) do { \
        a[0][0] b; \
        a[0][1] b; \
        a[0][2] b; \
        a[1][0] b; \
        a[1][1] b; \
        a[1][2] b; \
        a[2][0] b; \
        a[2][1] b; \
        a[2][2] b; \
      } while(0)

#define MVV(a, b, c) do { \
	a[0][0] b[0] c[0]; \
	a[0][1] b[0] c[1]; \
	a[0][2] b[0] c[2]; \
	a[1][0] b[1] c[0]; \
	a[1][1] b[1] c[1]; \
	a[1][2] b[1] c[2]; \
	a[2][0] b[2] c[0]; \
	a[2][1] b[2] c[1]; \
	a[2][2] b[2] c[2]; \
      } while(0)

#define Vdecl(type, v) type v[NDIM]

#define Dotx(a, b) \
  (GLUE(a,0)*GLUE(b,0) + GLUE(a,1)*GLUE(b,1) + GLUE(a,2)*GLUE(b,2))

/* Use for declarations */
#define Vxd(a) \
	GLUE(a,0); \
	GLUE(a,1); \
	GLUE(a,2)

/* Use in prototypes */
#define Vxp(a) \
	GLUE(a,0), \
	GLUE(a,1), \
	GLUE(a,2)

#define VxS(a, b) do { \
	GLUE(a,0) b; \
	GLUE(a,1) b; \
	GLUE(a,2) b; \
      } while(0)

#define VxV(a, b) do { \
        GLUE(a,0) b[0]; \
        GLUE(a,1) b[1]; \
        GLUE(a,2) b[2]; \
      } while(0)

#define VxdV(a, b) \
        GLUE(a,0) b[0]; \
        GLUE(a,1) b[1]; \
        GLUE(a,2) b[2]

#define VVx(a, b) do { \
        a[0] GLUE(b,0); \
        a[1] GLUE(b,1); \
        a[2] GLUE(b,2); \
      } while(0)

#define VxVx(a, b) do { \
        GLUE(a,0) GLUE(b,0); \
        GLUE(a,1) GLUE(b,1); \
        GLUE(a,2) GLUE(b,2); \
      } while(0)


#define VxVV(a, b, c) do { \
        GLUE(a,0) b[0] c[0]; \
        GLUE(a,1) b[1] c[1]; \
        GLUE(a,2) b[2] c[2]; \
      } while(0)

#define VxVVS(a, b, c, s) do { \
        GLUE(a,0) b[0] c[0] s; \
        GLUE(a,1) b[1] c[1] s; \
        GLUE(a,2) b[2] c[2] s; \
      } while(0)

#define VxVVx(a, b, c) do { \
        GLUE(a,0) b[0] GLUE(c,0); \
        GLUE(a,1) b[1] GLUE(c,1); \
        GLUE(a,2) b[2] GLUE(c,2); \
      } while(0)

#define VxVxV(a, b, c) do { \
        GLUE(a,0) GLUE(b,0) c[0]; \
        GLUE(a,1) GLUE(b,1) c[1]; \
        GLUE(a,2) GLUE(b,2) c[2]; \
      } while(0)

#define VxVxVx(a, b, c) do { \
        GLUE(a,0) GLUE(b,0) GLUE(c,0); \
        GLUE(a,1) GLUE(b,1) GLUE(c,1); \
        GLUE(a,2) GLUE(b,2) GLUE(c,2); \
      } while(0)

#define VVxVx(a, b, c) do{ \
	a[0] GLUE(b,0) GLUE(c,0); \
	a[1] GLUE(b,1) GLUE(c,1); \
	a[2] GLUE(b,2) GLUE(c,2); \
      } while(0)

#endif /* NDIM == 3 */

#if (NDIM==2)

#define Sinfix(s, op) s op s
#define Vinfix(a, op) a[0] op a[1]
#define VVinfix(a, b, op) a[0] b[0] op a[1] b[1]
/* Usage: printf(Sinfix("%g", " "), Vinfix(pos, COMMA)) */

#define VS(a, b) do { \
	a[0] b; \
	a[1] b; \
      } while(0)

#define VV(a, b) do { \
        a[0] b[0]; \
        a[1] b[1]; \
      } while(0)

#define VVS(a, b, s) do { \
        a[0] b[0] s; \
        a[1] b[1] s; \
        a[2] b[2] s; \
      } while(0)

#define VVV(a, b, c) do { \
        a[0] b[0] c[0]; \
        a[1] b[1] c[1]; \
      } while(0)

#define VVVS(a, b, c, s) do { \
        a[0] b[0] c[0] s; \
        a[1] b[1] c[1] s; \
      } while(0)

#define VVVVS(a, b, c, d, s) do { \
        a[0] b[0] c[0] d[0] s; \
        a[1] b[1] c[1] d[1] s; \
      } while(0)

#define VVVV(a, b, c, d) do { \
        a[0] b[0] c[0] d[0]; \
        a[1] b[1] c[1] d[1]; \
      } while(0)

#define VVVVV(a, b, c, d, e) do { \
        a[0] b[0] c[0] d[0] e[0]; \
        a[1] b[1] c[1] d[1] e[1]; \
      } while(0)

#define Vdecl(type, v) type v[NDIM]

#define Dotx(a, b) \
  (GLUE(a,0)*GLUE(b,0) + GLUE(a,1)*GLUE(b,1))

#define Vxd(a) \
	GLUE(a,0); \
	GLUE(a,1)

#define VxS(a, b) do { \
	GLUE(a,0) b; \
	GLUE(a,1) b; \
      } while(0)

#define VxV(a, b) do { \
        GLUE(a,0) b[0]; \
        GLUE(a,1) b[1]; \
      } while(0)

#define VxdV(a, b) \
        GLUE(a,0) b[0]; \
        GLUE(a,1) b[1]

#define VVx(a, b) do { \
        a[0] GLUE(b,0); \
        a[1] GLUE(b,1); \
      } while(0)

#define VxVx(a, b) do { \
        GLUE(a,0) GLUE(b,0); \
        GLUE(a,1) GLUE(b,1); \
      } while(0)


#define VxVV(a, b, c) do { \
        GLUE(a,0) b[0] c[0]; \
        GLUE(a,1) b[1] c[1]; \
      } while(0)

#define VxVVS(a, b, c, s) do { \
        GLUE(a,0) b[0] c[0] s; \
        GLUE(a,1) b[1] c[1] s; \
      } while(0)

#define VxVVx(a, b, c) do { \
        GLUE(a,0) b[0] GLUE(c,0); \
        GLUE(a,1) b[1] GLUE(c,1); \
      } while(0)

#define VxVVS(a, b, c, s) do { \
        GLUE(a,0) b[0] c[0] s; \
        GLUE(a,1) b[1] c[1] s; \
      } while(0)

#define VVxVx(a, b, c) do{ \
	a[0] GLUE(b,0) GLUE(c,0); \
	a[1] GLUE(b,1) GLUE(c,1); \
      } while(0)

#define VxVxV(a, b, c) do { \
        GLUE(a,0) GLUE(b,0) c[0]; \
        GLUE(a,1) GLUE(b,1) c[1]; \
      } while(0)

#define VxVxVx(a, b, c) do { \
        GLUE(a,0) GLUE(b,0) GLUE(c,0); \
        GLUE(a,1) GLUE(b,1) GLUE(c,1); \
      } while(0)

#endif /* NDIM == 2 */
