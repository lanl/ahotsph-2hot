#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <float.h>
#include "Malloc.h"
#include "macr.h"
#define NDIM 3
#include "vop.h"
#include "error.h"
#include "mpmy.h"
#include "Msgs.h"
#include "SDF.h"
#include "SDFwrite.h"
#include "pqsort.h"
#include "singlio.h"
#include "version_2HOT.h"

#define BITS_PER_DIM ((KEYBITS-1)/NDIM)
#define IDENTMASK ((1LL<<48)-1)	/* use high bits for other purposes */

typedef struct {
    float pos[NDIM];
    float vel[NDIM];
    float mvir, m200b, m200c, m500c, m2500c;
    float rvir, r200b, r500c;
    float radius, rs, rvmax;
    float vrms, vmax, spin, kin_to_pot;
    int64_t id, mvir_pid, m200b_pid, m500c_pid;
} __attribute__ ((packed)) body;

#define OUTBODYDESC \
"struct {\n\
    float x, y, z;\n\
    float vx, vy, vz;\n\
    float mvir, m200b, m200c, m500c, m2500c;\n\
    float rvir, r200b, r500c; /* Mpcccm/h */\n\
    float radius, rs, rvmax;  /* kpc/h */\n\
    float vrms, vmax, spin, kin_to_pot;\n\
    int64_t id, mvir_pid, m200b_pid, m500c_pid; /* pid is -1 if not subhalo */\n\
}"

static float Rmin[NDIM], Rsize[NDIM];
static float keyfactor[NDIM];

void
FixRsizeExact(const float rmin[NDIM], const float rmax[NDIM])
{
    float size[NDIM];
    float center[NDIM];

    VVV(size, = rmax, - rmin);
    VVVS(center, = LPAREN rmax, + rmin, RPAREN*0.5f);
    VS(size, *= (1.0f + 4.0f*FLT_EPSILON));
    VV(keyfactor, = (1LL<<BITS_PER_DIM)/size);
    VV(Rsize, = size);
    VVV(Rmin, = -0.5f*size, + center);
}

void 
CellCorner(Key_t key, float corner[NDIM], float size[NDIM])
{
    unsigned int icorner[NDIM];
    unsigned int iscale = 1;
    float factor[NDIM];
    int i;

    VS(icorner, = 0);
    while (KeyGT(key, KeyInt(1))) {
	for (i = 0; i < NDIM; i++) {
	    if (KeyAndInt(key, (1<<i)))
		icorner[i] |= iscale;
	}
	key = KeyRshift(key, NDIM);
	iscale <<= 1;
    }
    /* Now scale it back to "physical" units */
    VV(factor, = (1.0f/iscale)*Rsize);
    VVVV(corner, = Rmin, + factor, * icorner);
    if (size) {
	VV(size, = factor);
    }
}


