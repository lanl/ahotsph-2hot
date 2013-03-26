/* This file has source code which is generically useful to programs */
/* that use libtree.  However, this code depends on how the "application" */
/* chooses to lay out it's bodies, so it can't be put in a library. */
/* Thus, the typical action is to #include "physics_generic.c" */
/* in one of the .c files associated with the "application", after */
/* body is typedef'ed. */

#ifdef STANDALONE
typedef struct{float pos[NDIM];} body;
typedef struct{float pos[NDIM];} cell;
#define Pos(p) (p->pos)
#include "physics_generic.h"
#endif

#include <math.h>
#include <stddef.h>
#include <float.h>
#include "timers.h"
#include "protos.h"
#include "Malloc.h"
#include "mpmy.h"
#include "vop.h"
#include "Msgs.h"
#include "Assert.h"
#include "verify.h"
#include "files.h"
#include "key.h"

/* Using HUGE (from math.h) doesn't work because we assign it to float */
/* We could use something from float.h */
#ifndef FLT_MAX
/* I wonder what they put in float.h, anyway */
#define FLT_MAX 1.e38
#endif

/* Used below to give a bit of extra breathing space to rsize/rmin */
#define RSIZE_EPS  (.005)

static float Rmin[NDIM], Rsize;
static float keyfactor;
static Key_t
phkey(unsigned int ikey[NDIM], unsigned int depth, int start, int type);
static unsigned int
phkey_rev(Key_t key, unsigned int ikey[NDIM], int depth, int start, int type);

Counter_t GetKeyCnt;

/* Call FindBbox to learn what the extent of the system is. */
void
FindBbox(body *bp, int n, float *rmin, float *rmax)
{
    body *b;
    MPMY_Comm_request req;

    VS(rmax, = -FLT_MAX);
    VS(rmin, = FLT_MAX);
    for(b=bp; b<&bp[n]; b++){
	VVVV(if LPAREN rmin, > b->pos, RPAREN rmin, = b->pos);
	VVVV(if LPAREN rmax, < b->pos, RPAREN rmax, = b->pos);
	VS(if LPAREN !finite LPAREN b->pos, RPAREN RPAREN 
	     	  Error("Bad value for particle %ld of %d\n",
		(long)(b-bp), n));
    }
    MPMY_ICombine_Init(&req);
    MPMY_ICombine(rmin, rmin, NDIM, MPMY_FLOAT, MPMY_MIN, req);
    MPMY_ICombine(rmax, rmax, NDIM, MPMY_FLOAT, MPMY_MAX, req);
    MPMY_ICombine_Wait(req);
}    

/* Call FixRsize to set the limits for key-generation. */
float 
FixRsize(float *rmin, float *rmax)
{
    float d;
    float size[NDIM];
    float center[NDIM];

    VVV( size, = rmax, - rmin );
    VVVS( center, = LPAREN rmax, + rmin, RPAREN*0.5 );
    d = size[0];
#if NDIM > 1
    if( d < size[1] ) d = size[1];
#if NDIM > 2
    if( d < size[2] ) d = size[2];
#endif
#endif
    /* Now add a little bit of breathing room on both ends */
    d *= (1.0 + 2.*RSIZE_EPS);
    VV( Rmin, = -0.5*d + center );
    keyfactor = MAXCHU/d;
    Rsize = d;
    Msgf(("Fixrsize: rmin=(" Sinfix("%g", " ") "), Rsize=%g, keyfactor=%g\n",
	  Vinfix(Rmin, COMMA), Rsize, keyfactor));
    return Rsize;
}    

/* The other fix_rsize causes mesh artifacts if do_periodic is true */
float 
FixRsizeExact(float *rmin, float *rmax)
{
    float d;
    float size[NDIM];
    float center[NDIM];

    VVV( size, = rmax, - rmin );
    VVVS( center, = LPAREN rmax, + rmin, RPAREN*0.5 );
    d = size[0];
#if NDIM > 1
    if( d < size[1] ) d = size[1];
#if NDIM > 2
    if( d < size[2] ) d = size[2];
#endif
#endif
    VV( Rmin, = -0.5*d + center );
    keyfactor = MAXCHU/d;
    Rsize = d;
    Msgf(("Fixrsize: rmin=(" Sinfix("%g", " ") "), Rsize=%g, keyfactor=%g\n",
	  Vinfix(Rmin, COMMA), Rsize, keyfactor));
    return Rsize;
}    

