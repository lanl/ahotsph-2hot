#include <math.h>
#include "Malloc.h"
#include "error.h"
#include "qromo.h"

#define NR_END 1

static double *vector(long nl, long nh)
/* allocate a double vector with subscript range v[nl..nh] */
{
    double *v;

    v = Malloc((nh-nl+1+NR_END)*sizeof(double));
    return v-nl+NR_END;
}

static void free_vector(v,nl,nh)
double *v;
long nh,nl;
/* free a double vector allocated with vector() */
{
    Free(v+nl-NR_END);
}

void polintd(double xa[], double ya[], int n, double x, double *y, double *dy)
{
    int i,m,ns=1;
    double den,dif,dift,ho,hp,w;
    double *c,*d;

    dif=fabs(x-xa[1]);
    c=vector(1,n);
    d=vector(1,n);
    for (i=1;i<=n;i++) {
	if ( (dift=fabs(x-xa[i])) < dif) {
	    ns=i;
	    dif=dift;
	}
	c[i]=ya[i];
	d[i]=ya[i];
    }
    *y=ya[ns--];
    for (m=1;m<n;m++) {
	for (i=1;i<=n-m;i++) {
	    ho=xa[i]-x;
	    hp=xa[i+m]-x;
	    w=c[i+1]-d[i];
	    if ( (den=ho-hp) == 0.0) Error("Error in routine polint");
	    den=w/den;
	    d[i]=hp*den;
	    c[i]=ho*den;
	}
	*y += (*dy=(2*ns < (n-m) ? c[ns+1] : d[ns--]));
    }
    free_vector(d,1,n);
    free_vector(c,1,n);
}

#define FUNC(x) ((*func)(x))

double midpntd(double (*func)(double), double a, double b, int n)
{
    double x,tnm,sum,del,ddel;
    static double s;
    int it,j;

    if (n == 1) {
	return (s=(b-a)*FUNC(0.5*(a+b)));
    } else {
	for(it=1,j=1;j<n-1;j++) it *= 3;
	tnm=it;
	del=(b-a)/(3.0*tnm);
	ddel=del+del;
	x=a+0.5*del;
	sum=0.0;
	for (j=1;j<=it;j++) {
	    sum += FUNC(x);
	    x += ddel;
	    sum += FUNC(x);
	    x += del;
	}
	s=(s+(b-a)*sum/tnm)/3.0;
	return s;
    }
}

#define EPS 1.0e-12
#define JMAX 20
#define JMAXP (JMAX+1)
#define K 5

double qromod(double (*func)(double), double a, double b,
	double (*choose)(double(*)(double), double, double, int))
{

    int j;
    double ss,dss,h[JMAXP+1],s[JMAXP+1];

    h[1]=1.0;
    for (j=1;j<=JMAX;j++) {
	s[j]=(*choose)(func,a,b,j);
	if (j >= K) {
	    polintd(&h[j-K],&s[j-K],K,0.0,&ss,&dss);
	    if (fabs(dss) < EPS*fabs(ss)) return ss;
	}
	s[j+1]=s[j];
	h[j+1]=h[j]/9.0;
    }
    Error("Too many steps in routine qromod\n");
    return 0.0;
}
