#ifndef NDIM
#define NDIM 3
#endif

#include "Msgs.h"
#include "Assert.h"
#include "key.h"
#include "vop.h"


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

static Key_t _PHKeyFromInts(unsigned int ikey[NDIM], unsigned int depth, int start, int type){
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

Key_t PHKeyFromInts(unsigned int ikey[NDIM], int ndim, unsigned int depth){
  assert(ndim == NDIM);
  return _PHKeyFromInts(ikey, depth, 0, 0);
}

/* Convert from a PH key to a NDIM-tuple of ints. */
/* Return the "depth" of the key */

static unsigned int 
_IntsFromPHKey(Key_t key, unsigned int ikey[NDIM], 
	  int depth, int start, int type){
    unsigned int lobits, unscrambled;
    unsigned int otype;
    unsigned int rshift, ret;
    Vxd(unsigned int out);
    Key_t keymax;
    Key_t key0;

    if( !setup_done )
	setup();
    keymax = KeyLshift(KeyInt(1), NDIM*depth);
    key0 = KeyInt(0);

    VxS(out, = 0);
    /* We need to start at the left, so we need to figure out where the */
    /* left of key is! */
    while( KeyEQ(KeyAnd(keymax, key), key0) ){
	keymax = KeyRshift(keymax, NDIM);
	depth--;
    }
    ret = depth;

    rshift = depth*NDIM;
    Msgf(("IntsFromPHKey(%s)\n", PrintKey(key)));
    while( rshift > 0 ){
        Key_t kr;
	rshift -= NDIM;
	depth--;
	kr = KeyRshift(key, rshift);
	lobits = KeyAndInt(kr, (1<<NDIM)-1);
	Msgf(("rshift=%d, kr=%s, lobits: %d\n", rshift, PrintKey(kr), lobits));
	unscrambled = revbitmap[start][type][lobits];
	
	out0 |= (unscrambled&1)<<depth;
#if NDIM > 1
	out1 |= ((unscrambled>>1)&1)<<depth;
#if NDIM > 2
	out2 |= ((unscrambled>>2)&1)<<depth;
#endif
#endif
	otype = type;
	type = typmap[start][otype][unscrambled];
	start = startmap[start][otype][unscrambled];
    }
    VVx(ikey, = out);
    return ret;
}

unsigned int 
IntsFromPHKey(Key_t key, unsigned int ikey[NDIM], int ndim){
  assert(ndim == NDIM);
  return _IntsFromPHKey(key, ikey, TreeLevel(key, NDIM), 0, 0);
}

#ifdef STANDALONE
#define MAXDEPTH (15/NDIM)

/* The loops here are just too hard to deal with for generic NDIM. */
/* And in two-d, we can make Postscript output! */
#if NDIM == 3
main(int argc, char **argv){
    int depth;
    unsigned int i, ikey;
    unsigned int ik[NDIM];
    int type, start;
    unsigned int revk[NDIM], revd;
    Key_t key, base;

    depth = atoi(argv[1]);
    start = atoi(argv[2]);
    type = atoi(argv[3]);
    if( depth < 0 )
	Error("bad depth\n");
    if( type < 0 || type >= NDIM )
	Error("bad type\n");
    if( start < 0 || start >= 1<<(NDIM-1) )
	Error("bad start");

    /* Test each ikey in a 3-d grid.  Make sure that 
       _IntsFromKey(KeyFromInts(ik)) == ik  */
    for(ik[0]=0; ik[0]<(1<<depth); ik[0]++){
	for(ik[1]=0; ik[1]<(1<<depth); ik[1]++){
	    for(ik[2]=0; ik[2]<(1<<depth); ik[2]++){
		key = _PHKeyFromInts(ik, depth, start, type);
		revd = _IntsFromPHKey(key, revk, depth, start, type);
		assert(revd == depth );
		if( revk[0]!=ik[0] || revk[1]!=ik[1] || revk[2]!=ik[2] )
		    Warning("ik=(%x %x %x) -> %s -> revk=(%x %x %x)\n",
				   ik[0], ik[1], ik[2],
				   PrintKey(key),
				   revk[0], revk[1], revk[2]);
	    }
	}
    }

    /* Test each key starting at 0. Make sure that 
       KeyFromInts(IntsFromKey(k))==k */
    base = KeyLshift(KeyInt(1), depth*NDIM);
    for(ikey=0; ikey < (1<<(NDIM*depth)); ikey++){
      Key_t revkey;
      unsigned int di[NDIM];
      unsigned int iklast[NDIM];
      int d2;

      revd = _IntsFromPHKey(base, ik, depth, start, type);
      assert(revd == depth);
      revkey = _PHKeyFromInts(ik, depth, start, type);
      if( KeyNEQ(revkey, base)){
	char s[64];
	strcpy(s, PrintKey(revkey));
	Warning("key %s -> %x %x %x -> %s\n",
		       PrintKey(base), ik[0], ik[1], ik[2], s);
      }
      VVV(di, = ik, - iklast);
      d2 = Dot(di, di);
      if( d2 != 1 && ikey ){
	Warning("Move by more than 1 at ikey=%#0o\n", ikey);
      }
	
      VV(iklast, = ik);
      base = KeyAddInt(base, 1);
    }
    exit(0);
}

#endif

#if NDIM==2

#define SZ 6. /* inches */
main(int argc, char **argv){
    int depth;
    unsigned int i, ikey;
    unsigned int ik[NDIM];
    int type, start;
    unsigned int revk[NDIM], revd;
    Key_t key, base;

    depth = atoi(argv[1]);
    start = atoi(argv[2]);
    type = atoi(argv[3]);
    if( depth < 0 )
	Error("bad depth\n");
    if( type < 0 || type >= NDIM )
	Error("bad type\n");
    if( start < 0 || start >= 1<<(NDIM-1) )
	Error("bad start");

    Warning("This is test of the emergency warning system\n");
    /* Test each ikey in a 3-d grid.  Make sure that 
       _IntsFromKey(KeyFromInts(ik)) == ik  */
    for(ik[0]=0; ik[0]<(1<<depth); ik[0]++){
	for(ik[1]=0; ik[1]<(1<<depth); ik[1]++){
	  key = _PHKeyFromInts(ik, depth, start, type);
	  revd = _IntsFromPHKey(key, revk, depth, start, type);
	  assert(revd == depth );
	  if( revk[0]!=ik[0] || revk[1]!=ik[1])
	    Warning("ik=(%x %x) -> %s -> revk=(%x %x)\n",
			   ik[0], ik[1],
			   PrintKey(key),
			   revk[0], revk[1]);
	}
    }

    /* Test each key starting at 0. Make sure that 
       KeyFromInts(IntsFromKey(k))==k */
    printf("%%!\n");
    printf("/L {2 copy lineto stroke moveto pop} def\n");
    printf("72 72 translate\n");
    printf("%%scale to 6 inches width, hgt\n");
    printf("%g %g scale\n", 72.*SZ/(1<<depth), 72.*SZ/(1<<depth));
    printf("%% line width of 1pt\n");
    printf("%g setlinewidth\n", (1<<depth)/(SZ*72));
    printf("0 0 moveto\n");

    base = KeyLshift(KeyInt(1), depth*NDIM);
    for(ikey=0; ikey < (1<<(NDIM*depth)); ikey++){
      Key_t revkey;
      unsigned int di[NDIM];
      unsigned int iklast[NDIM];
      int d2;

      revd = _IntsFromPHKey(base, ik, depth, start, type);
      assert(revd == depth);
      revkey = _PHKeyFromInts(ik, depth, start, type);
      if( KeyNEQ(revkey, base)){
	char s[64];
	strcpy(s, PrintKey(revkey));
	Warning("key %s -> %x %x -> %s\n",
		       PrintKey(base), ik[0], ik[1], s);
      }
      printf("%d %d %d L\n", i, ik[0], ik[1]);
      VVV(di, = ik, - iklast);
      d2 = Dot(di, di);
      if( d2 != 1 && ikey ){
	Warning("Move by more than 1 at ikey=%#0o\n", ikey);
      }

      VV(iklast, = ik);
      base = KeyAddInt(base, 1);
    }
    printf("showpage\n");
    exit(0);
}

#endif /* NDIM==2 */
#endif /* STANDALONE */
