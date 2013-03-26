/* A non-plug-compatible replacement for physics_generic.[ch].  Use with
   care, but then, you were using physics_generic.c with care, weren't
   you? */

#include <stddef.h>
#include <float.h>
#include "protos.h"
#include "mpmy.h"
#include "Msgs.h"
#include "Assert.h"
#include "key.h"
#include "peano.h"
#include "keycvt.h"

#ifndef FLT_MAX
/* I wonder what they put in float.h, anyway */
#define FLT_MAX 1.e38
#endif

int KeyOutOfBounds;

/* This gives a tight bounding box around the list of positions.  Note
   that the result is rmin <= pos[i] and rmax >= pos[i].  The equality
   can be a headache for float-to-int conversions!  Consider using
   InflateBbox and or CubeBbox! */
void
TightBbox(float *pstart, int nobj, int pstride, int ndim, tbbox *bb){
    MPMY_Comm_request req;
    int d;
    float *pos = pstart;
    float rmin[MAXNDIMKU];
    float rmax[MAXNDIMKU];

    assert(ndim < MAXNDIMKU);

    for(d=0; d<ndim; d++){
	rmin[d] = FLT_MAX;
	rmax[d] = -FLT_MAX;
    }
    bb->ndim = ndim;
    while(nobj--){
	for(d=0; d<ndim; d++){
	    float pd = pos[d];
	    if( rmin[d] > pd ) rmin[d] = pd;
	    if( rmax[d] < pd ) rmax[d] = pd;
	}
	pos = (float *)(pstride + (char *)pos);
    }
    MPMY_ICombine_Init(&req);
    MPMY_ICombine(rmin, bb->rmin, ndim, MPMY_FLOAT, MPMY_MIN, req);
    MPMY_ICombine(rmax, rmax, ndim, MPMY_FLOAT, MPMY_MAX, req);
    MPMY_ICombine_Wait(req);
    for(d=0; d<ndim; d++){
	bb->sz[d] = rmax[d] - bb->rmin[d];
    }
}

void 
CenterBbox(tbbox *bb, float *center){
    int d;
    for(d=0; d<bb->ndim; d++){
	center[d] = bb->rmin[d] + 0.5F*bb->sz[d];
    }
}

int 
ContainsBbox(tbbox *bb1, tbbox *bb2){
    /* Return 1 if bb1 completely contains bb2 */
    int d;

    for(d=0; d<bb1->ndim; d++){
	if( bb1->rmin[d] > bb2->rmin[d] ||
	   bb1->rmin[d] + bb1->sz[d] < bb2->rmin[d]+bb2->sz[d] )
	    return 0;
    }
    return 1;
}

/* Construct bbu, the 'union' of bb1 and bb2 */
void
UnionBbox(tbbox *bb1, tbbox *bb2, tbbox *bbu){
    int d;

    bbu->ndim = bb1->ndim;
    for(d=0; d<bb1->ndim; d++){
	float max1, max2;
	float min1, min2;

	/* Read these first in case bbu is equal to bb1 or bb2! */
	min1 = bb1->rmin[d];
	min2 = bb2->rmin[d];
	bbu->rmin[d] = (min1 < min2) ? min1 : min2;
	max1 = min1 + bb1->sz[d];
	max2 = min2 + bb2->sz[d];
	bbu->sz[d] = (max1 > max2) ? max1 - bbu->rmin[d] : max2 - bbu->rmin[d];
    }
}

/* Make the bbox a cube by expanding the smaller dimensions. */
void
CubeBbox(tbbox *bb){
    int d;
    float maxs, halfmaxs;
    float center[MAXNDIMKU];

    maxs = 0.;
    for(d=0; d<bb->ndim; d++) {
	center[d] = bb->rmin[d] + 0.5f * bb->sz[d];
	if( maxs < bb->sz[d] ) 
	    maxs = bb->sz[d];
    }
    halfmaxs = 0.5f*maxs;
    for( d=0; d<bb->ndim; d++){
	bb->rmin[d] = center[d] - halfmaxs;
	bb->sz[d] = maxs;
    }
}

/* Increase the linear dimension by 'factor' on all sides */
void
InflateBbox(tbbox *bb, float factor){
    float center;
    int d;
    for( d=0; d<bb->ndim; d++){
	center = 0.5f*bb->sz[d] + bb->rmin[d];
	bb->sz[d] *= factor;
	bb->rmin[d] = center - 0.5f*bb->sz[d];
    }
}