static void IntPos(const body *p, unsigned int *xp){
    float pos[NDIM];	/* pos might be double */

    VV(pos, = p->pos);
    /* How expensive is this test??? */
    if( VVinfix(pos, < Rmin, ||) || VVinfix(pos, >Rsize+Rmin, ||) ){
	Msg_do("Rsize=%g\n", Rsize);
	Msg_do("pos=" Sinfix("%g", " ")"\n", Vinfix(pos, COMMA));
	Msg_do("Rmin=" Sinfix("%g", " ")"\n", Vinfix(Rmin, COMMA));
	Msg_do("Rmax=" Sinfix("%g", " ")"\n", Vinfix(Rsize+Rmin, COMMA));
	Error("Pos outside universe!\n");
    }
    VVVS( xp, = (int)LPAREN keyfactor*LPAREN pos, - Rmin, RPAREN RPAREN );
}


Key_t GetKey(const body *p)
{
    unsigned int xp[NDIM];
    Vxd(unsigned int xp);
    Key_t key;
    unsigned int rshift, bits;

    IntPos(p, xp);
    VxV(xp, = xp);
    /* Set first bit. Important! */
    key = KeyInt(1);
    for(rshift=CHUBITS; rshift; ){
	rshift--;
	bits = (xp0>>rshift)&1;
#if NDIM > 1
	bits |= ((xp1>>rshift)&1) << 1;
#if NDIM > 2
	bits |= ((xp2>>rshift)&1) << 2;
#endif
#endif
	key = KeyOrInt(KeyLshift(key, NDIM), bits);
    }
    IncrCounter(&GetKeyCnt);
    return(key);
}

Key_t GetKeyPH(const body *p)
{
    unsigned int xp[NDIM];
    Key_t key;

    IntPos(p, xp);
    key = phkey(xp, CHUBITS, 0, 0);
    IncrCounter(&GetKeyCnt);
    return(key);
}


void 
CellCorner(Key_t key, float *corner, float *size)
{
    unsigned int icorner[NDIM];
    unsigned int iscale = 1;
    float factor;
    int i;

    VS(icorner, = 0);
    while(KeyGT(key, KeyInt(1))) {
	for(i=0; i<NDIM; i++){
	    if( KeyAndInt(key, (1<<i)) )
	      icorner[i] |= iscale;
	}
	key = KeyRshift(key, NDIM);
	iscale <<= 1;
    }
    /* Now scale it back to "physical" units */
    factor = Rsize/iscale;
    VVV(corner, = Rmin, +factor*icorner);
    if( size ){
	*size = factor;
    }
}

void 
CellCornerPH(Key_t key, float *corner, float *size)
{
    unsigned int icorner[NDIM];
    unsigned int ifactor;
    float factor;

    ifactor = phkey_rev(key, icorner, CHUBITS, 0, 0);
    factor = Rsize/(float)ifactor;
    /* Now scale it back to "physical" units */
    factor = Rsize/((long long)1<<CHUBITS);
    VVV(corner, = Rmin, +factor*icorner);
    if( size ){
	*size = factor;
    }
}

#ifdef HAS_KEY
/* Presumably ptr->key has been previously filled with either */
/* GetKey() or GetKeyPH */
Key_t GetKeyFromStruct(const body *ptr){
    return ptr->key;
}

void FixKeys(body *btab, int nobj, Key_t (*func)(const body *)){
    body *btabend= btab+nobj;

    while(btab<btabend){
	btab->key = func(btab);
	btab++;
    }
}
#endif /* HAS_KEY */

float GetCost(const body *ptr)
{
#ifdef HAS_NTERMS
    return (float) ptr->nterms;
#else
    return 1.0;
#endif
}

#ifdef HAS_IDENT
#include "gc.h"

void FixId(body *btab, int nobj, int gnobj){
    int start;
    int mynobj;
    int i;

    NobjInitial(gnobj, MPMY_Nproc(), MPMY_Procnum(), &mynobj, &start);
    VerifyX(mynobj == nobj, Shout("mynobj=%d, nobj=%d, start=%d, gnobj=%d, nproc=%d, procnum=%d\n", mynobj, nobj, start, gnobj, MPMY_Nproc(), MPMY_Procnum()));
    for(i=0; i<nobj; i++){
	btab[i].ident = start+i;
    }
}

