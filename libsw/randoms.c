#include "error.h"
#include "randoms.h"

/* ran2 from Numerical Recipes 2nd ed. modified to conform to our interface */
/* You win $1000 from Press et al. if it fails non-trivially */

#define IM1 2147483563
#define IM2 2147483399
#define AM (1.0/IM1)
#define IMM1 (IM1-1)
#define IA1 40014
#define IA2 40692
#define IQ1 53668
#define IQ2 52774
#define IR1 12211
#define IR2 3791
#define NDIV (1+IMM1/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

void
ran_init(int seed, ran_state *s)
{
    int j;
    long k;

    if (seed < 1) 
      Error("Bad seed in ran_init2 (%d)\n", seed);
    s->idum = s->idum2 = seed;
    s->next_norml_ok = 0;
    for (j=NTAB+7;j>=0;j--) {
	k=(s->idum)/IQ1;
	s->idum=IA1*(s->idum-k*IQ1)-k*IR1;
	if (s->idum < 0) s->idum += IM1;
	if (j < NTAB) s->iv[j] = s->idum;
    }
    s->iy=s->iv[0];
    s->did_init = IQ1;
}

float
uniform_rand(ran_state *s)
{
    int j;
    long k;
    float temp;
    
    if (s->did_init != IQ1)
      Error("You forgot to call ran_init2\n");
    k=(s->idum)/IQ1;
    s->idum=IA1*(s->idum-k*IQ1)-k*IR1;
    if (s->idum < 0) s->idum += IM1;
    k=s->idum2/IQ2;
    s->idum2=IA2*(s->idum2-k*IQ2)-k*IR2;
    if (s->idum2 < 0) s->idum2 += IM2;
    j=s->iy/NDIV;
    s->iy=s->iv[j]-s->idum2;
    s->iv[j] = s->idum;
    if (s->iy < 1) s->iy += IMM1;
    if ((temp=AM*s->iy) > RNMX) return RNMX;
    else return temp;
}

float normal_rand(ran_state *st)
/*
This is the Polar method for normal distributions, as described on or near
page 104 of Knuth, Semi-numerical Algorithms.  To quote Knuth, "The polar
method is quite slow, but it has essentially perfect accuracy, and it is very
easy to write a program for the polar method..."  'nuf said.  Algorithm due
to Box, Muller and Marsaglia.
*/
{
    float	v1, v2;	/* uniformly distributed on [-1, 1) */
    float s;	/* radius of a point pulled from a uniform circle */
    float	foo;	/* A useful intermediate value. */
    double log(double), sqrt(double);
    
    if(st->next_norml_ok){
	st->next_norml_ok = 0;
	return st->next_norml;
    }
    
    do{
	v1 = 2.0F * uniform_rand(st) - 1.0F;
	v2 = 2.0F * uniform_rand(st) - 1.0F;
	s = v1*v1 + v2*v2;
    } while(s >= 1.0F);
    foo = sqrt( -2.0F * log(s)/s);
    st->next_norml_ok = 1;
    st->next_norml = v1*foo;
    return v2*foo;
}

/* Return a uniform point in a ndim-sphere by rejection. */
/* If ndim is large (bigger than 4 or so), this becomes very inefficient */
/* Return the radius-squard of the result. */
float sphere_rand(ran_state *st, int ndim, float *x)
{
    int k;
    float rsqx;
 
    do {
	rsqx = (float)0.0;
	for (k = 0; k < ndim; k++) {
            x[k] = uniform_rand(st)*2.0F - 1.0F; /* a pt in (-1,1) */
            rsqx += x[k] * x[k];
	}
    } while (rsqx > (float)1.0);
    return rsqx;
}

/* Return a uniform point in a ndim-cube */
/* Return the radius-squard of the result. */
float cube_rand(ran_state *st, int ndim, float *x)
{
    int k;
    float rsqx;
 
    rsqx = (float)0.0;
    for (k = 0; k < ndim; k++) {
	x[k] = uniform_rand(st)*2.0F - 1.0F; /* a pt in (-1,1) */
	rsqx += x[k] * x[k];
    }
    return rsqx;
}