void
GenerateKeys(float *pstart, int nobj, int pstride, tbbox *bb, 
	     Key_t *kstart, int kstride, int order_type){
    int i, d;
    float keyfactor[MAXNDIMKU];
    unsigned int ik[MAXNDIMKU];
    unsigned int chubits;
    Key_t *kp;
    float *pos;
    unsigned int maxikey;
    float fmaxikey;
    Key_t (*KfI)(unsigned int *xp, int ndim, int nbits);

    chubits = ((KEYBITS-1) / bb->ndim );
    maxikey = (1U<<chubits) - 1;
    fmaxikey = (float)maxikey;
    for(d=0; d<bb->ndim; d++){
	/* Divide by 0 ?? */
	keyfactor[d] = (1U<<chubits)/(bb->sz[d]);
    }
    pos = pstart;
    kp = kstart;
    switch( order_type ){
    case MORTON_ORDER:
      KfI = &KeyFromInts;
      break;
    case PH_ORDER:
      assert(bb->ndim == 3);	/*  assumed by the current implementation*/
      KfI = &PHKeyFromInts;
      break;
    default:
      Error("Unrecognized order_type in GenerateKeys\n");
    }

    for(i=0; i<nobj; i++){
	for(d=0; d<bb->ndim; d++){
	    float fk = keyfactor[d] * (pos[d] - bb->rmin[d]);
	    if( fk < 0. ){
	      KeyOutOfBounds = 1;
	      ik[d] = 0;
	    }else if( fk > fmaxikey ){
	      ik[d] = maxikey;
	      KeyOutOfBounds = 1;
	    }else
	      ik[d] = (unsigned int)fk;
	}
	*kp = (*KfI)(ik, bb->ndim, chubits);
	kp = (Key_t *) ( kstride + (char *)kp);
	pos = (float *) ( pstride + (char *)pos);
    }
}

void 
CellBBFromKey(Key_t key, tbbox *bb, tbbox *cellbb, int order_type)
{
    unsigned int icorner[MAXNDIMKU];
    unsigned int iscale;
    float factor;
    int d;

    switch(order_type){
    case MORTON_ORDER:
      iscale = (1<<IntsFromKey(key, icorner, bb->ndim));
      break;
    case PH_ORDER:
      iscale = (1<<IntsFromPHKey(key, icorner, bb->ndim));
      break;
    default:
      Error("Unrecognized ordering in CellBBFromKey\n");
    }
      
    /* Now scale it back to "physical" units */
    cellbb->ndim = bb->ndim;
    for(d=0; d<bb->ndim; d++){
	factor = (bb->sz[d])/iscale;
	cellbb->rmin[d] = bb->rmin[d] + factor*icorner[d];
	cellbb->sz[d] = factor;
    }
}

void
IntsFromFloats(const float *x, unsigned int *ix, tbbox *bb, int nbits){
    int d;
    unsigned int iscale = 1<<nbits;

    if (nbits >= CHAR_BIT*sizeof(iscale)) {
	Error("nbits out of range in IntsFromFloats\n");
    }
    for(d=0; d<bb->ndim; d++){
	ix[d] = iscale * (x[d] - bb->rmin[d]) / (bb->sz[d]);
    }
}

void
FloatsFromInts(const int *ix, float *x, tbbox *bb, int nbits){
    unsigned int iscale = 1<<nbits;
    int d;
    if (nbits >= CHAR_BIT*sizeof(iscale)) {
	Error("nbits out of range in FloatsFromInts\n");
    }
    for(d=0; d<bb->ndim; d++){
	x[d] = ix[d] * ((bb->sz[d])/iscale);
    }
}

/* These two names conflict with physics_generic.c.  Good.  It will
   keep me from using physics_generic.c accidentally. */
Key_t 
KeyFromInts(unsigned int *xp, int ndim, int nbits){
    Key_t key;
    unsigned int rshift, bits, dim;

    /* Set first bit. Important! */
    key = KeyInt(1);
    for(rshift=nbits; rshift; ){
	rshift--;
	bits = 0;
	for(dim = 0; dim < ndim; dim++ )
	    bits |= ((xp[dim]>>rshift)&1) << dim;
	key = KeyOrInt(KeyLshift(key, ndim), bits);
    }
    return(key);
}

/* Notice that the return value is DIFFERENT FROM physics_generic.c 
 Here, we return the number of bits.  It's easier for the caller to do
 left-shift than ilog2. */
int
IntsFromKey(Key_t key, unsigned int *ip, int ndim){
    unsigned int iscale = 1;
    unsigned int lev = 0;
    int i;

    for(i=0; i<ndim; i++){
	ip[i] = 0;
    }
    while(KeyGT(key, KeyInt(1))) {
	for(i=0; i<ndim; i++){
	    if( KeyAndInt(key, (1<<i)) )
	      ip[i] |= iscale;
	}
	key = KeyRshift(key, ndim);
	iscale <<= 1;
	lev++;
	if (lev >= CHAR_BIT*sizeof(iscale)) {
	    Error("lev out of range in IntsFromKey\n");
	}
    }
    return lev;
}

