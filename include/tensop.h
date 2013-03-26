/*
 * Copyright 1991 Michael S. Warren and John K. Salmon.  All Rights Reserved.
 */

#ifndef NDIM
 # error NDIM must be defined before reading this file.
#endif

#if (NDIM!=3) && (NDIM!=2)
 #error NDIM must be either 2 or 3
#endif

#if (NDIM==3)

#define TS(t, s) do{ \
	t.xx s;\
	t.yy s;\
	t.zz s;\
	t.xy s;\
	t.xz s;\
	t.yz s;\
}while(0)

#define TT(a, b) do{ \
	a->xx b->xx;\
	a->yy b->yy;\
	a->zz b->zz;\
	a->xy b->xy;\
	a->xz b->xz;\
	a->yz b->yz;\
}while(0)

#endif /* NDIM == 3 */

#if (NDIM==2)

#define TS(t, s) do{ \
	t.xx s;\
	t.yy s;\
	t.xy s;\
}while(0)

#define TT(a, b) do{ \
	a->xx b->xx;\
	a->yy b->yy;\
	a->xy b->xy;\
}while(0)

#endif /* NDIM == 2 */