/* for i in range(256): */
/*     print '%d, ' % (i&1 | (i>>1&1)<<3 | (i>>2&1)<<6 | (i>>3&1)<<9 | (i>>4&1)<<12 | (i>>5&1)<<15 | (i>>6&1)<<18 | (i>>7&1)<<21) */
static const uint32_t morton[256] = {
    0, 1, 8, 9, 64, 65, 72, 73, 512, 513, 520, 521, 576, 577, 584, 585,
    4096, 4097, 4104, 4105, 4160, 4161, 4168, 4169, 4608, 4609, 4616,
    4617, 4672, 4673, 4680, 4681, 32768, 32769, 32776, 32777, 32832,
    32833, 32840, 32841, 33280, 33281, 33288, 33289, 33344, 33345, 33352,
    33353, 36864, 36865, 36872, 36873, 36928, 36929, 36936, 36937, 37376,
    37377, 37384, 37385, 37440, 37441, 37448, 37449, 262144, 262145,
    262152, 262153, 262208, 262209, 262216, 262217, 262656, 262657,
    262664, 262665, 262720, 262721, 262728, 262729, 266240, 266241,
    266248, 266249, 266304, 266305, 266312, 266313, 266752, 266753,
    266760, 266761, 266816, 266817, 266824, 266825, 294912, 294913,
    294920, 294921, 294976, 294977, 294984, 294985, 295424, 295425,
    295432, 295433, 295488, 295489, 295496, 295497, 299008, 299009,
    299016, 299017, 299072, 299073, 299080, 299081, 299520, 299521,
    299528, 299529, 299584, 299585, 299592, 299593, 2097152, 2097153,
    2097160, 2097161, 2097216, 2097217, 2097224, 2097225, 2097664,
    2097665, 2097672, 2097673, 2097728, 2097729, 2097736, 2097737,
    2101248, 2101249, 2101256, 2101257, 2101312, 2101313, 2101320,
    2101321, 2101760, 2101761, 2101768, 2101769, 2101824, 2101825,
    2101832, 2101833, 2129920, 2129921, 2129928, 2129929, 2129984,
    2129985, 2129992, 2129993, 2130432, 2130433, 2130440, 2130441,
    2130496, 2130497, 2130504, 2130505, 2134016, 2134017, 2134024,
    2134025, 2134080, 2134081, 2134088, 2134089, 2134528, 2134529,
    2134536, 2134537, 2134592, 2134593, 2134600, 2134601, 2359296,
    2359297, 2359304, 2359305, 2359360, 2359361, 2359368, 2359369,
    2359808, 2359809, 2359816, 2359817, 2359872, 2359873, 2359880,
    2359881, 2363392, 2363393, 2363400, 2363401, 2363456, 2363457,
    2363464, 2363465, 2363904, 2363905, 2363912, 2363913, 2363968,
    2363969, 2363976, 2363977, 2392064, 2392065, 2392072, 2392073,
    2392128, 2392129, 2392136, 2392137, 2392576, 2392577, 2392584,
    2392585, 2392640, 2392641, 2392648, 2392649, 2396160, 2396161,
    2396168, 2396169, 2396224, 2396225, 2396232, 2396233, 2396672,
    2396673, 2396680, 2396681, 2396736, 2396737, 2396744, 2396745
};

Key_t GetKeyFast(const body *p)
{
    uint32_t xp0, xp1, xp2;
    uint32_t k0, k1, k2, k3;
    Key_t key = {{0, 1<<29}};

    if (p->pos[0] < Rmin[0] || p->pos[1] < Rmin[1] || p->pos[2] < Rmin[2] ||
	p->pos[1] >= Rmin[0]+Rsize[0] || p->pos[1] >= Rmin[1]+Rsize[1] || p->pos[2] >= Rmin[2]+Rsize[2])
	Error("pos outside bounds\n");

    xp0 = keyfactor[0] * (p->pos[0] - Rmin[0]);
    xp1 = keyfactor[1] * (p->pos[1] - Rmin[1]);
    xp2 = keyfactor[2] * (p->pos[2] - Rmin[2]);

    k0 = morton[xp0 & 0xff] | morton[xp1 & 0xff] << 1 | morton[xp2 & 0xff] << 2;
    k1 = morton[xp0 >> 8 & 0xff] | morton[xp1 >> 8 & 0xff] << 1 | morton[xp2 >> 8 & 0xff] << 2;
    k2 = morton[xp0 >> 16 & 0xff] | morton[xp1 >> 16 & 0xff] << 1 | morton[xp2 >> 16 & 0xff] << 2;
    k3 = morton[xp0 >> 24 & 0xff] | morton[xp1 >> 24 & 0xff] << 1 | morton[xp2 >> 24 & 0xff] << 2;

    key.k[0] = (k2 & 0xffff);
    key.k[0] <<= 24;
    key.k[0] |= k1;
    key.k[0] <<= 24;
    key.k[0] |= k0;
    key.k[1] |= k3 << 8 | k2 >> 16;

    return(key);
}