void FixId64(body *btab, int nobj, int64_t gnobj){
    int64_t start;
    int mynobj;
    int i;

    NobjInitial64(gnobj, MPMY_Nproc(), MPMY_Procnum(), &mynobj, &start);
    VerifyX(mynobj == nobj, Shout("mynobj=%d, nobj=%d, start=%ld, gnobj=%ld, nproc=%d, procnum=%d\n", mynobj, nobj, start, gnobj, MPMY_Nproc(), MPMY_Procnum()));
    for(i=0; i<nobj; i++){
	btab[i].ident = start+i;
    }
}

Key_t OutIdentKey(const outbody *bp)
{
    return KeyLshift(KeyInt(bp->ident), KEYBITS/2);
}

Key_t OutIdent64Key(const outbody *bp)
{
    return KeyInt(bp->ident);
}
#endif

#ifdef HAS_NTERMS
void FixNterms(body *btab, int nobj){
    int i;
    for(i=0; i<nobj; i++){
	btab[i].nterms = 1;
    }
}
#endif

/* Use this to sort by "ident" for output */
float UnityCost(const void *ptr){
    return 1.0;
}

/* The long-awaited peano-hilbert key. */
/* "When the going gets wierd, the wierd turn pro." */
/* Interestingly, it isn't particularly more complicated by virtue */
/* of the arbitrary NDIM support.  The NDIM=3 only code was essentially */
/* the same except for some loop indices */

#if NDIM==3
/* The possible places to start. */
/* They can only have an even number of bits turned on. */
#define S000 0
#define S011 1
#define S101 2
#define S110 3
/* one for each of the possible startindices */
static int sindex_to_mask[1<<(NDIM-1)] = {0, 3, 5, 6};
static int smask_to_index[1<<NDIM] = {S000, -1, -1, S011, -1, S101, S110, -1};

/* The possible "path-types" (there are NDIM of them) */
#define Px 0
#define Py 1
#define Pz 2

static int pindex_to_mask[NDIM] = {4, 2, 1};

/* What are the large transitions in each of the three basic paths? */
static int bigtrans[NDIM][1<<NDIM] = {
    {Py, Pz, Py, Px, Py, Pz, Py, Px}, /* Px */
    {Px, Pz, Px, Py, Px, Pz, Px, Py}, /* Py */
    {Px, Py, Px, Pz, Px, Py, Px, Pz} /* Pz */
};

/* What are the small transitions on each of the NDIM basic paths? */
static int ltltrans[NDIM][1<<NDIM] = {
    {Py, Pz, Pz, Px, Px, Pz, Pz, Py}, /* Px */
    {Px, Pz, Pz, Py, Py, Pz, Pz, Px}, /* Py */
    {Px, Py, Py, Pz, Pz, Py, Py, Px} /* Pz */
};

#endif /* NDIM==3 */

#if NDIM==2
/* The possible places to start. */
/* They can only have an even number of bits turned on. */
#define S00 0
#define S11 1

/* one for each of the possible startindices */
static int sindex_to_mask[1<<(NDIM-1)] = {0, 3};
static int smask_to_index[1<<NDIM] = {S00, -1, -1, S11};

/* The possible "path-types" (there are NDIM of them) */
#define Px 0
#define Py 1

static int pindex_to_mask[NDIM] = {2, 1};

/* What are the large transitions in each of the NDIM basic paths? */
static int bigtrans[NDIM][1<<NDIM] = {
    {Py, Px, Py, Px}, /* Px */
    {Px, Py, Px, Py} /* Py */
};

/* What are the small transitions on each of the three basic paths? */
static int ltltrans[NDIM][1<<NDIM] = {
    {Py, Px, Px, Py}, /* Px */
    {Px, Py, Py, Px} /* Py */
};

#endif /* NDIM==2 */

static int bitmap[1<<(NDIM-1)][NDIM][1<<NDIM]; /* range 1<<NDIM */
static int revbitmap[1<<(NDIM-1)][NDIM][1<<NDIM]; /* range 1<<NDIM */
static int startmap[1<<(NDIM-1)][NDIM][1<<NDIM]; /* range 1<<(NDIM-1) */
static int typmap[1<<(NDIM-1)][NDIM][1<<NDIM]; /* range NDIM */
static int setup_done;

