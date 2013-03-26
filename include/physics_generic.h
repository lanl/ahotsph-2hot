#ifndef _PhysicsDOTh
#define _PhysicsDOTh
#include <sys/types.h>
#include "key.h"

#ifndef NDIM
 # error You must define NDIM before including physics.h
#endif

/* from physics.c (generic) */
#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
void FindBbox(body *bp, int n, float *rmin, float *rmax);
float FixRsize(float *rmin, float *rmax);
float FixRsizeExact(float *rmin, float *rmax);
void CellCorner(Key_t key, float *corner, float *size);
void CellCornerPH(Key_t key, float *corner, float *size);
Key_t GetKey(const body *p);
Key_t GetKeyLong(const body *p);
Key_t GetKeyPH(const body *p);	/* peano-hilbert key */
float GetCost(const body *p);
float UnityCost(const void *p);

#ifdef HAS_KEY
Key_t GetKeyFromStruct(const body *p);
void FixKeys(body *btab, int nobj, Key_t (*func)(const body *));
#endif

#ifdef HAS_IDENT
Key_t OutIdentKey(const outbody *outb);
Key_t OutIdent64Key(const outbody *outb);
void FixId(body *btab, int nobj, int gnobj);
void FixId64(body *btab, int nobj, int64_t gnobj);
#endif

#ifdef HAS_NTERMS
void FixNterms(body *btab, int nobj);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* CHUBITS is the number of bits per-dimension in the key */
#define CHUBITS ((KEYBITS-1)/NDIM)
#define MAXCHU ((long long)1<<CHUBITS)

#endif