Key_t GetKeySphericalFast(const body *p)
{
    uint32_t xp0, xp1, xp2;
    uint32_t k0, k1, k2, k3;
    Key_t key = {{0, 1<<29}};

    float r = sqrtf(Dot(p->pos, p->pos));
    float theta = acosf(p->pos[2]/r);
    float phi = atan2f(p->pos[1], p->pos[0]);

    if (r < Rmin[0] || r >= Rmin[0]+Rsize[0]) Error("r %g limits %g %g\n", r, Rmin[0], Rmin[0]+Rsize[0]);
    if (theta < Rmin[1] || theta >= Rmin[1]+Rsize[1]) Error("theta %g limits %g %g\n", theta, Rmin[1], Rmin[1]+Rsize[1]);
    if (phi < Rmin[2] || phi >= Rmin[2]+Rsize[2]) Error("phi %g limits %g %g\n", phi, Rmin[2], Rmin[2]+Rsize[2]);

    xp0 = keyfactor[0] * (r - Rmin[0]);
    xp1 = keyfactor[1] * (theta - Rmin[1]);
    xp2 = keyfactor[2] * (phi - Rmin[2]);

    k0 = morton[xp0 & 0xff] | morton[xp1 & 0xff] << 1 | morton[xp2 & 0xff] << 2;
    k1 = morton[xp0 >> 8 & 0xff] | morton[xp1 >> 8 & 0xff] << 1 | morton[xp2 >> 8 & 0xff] << 2;
    k2 = morton[xp0 >> 16 & 0xff] | morton[xp1 >> 16 & 0xff] << 1 | morton[xp2 >> 16 & 0xff] << 2;
    k3 = morton[xp0 >> 24 & 0xff] | morton[xp1 >> 24 & 0xff] << 1 | morton[xp2 >> 24 & 0xff] << 2;

    key.k[0] = (k2 & 0xffff);
    key.k[0] <<= 24;
    key.k[0] |= k1;
    key.k[0] <<= 24;
    key.k[0] |= k0;
    key.k[1] |= k3 << 8 | k2 >> 16;

    return(key);
}

float UnityCost(const void *ptr) /* load balance cost for *ptr */
{
    return 1.0;
}

