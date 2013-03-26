/* Functions defined in keycvt.c.  This will go in the library, eventually */

#ifndef KeyUTiLsDOTh
#define KeyUTiLsDOTh

#include "key.h"
#define MAXNDIMKU 5

/* Use these as the 'order' argument in CellBBFromKey and GenerateKeys */
#define MORTON_ORDER 1
#define PH_ORDER 2

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

typedef struct {
    int ndim;
    /* Store rmin and the size.  There are lots of choices.  Some might
       be better than others.  This is one of them... */
    float rmin[MAXNDIMKU];
    float sz[MAXNDIMKU];
} tbbox;			/* a tree-bbox,  */

/* For now, we will break the OO secrecy rule and just let routines know
   what's inside the tbbox.  Otherwise I have to write a dozen functions
   to extract and insert values...Ugh. */
/* CenterBbox is useful, nevertheless */
void CenterBbox(tbbox *bb, float *center);

/* This gives a tight bounding box around the list of positions.  Note
   that the result is rmin <= pos[i] and rmax >= pos[i].  The equality
   can be a headache for float-to-int conversions!  Consider using
   InflateBbox and or CubeBbox! */
void TightBbox(float *pstart, int nobj, int pstride, int ndim, tbbox *bb);

/* Make the bbox a cube by expanding the smaller dimensions. */
void CubeBbox(tbbox *bb);

/* Increase the linear dimension by 'factor' on all sides */
void InflateBbox(tbbox *bb, float factor);

/* Return 1 if bb1 completely contains bb2 */
int ContainsBbox(tbbox *bb1, tbbox *bb2);

/* Construct bbu, the 'union' of bb1 and bb2 */
void UnionBbox(tbbox *bb1, tbbox *bb2, tbbox *bbu);

/* Generate keys for an array of positions (imagine sizeof(body) as the
   stride argument!) */
void GenerateKeys(float *pstart, int nobj, int pstride, tbbox *bb, Key_t *kstart, int kstride, int ordering);
/* This will be set when GenerateKeys detects that a key is out of
   bounds.  GenerateKeys will not crash and burn, but prudent callers
   will check KeyOutOfBounds after calling it.  It is cumulative. */
extern int KeyOutOfBounds;

/* Replacement for CellCorner:  you supply the bbox that describes the
   Universe, and we return a bbox that describes the cell */
void CellBBFromKey(Key_t key, tbbox *bb, tbbox *cellbb, int ordering);

/* Some primitive building blocks.  They can be combined with
 KeyFromInts and IntsFromKey to do the full conversion... */
void IntsFromFloats(const float *x, unsigned int *ix, tbbox *bb, int nbits);
void FloatsFromInts(const int *ix, float *x, tbbox *bb, int nbits);

/* These two names conflict with physics_generic.c.  Good.  It will
   keep me from using physics_generic.c accidentally. */
Key_t KeyFromInts(unsigned int *xp, int ndim, int nbits);
int IntsFromKey(Key_t key, unsigned int *ip, int ndim);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