static void setup(void){
    unsigned int start, typ, j;
    unsigned int smap[1<<NDIM], bmap[1<<NDIM], lmap[1<<NDIM];
    unsigned int bt, lt, bm, st;

    for(typ=0; typ<NDIM; typ++){
	for(start=0; start<(1<<(NDIM-1)); start++){
	    bm = st = sindex_to_mask[start];
	    for(j=0; j<(1<<NDIM); j++){
		smap[j] = smask_to_index[st];
		lmap[j] = ltltrans[typ][j];
		bmap[j] = bm;
		bt = pindex_to_mask[ bigtrans[typ][j] ];
		lt = pindex_to_mask[ ltltrans[typ][j] ];
		bm = bm^bt;
		st = st^(bt^lt);
	    }
	    /* This little wierdness arranges that map works in the */
	    /* right direction...really */
	    for(j=0; j<(1<<NDIM); j++){
		bm = bmap[j];
		startmap[start][typ][bm] = smap[j];
		bitmap[start][typ][bm] = j;
		revbitmap[start][typ][j] = bm; /* a shot in the dark */
		typmap[start][typ][bm] = lmap[j];
	    }
	}
    }
    setup_done = 1;
}

static 
Key_t phkey(unsigned int ikey[NDIM], unsigned int depth, int start, int type){
    Key_t ret;
    unsigned int bits;
    unsigned int rshift;
    unsigned int otype;
    Vxd(unsigned int ikey);
    ret = KeyInt(1);

    if( !setup_done )
	setup();

    VxV(ikey, = ikey);
    for(rshift=depth; rshift; ){
	rshift--;
	bits = (ikey0>>rshift)&1;
#if NDIM > 1
	bits |= ((ikey1>>rshift)&1) << 1;
#if NDIM > 2
	bits |= ((ikey2>>rshift)&1) << 2;
#endif
#endif
	ret = KeyOrInt(KeyLshift(ret, NDIM), bitmap[start][type][bits]);
	otype = type;
	type = typmap[start][otype][bits];
	start = startmap[start][otype][bits];
    }
    return ret;
}

/* Convert from a PH key to a NDIM-tuple of ints. */
/* Return the "depth" of the key, (in it's mask form, i.e., 010...0) */
static unsigned int 
phkey_rev(Key_t key, unsigned int ikey[NDIM], 
	  int depth, int start, int type){
    unsigned int lobits, unscrambled;
    unsigned int otype;
    unsigned int rshift, ret;
    Vxd(unsigned int out);
    Key_t keymax;
    Key_t key0;

    keymax = KeyLshift(KeyInt(1), NDIM*depth);
    key0 = KeyInt(0);

    VxS(out, = 0);
    /* We need to start at the left, so we need to figure out where the */
    /* left of key is! */
    while( KeyEQ(KeyAnd(keymax, key), key0) ){
	keymax = KeyRshift(keymax, NDIM);
	depth--;
    }
    ret = 1<<depth;

    rshift = depth*NDIM;
    while( rshift > 0 ){
	rshift -= NDIM;
	depth--;
	lobits = KeyAndInt(KeyRshift(key, rshift), (1<<NDIM)-1);
	unscrambled = revbitmap[start][type][lobits];
	
	out0 |= (unscrambled&1)<<depth;
#if NDIM > 1
	out1 |= (unscrambled&(1<<1))<<depth;
#if NDIM > 2
	out2 |= (unscrambled&(1<<2))<<depth;
#endif
#endif
	otype = type;
	type = typmap[start][otype][unscrambled];
	start = startmap[start][otype][lobits];
	key = KeyRshift(key, NDIM); 
    }
    VVx(ikey, = out);
    return ret;
}

#ifdef STANDALONE
#define MAXDEPTH (15/NDIM)