int
main(int argc, char *argv[])
{
    int nfiles;
    char *filelist, sdfhdr[256];
    char outname[256];
    struct stat sb;
    SDF *sdfp;
    int64_t gnobj, nobj;
    double L0;
    double Omega0_m, Omega0_lambda, h_100;
    double redshift;
    double tpos;
    float particle_mass;
    double a;
    body *btab = NULL;

    MPMY_Init(&argc, &argv);

    if (argc != 5) {
	fprintf(stderr, "usage: %s outname file.hdr filenames nfiles\n", argv[0]);
	exit(1);
    }
    singlPrintf("Welcome to the machine\n");
    strncpy(outname, argv[1], sizeof(outname));
    strncpy(sdfhdr, argv[2], sizeof(sdfhdr));
    filelist = argv[3];
    nfiles = atoi(argv[4]);

    FILE *fp = fopen(filelist, "r");
    if (!fp) Error("fopen %s failed\n", filelist);
    char filename[nfiles][256];
    for (int i = 0; i < nfiles; i++) {
	if (!fgets(filename[i], 256, fp)) Error("fgets failed\n");
	/* strip carriage returns */
	char *cr = index(filename[i], '\n');
	if (cr) *cr = '\0';
    }

    singlPrintf("Reading %d files on %d procs\n", nfiles, MPMY_Nproc());

    if (!(sdfp = SDFopen(NULL, sdfhdr))) {
	Error("SDFopen failed: %s", SDFerrstring);
    }
    SDFgetfloatOrDie(sdfp, "particle_mass",  &particle_mass);
    SDFgetdoubleOrDie(sdfp, "L0",  &L0);
    SDFgetdoubleOrDie(sdfp, "a",  &a);
    SDFgetdoubleOrDie(sdfp, "redshift",  &redshift);
    SDFgetdoubleOrDie(sdfp, "tpos",  &tpos);
    SDFgetdoubleOrDie(sdfp, "Omega0_m",  &Omega0_m);
    SDFgetdoubleOrDie(sdfp, "Omega0_lambda",  &Omega0_lambda);
    SDFgetdoubleOrDie(sdfp, "h_100",  &h_100);
    /* Convert to rockstar units */
    particle_mass *= h_100 * 1e10;
    L0 *= 0.001*h_100;

    int light_cone = 0;
    SDFgetint(sdfp, "light_cone", &light_cone);
    int do_periodic = 1;
    SDFgetint(sdfp, "do_periodic", &do_periodic);
    SDFclose(sdfp);

    gnobj = nobj = 0;
    btab = Malloc(sizeof(body)); /* so SDFwrite behaves if nobj is zero */
    for (int i = 0; i < nfiles; i++) {
	if (i % MPMY_Nproc() == MPMY_Procnum()) {
	    printf("%d reading %s\n", MPMY_Procnum(), filename[i]);
	    Fopen(fp, filename[i], "r");
	    if (fstat(fileno(fp), &sb) == -1) Error("stat failed");
	    if (sb.st_size % sizeof(body)) 
		Error("File does not end on record boundary\n");
	    nobj = sb.st_size/sizeof(body);
	    btab = Realloc(btab, (gnobj+nobj) * sizeof(body));
	    Fread(&btab[gnobj], sizeof(body), nobj, fp);
	    gnobj += nobj;
	    Fclose(fp);
	}
    }
    nobj = gnobj;
    MPMY_Combine(&nobj, &gnobj, 1, MPMY_INT64, MPMY_SUM);

    singlPrintf("Sorting %ld particles by xyz\n", gnobj);
    float rmin[NDIM] = {0.0f, 0.0f, 0.0f};
    float rmax[NDIM] = {L0, L0, L0};
    FixRsizeExact(rmin, rmax);

    sortresult_t outputsort;
    pqsortsetup_order(&outputsort, btab, nobj,
		      sizeof(body), 0.1, 1, Realloc_f);
    outputsort.method = 1;
    btab = pqsort(&outputsort,
		  (pq_wgtproto)UnityCost, 
		  (pq_keyproto)GetKeyFast);
    nobj = outputsort.nobj;

    int64_t outnobj = 1;
    for (int64_t i = 1; i < nobj; i++) {
	if ((btab[i-1].pos[0] == btab[i].pos[0]) &&
	    (btab[i-1].pos[1] == btab[i].pos[1]) &&
	    (btab[i-1].pos[2] == btab[i].pos[2])) {
	    SeriousWarning("identical positions %g %g %g %g\n", 
			   btab[0].pos[0], btab[i].pos[1], btab[i].pos[2], btab[i].m200b);
	} else {
	    btab[outnobj++] = btab[i];
	}
    }
    nobj = outnobj;
    btab = Realloc(btab, nobj * sizeof(body));
    MPMY_Combine(&nobj, &gnobj, 1, MPMY_INT64, MPMY_SUM);


    singlPrintf("Writing \"%s\"\n", outname);

    SDFwrite64(outname, gnobj,
	       nobj, btab, sizeof(body), OUTBODYDESC,
	       "nhalo", SDF_INT64, gnobj,
	       "particle_mass", SDF_FLOAT, particle_mass,
	       "part_mass", SDF_FLOAT, particle_mass,
	       "BOX_SIZE", SDF_DOUBLE, L0,
	       "L0", SDF_DOUBLE, L0,
	       "R0", SDF_DOUBLE, 0.5 * L0,
	       "offset_center", SDF_INT, 1,
	       "redshift", SDF_DOUBLE, redshift,
	       "tpos", SDF_DOUBLE, tpos,
	       "overdensity", SDF_DOUBLE, 200.0,
	       "SCALE_NOW", SDF_DOUBLE, a,
	       "a", SDF_DOUBLE, a,
	       "do_periodic", SDF_INT, do_periodic,
	       "light_cone", SDF_INT, light_cone,
	       "x_min", SDF_FLOAT, rmin[0],
	       "y_min", SDF_FLOAT, rmin[1],
	       "z_min", SDF_FLOAT, rmin[2],
	       "x_max", SDF_FLOAT, rmax[0],
	       "y_max", SDF_FLOAT, rmax[1],
	       "z_max", SDF_FLOAT, rmax[2],
	       "Omega0_m", SDF_DOUBLE, Omega0_m,
	       "Omega0_lambda", SDF_DOUBLE, Omega0_lambda,
	       "h_100", SDF_DOUBLE, h_100,
	       "so200b", SDF_INT, 1,
	       "so500c", SDF_INT, 1,
	       "sovir", SDF_INT, 1,
	       "rockstar_units", SDF_INT, 1,
	       "morton_xyz", SDF_INT, 1,
	       "length_unit", SDF_STRING, "Mpccm/h", 
	       "mass_unit", SDF_STRING, "Msun/h", 
	       "time_unit", SDF_STRING, "Gyr", 
	       "velocity_unit", SDF_STRING, "km/s",
	       "compiled_version_2HOT", SDF_STRING, version_2HOT,
	       "compiled_date_2HOT", SDF_STRING, compiled_date_2HOT,
	       "compiled_time_2HOT", SDF_STRING, compiled_time_2HOT,
	       NULL);

    singlPrintf("Done.\n", gnobj);
    MPMY_Finalize();
    exit(0);
}