/* The loops here are just too hard to deal with for generic NDIM. */
/* And in two-d, we can make Postscript output! */
#if NDIM == 3
main(int argc, char **argv){
    int depth;
    int outx[1<<(NDIM*MAXDEPTH)];
    int outy[1<<(NDIM*MAXDEPTH)];
    int outz[1<<(NDIM*MAXDEPTH)];
    unsigned int i, ikey;
    unsigned int ik[NDIM];
    unsigned int outxlast, outylast, outzlast;
    int dx, dy, dz, d2;
    int type, start;
    unsigned int revk[NDIM], revd;
    Key_t key;

    depth = atoi(argv[1]);
    start = atoi(argv[2]);
    type = atoi(argv[3]);
    if( depth > MAXDEPTH || depth < 0 )
	Error("bad depth\n");
    if( type < 0 || type >= NDIM )
	Error("bad type\n");
    if( start < 0 || start >= 1<<(NDIM-1) )
	Error("bad start");

    for(i=0; i<(1<<(NDIM*depth)); i++){
	outx[i] = -1;
	outy[i] = -1;
	outz[i] = -1;
    }

    for(ik[0]=0; ik[0]<(1<<depth); ik[0]++){
	for(ik[1]=0; ik[1]<(1<<depth); ik[1]++){
	    for(ik[2]=0; ik[2]<(1<<depth); ik[2]++){
		key = phkey(ik, depth, start, type);
		ikey = KeyAndInt(key, (1<<(NDIM*depth))-1) ;	    
		if( outx[ikey] != -1 || outy[ikey] != -1 || outz[ikey] != -1)
		    Error("revisiting ikey=%d\n", ikey);
		revd = phkey_rev(key, revk, depth, start, type);
		assert(revd == 1<<depth );
		if( revk[0]!=ik[0] || revk[1]!=ik[1] || revk[2]!=ik[2] )
		    Warning("ik=(%x %x %x), revk=(%x %x %x)\n",
			    ik[0], ik[1], ik[2],
			    revk[0], revk[1], revk[2]);

		outx[ikey] = ik[0];
		outy[ikey] = ik[1];
		outz[ikey] = ik[2];
	    }
	}
    }
    outxlast = 0;
    outylast = 0;
    outzlast = 0;
    for(i=0; i<(1<<(NDIM*depth)); i++){
	/* Assert that only one value has changed by at most 1. */
	dx = outx[i] - outxlast;
	dy = outy[i] - outylast;
	dz = outz[i] - outzlast;
	d2 = dx*dx + dy*dy + dz*dz;
	if( d2 != 1 && i )
	    Warning("transition (i=%d) from %d %d %d to %d %d %d\n",
		    i, outxlast, outylast, outzlast,
		    outx[i], outy[i], outz[i]);
	outxlast = outx[i];
	outylast = outy[i];
	outzlast = outz[i];
    }
    exit(0);
}

#endif

#if NDIM==2

#define SZ 6. /* inches */

main(int argc, char **argv){
    int depth = atoi(argv[1]);
    int outx[1<<(NDIM*MAXDEPTH)];
    int outy[1<<(NDIM*MAXDEPTH)];
    unsigned int i, ikey;
    unsigned int ik[NDIM];
    unsigned int revk[NDIM];
    unsigned int revd;
    Key_t key;

    if( depth > MAXDEPTH)
	Error("depth too large\n");

    if( depth < 0 )
	Error("negative depth\n");

    for(i=0; i<(1<<(NDIM*depth)); i++){
	outx[i] = -1;
	outy[i] = -1;
    }

    printf("%%!\n");
    printf("/L {2 copy lineto stroke moveto pop} def\n");
    printf("72 72 translate\n");
    printf("%%scale to 6 inches width, hgt\n");
    printf("%g %g scale\n", 72.*SZ/(1<<depth), 72.*SZ/(1<<depth));
    printf("%% line width of 1pt\n");
    printf("%g setlinewidth\n", (1<<depth)/(SZ*72));
    printf("0 0 moveto\n");

    printf("%%sorted by xy\n");
    for(ik[0]=0; ik[0]<(1<<depth); ik[0]++){
	for(ik[1]=0; ik[1]<(1<<depth); ik[1]++){
	    key = phkey(ik, depth, 0, 0);
	    ikey = KeyAndInt(key, (1<<(2*depth))-1) ;
	    revd = phkey_rev(key, revk, depth, 0, 0);
	    
	    if( revd != 1<<depth )
		Error("Depths wrong in phkey_rev!\n");

	    if( ik[0] != revk[0] || ik[1] != revk[1] )
		Warning("ik=(%d, %d), revk=(%d,%d)\n", 
			ik[0], ik[1], revk[0], revk[1]);
	    
	    if( outx[ikey] != -1 || outy[ikey] != -1 )
		Warning("revisiting ikey=%d\n", ikey);

	    printf("%%%d %d %d\n", ik[0], ik[1], ikey);
	    outx[ikey] = ik[0];
	    outy[ikey] = ik[1];
	}
    }
    printf("%% sorted by key\n");
    for(i=0; i<(1<<(NDIM*depth)); i++){
	printf("%d %d %d L\n", i, outx[i], outy[i]);
    }
    printf("showpage");
    exit(0);
}

#endif /* NDIM==2 */
#endif /* STANDALONE */
