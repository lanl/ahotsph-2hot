#include <stdio.h>
#include <math.h>
#include "cosmo.h"
#include "Malloc.h"
#include "error.h"
#include "macr.h"

#define _SUCCESS_ 0
#define _FAILURE_ 1
#define _ERRORMSGSIZE_ 256
typedef char ErrorMsg[_ERRORMSGSIZE_];
#define _SPLINE_NATURAL_ 0
#define _SPLINE_EST_DERIV_ 1

#define _Gyr_over_Mpc_ 3.06601394e2

struct tbl_s {
    int bt_size, bg_size;
    double *tau_tbl, *z_tbl, *t_tbl, *tbl;
    double *d2tau_dz2_tbl, *d2tau_dt2_tbl, *d2b_dtau2_tbl;
    ErrorMsg error_message;
};

static int
array_spline_table_lines(
			 double * x, /* vector of size x_size */
			 int x_size,
			 double * y_array, /* array of size x_size*y_size with elements 
					      y_array[index_x*y_size+index_y] */
			 int y_size,   
			 double * ddy_array, /* array of size x_size*y_size */
			 short spline_mode,
			 ErrorMsg errmsg
			 ) {
  double * p;
  double * qn;
  double * un; 
  double * u;
  double sig;
  int index_x;
  int index_y;
  double dy_first;
  double dy_last;

  u = Malloc((x_size-1) * y_size * sizeof(double));
  p = Malloc(y_size * sizeof(double));
  qn = Malloc(y_size * sizeof(double));
  un = Malloc(y_size * sizeof(double));

  index_x=0;

  if (spline_mode == _SPLINE_NATURAL_) {
    for (index_y=0; index_y < y_size; index_y++) {
      ddy_array[index_x*y_size+index_y] = u[index_x*y_size+index_y] = 0.0;
    }
  }
  else {
    if (spline_mode == _SPLINE_EST_DERIV_) {

      for (index_y=0; index_y < y_size; index_y++) {

	dy_first = 
	  ((x[2]-x[0])*(x[2]-x[0])*
	   (y_array[1*y_size+index_y]-y_array[0*y_size+index_y])-
	   (x[1]-x[0])*(x[1]-x[0])*
	   (y_array[2*y_size+index_y]-y_array[0*y_size+index_y]))/
	  ((x[2]-x[0])*(x[1]-x[0])*(x[2]-x[1]));
	
	ddy_array[index_x*y_size+index_y] = -0.5;
	
	u[index_x*y_size+index_y] =
	  (3./(x[1] -  x[0]))*
	  ((y_array[1*y_size+index_y]-y_array[0*y_size+index_y])/
	   (x[1] - x[0])-dy_first);

      }
    }
    else {
      sprintf(errmsg,"%s(L:%d) Spline mode not identified: %d",__func__,__LINE__,spline_mode);
      return _FAILURE_;
    }
  }
    

  for (index_x=1; index_x < x_size-1; index_x++) {

    sig = (x[index_x] - x[index_x-1])/(x[index_x+1] - x[index_x-1]);

    for (index_y=0; index_y < y_size; index_y++) {

      p[index_y] = sig * ddy_array[(index_x-1)*y_size+index_y] + 2.0;

      ddy_array[index_x*y_size+index_y] = (sig-1.0)/p[index_y];

      u[index_x*y_size+index_y] = 
	(y_array[(index_x+1)*y_size+index_y] - y_array[index_x*y_size+index_y])
	/ (x[index_x+1] - x[index_x])
	- (y_array[index_x*y_size+index_y] - y_array[(index_x-1)*y_size+index_y])
	/ (x[index_x] - x[index_x-1]);

      u[index_x*y_size+index_y] = (6.0 * u[index_x*y_size+index_y] /
				   (x[index_x+1] - x[index_x-1]) 
				   - sig * u[(index_x-1)*y_size+index_y]) / p[index_y];
    }

  }

  if (spline_mode == _SPLINE_NATURAL_) {

    for (index_y=0; index_y < y_size; index_y++) {
      qn[index_y]=un[index_y]=0.0;
    }

  }
  else {
    if (spline_mode == _SPLINE_EST_DERIV_) {

      for (index_y=0; index_y < y_size; index_y++) {

	dy_last = 
	  ((x[x_size-3]-x[x_size-1])*(x[x_size-3]-x[x_size-1])*
	   (y_array[(x_size-2)*y_size+index_y]-y_array[(x_size-1)*y_size+index_y])-
	   (x[x_size-2]-x[x_size-1])*(x[x_size-2]-x[x_size-1])*
	   (y_array[(x_size-3)*y_size+index_y]-y_array[(x_size-1)*y_size+index_y]))/
	  ((x[x_size-3]-x[x_size-1])*(x[x_size-2]-x[x_size-1])*(x[x_size-3]-x[x_size-2]));

	qn[index_y]=0.5;

	un[index_y]=
	  (3./(x[x_size-1] - x[x_size-2]))*
	  (dy_last-(y_array[(x_size-1)*y_size+index_y] - y_array[(x_size-2)*y_size+index_y])/
	   (x[x_size-1] - x[x_size-2]));	

      }
    }
    else {
      sprintf(errmsg,"%s(L:%d) Spline mode not identified: %d",__func__,__LINE__,spline_mode);
      return _FAILURE_;
    }
  }
    
  index_x=x_size-1;

  for (index_y=0; index_y < y_size; index_y++) {
    ddy_array[index_x*y_size+index_y] = 
      (un[index_y] - qn[index_y] * u[(index_x-1)*y_size+index_y]) /
      (qn[index_y] * ddy_array[(index_x-1)*y_size+index_y] + 1.0);
  }

  for (index_x=x_size-2; index_x >= 0; index_x--) {
    for (index_y=0; index_y < y_size; index_y++) {

      ddy_array[index_x*y_size+index_y] = ddy_array[index_x*y_size+index_y] *
	ddy_array[(index_x+1)*y_size+index_y] + u[index_x*y_size+index_y];

    }
  }

  Free(qn);
  Free(un);
  Free(p);
  Free(u);

  return _SUCCESS_;
 }

 /**
  * interpolate to get y_i(x), when x and y_i are in different arrays
  *
  */
static int
array_interpolate_spline(
			 double * x_array,
			 int n_lines,
			 double * array,
			 double * array_splined,
			 int n_columns,
			 double x,
			 int * last_index,
			 double * result,
			 int result_size, /** from 1 to n_columns */
			 ErrorMsg errmsg) {

  int inf,sup,mid,i;
  double h,a,b;
  
  inf=0;
  sup=n_lines-1;
  
  if (x_array[inf] < x_array[sup]){

    if (x < x_array[inf]) {
      sprintf(errmsg,"%s(L:%d) : x=%e < x_min=%e",__func__,__LINE__,x,x_array[inf]);
      return _FAILURE_;
    }

    if (x > x_array[sup]) {
      sprintf(errmsg,"%s(L:%d) : x=%e > x_max=%e",__func__,__LINE__,x,x_array[sup]);
      return _FAILURE_;
    }

    while (sup-inf > 1) {

      mid=(int)(0.5*(inf+sup));
      if (x < x_array[mid]) {sup=mid;}
      else {inf=mid;}

    }

  }

  else {

    if (x < x_array[sup]) {
      sprintf(errmsg,"%s(L:%d) : x=%e < x_min=%e",__func__,__LINE__,x,x_array[sup]);
      return _FAILURE_;
    }

    if (x > x_array[inf]) {
      sprintf(errmsg,"%s(L:%d) : x=%e > x_max=%e",__func__,__LINE__,x,x_array[inf]);
      return _FAILURE_;
    }

    while (sup-inf > 1) {

      mid=(int)(0.5*(inf+sup));
      if (x > x_array[mid]) {sup=mid;}
      else {inf=mid;}

    }

  }

  *last_index = inf;

  h = x_array[sup] - x_array[inf];
  b = (x-x_array[inf])/h;
  a = 1-b;

  for (i=0; i<result_size; i++)
    *(result+i) = 
      a * *(array+inf*n_columns+i) +
      b * *(array+sup*n_columns+i) +
      ((a*a*a-a)* *(array_splined+inf*n_columns+i) + 
       (b*b*b-b)* *(array_splined+sup*n_columns+i))*h*h/6.;

  return _SUCCESS_;
}



static void
tbl_background_at_tau(cosmology *c, double tau)
{
    struct tbl_s *p = c->private;
    int last_index;
    double *pvec = Malloc(p->bg_size*sizeof(double));

    array_interpolate_spline(p->tau_tbl,
			     p->bt_size,
			     p->tbl,
			     p->d2b_dtau2_tbl,
			     p->bg_size,
			     tau,
			     &last_index,
			     pvec,
			     p->bg_size,
			     p->error_message);
    c->z = pvec[0];
    c->t = pvec[1]/_Gyr_over_Mpc_;
    c->tau = tau;
    c->a = 1.0/(1.0+c->z);
    c->H = pvec[2]*_Gyr_over_Mpc_;
    c->conf_distance = pvec[3]*1000.0; /* kpc */
    c->kick = tau/_Gyr_over_Mpc_;;
    c->drift = pvec[4]/_Gyr_over_Mpc_;
    c->growthfac = pvec[5];
    /* c->velfac = pow(c->Omega0_m/(c->a*c->a*c->a)*c->H0*c->H0/(c->H*c->H), 0.6); */
    c->velfac = pvec[7];
    c->velfac2 = 2.0*pow(c->Omega0_m/(c->a*c->a*c->a)*c->H0*c->H0/(c->H*c->H), 4./7.);
    Free(pvec);
}


static void
tbl_background_at_z(cosmology *c, double z)
{
    struct tbl_s *p = c->private;
    int last_index;
    double tau;
    
    array_interpolate_spline(p->z_tbl,
			     p->bt_size, 
			     p->tau_tbl,
			     p->d2tau_dz2_tbl,
			     1,
			     z,
			     &last_index,
			     &tau,
			     1,
			     p->error_message);
    tbl_background_at_tau(c, tau);
}

static void
tbl_background_at_t(cosmology *c, double t)
{
    struct tbl_s *p = c->private;
    int last_index;
    double tau;
    
    array_interpolate_spline(p->t_tbl,
			     p->bt_size, 
			     p->tau_tbl,
			     p->d2tau_dt2_tbl,
			     1,
			     t*_Gyr_over_Mpc_,
			     &last_index,
			     &tau,
			     1,
			     p->error_message);
    tbl_background_at_tau(c, tau);
}

static double
tbl_t_at_z(cosmology *c, double z)
{
    tbl_background_at_z(c, z);
    return(c->t);
}

static double
tbl_z_at_t(cosmology *c, double t)
{
    tbl_background_at_t(c, t);
    return(c->z);
}

static double
tbl_a_at_t(cosmology *c, double t)
{
    tbl_background_at_t(c, t);
    return(c->a);
}

static double
tbl_t_at_a(cosmology *c, double a)
{
    tbl_background_at_z(c, 1.0/a-1.0);
    return(c->t);
}

static double
tbl_H_at_z(cosmology *c, double z)
{
    tbl_background_at_z(c, z);
    return(c->H);
}

static double
tbl_H_at_t(cosmology *c, double t)
{
    tbl_background_at_t(c, t);
    return(c->H);
}

static double
tbl_conformal_distance_at_z(cosmology *c, double z)
{
    tbl_background_at_z(c, z);
    return(c->conf_distance);
}

static double
tbl_conformal_distance_at_t(cosmology *c, double t)
{
    tbl_background_at_t(c, t);
    return(c->conf_distance);
}

static double
tbl_angular_diameter_distance_at_z(cosmology *c, double z)
{
    tbl_background_at_z(c, z);
    return(c->conf_distance/(1.0+z));
}

static double
tbl_angular_diameter_distance_at_t(cosmology *c, double t)
{
    tbl_background_at_t(c, t);
    return(c->conf_distance/(1.0+c->z));
}

static double
tbl_luminosity_distance_at_z(cosmology *c, double z)
{
    tbl_background_at_z(c, z);
    return(c->conf_distance*(1.0+z));
}

static double
tbl_luminosity_distance_at_t(cosmology *c, double t)
{
    tbl_background_at_t(c, t);
    return(c->conf_distance*(1.0+c->z));
}

static double
tbl_growthfac_at_z(cosmology *c, double z)
{
    tbl_background_at_z(c, z);
    return(c->growthfac);
}

static double
tbl_growthfac_at_t(cosmology *c, double t)
{
    tbl_background_at_t(c, t);
    return(c->growthfac);
}

static double
tbl_velfac_at_z(cosmology *c, double z)
{
    tbl_background_at_z(c, z);
    return(c->velfac);
}

static double
tbl_velfac_at_t(cosmology *c, double t)
{
    tbl_background_at_t(c, t);
    return(c->velfac);
}

static double
tbl_kick_t0_t1(cosmology *c, double t0, double t1)
{
    double k0, k1;
    tbl_background_at_t(c, t0);
    k0 = c->kick;
    tbl_background_at_t(c, t1);
    k1 = c->kick;
    return(k1-k0);
}

static double
tbl_drift_t0_t1(cosmology *c, double t0, double t1)
{
    double d0, d1;
    tbl_background_at_t(c, t0);
    d0 = c->drift;
    tbl_background_at_t(c, t1);
    d1 = c->drift;
    return(d1-d0);
}

static void
tbl_free(cosmology *c)
{
    struct tbl_s *p = c->private;

    Free(p->d2tau_dz2_tbl);
    Free(p->d2tau_dt2_tbl);
    Free(p->d2b_dtau2_tbl );
    Free(p->tbl);
    Free(p->t_tbl);
    Free(p->z_tbl);
    Free(p->tau_tbl);
    Free(p);
    c->private = NULL;
}

void
tbl_init(cosmology *c, char *tbl)
{
    char line[1024];
    FILE *fp;
    int i;
    double tau, z, t, H, R, drift, growth, growth_cdm, velfac;
    struct tbl_s *p = Malloc(sizeof(struct tbl_s));
    p->bt_size = 8192;
    p->bg_size = 8;

    p->tau_tbl = Malloc(p->bt_size*sizeof(double));
    p->z_tbl = Malloc(p->bt_size*sizeof(double));
    p->t_tbl = Malloc(p->bt_size*sizeof(double));
    p->tbl = Malloc(p->bg_size*p->bt_size*sizeof(double));
    Fopen(fp, tbl, "r");
    i = 0;
    while (fgets(line, sizeof(line), fp)) {
	if (line[0] == '#') continue;
	if (sscanf(line, "%lg %lg %lg %lg %lg %lg %lg %lg %lg\n", 
		   &tau, &z, &t, &H, &R, &drift, &growth, &growth_cdm, &velfac) != 9)
	    Error("Did not parse %s", line);
	p->tau_tbl[i] = tau;
	p->z_tbl[i] = z;
	p->t_tbl[i] = t;
	p->tbl[i*p->bg_size+0] = z;
	p->tbl[i*p->bg_size+1] = t;
	p->tbl[i*p->bg_size+2] = H;
	p->tbl[i*p->bg_size+3] = R;
	p->tbl[i*p->bg_size+4] = drift;
	p->tbl[i*p->bg_size+5] = growth;
	p->tbl[i*p->bg_size+6] = growth_cdm;
	p->tbl[i*p->bg_size+7] = velfac;
	i++;
	if (i >= p->bt_size) {
	    p->bt_size *= 2;
	    p->tau_tbl = Realloc(p->tau_tbl, p->bt_size*sizeof(double));
	    p->z_tbl = Realloc(p->z_tbl, p->bt_size*sizeof(double));
	    p->t_tbl = Realloc(p->t_tbl, p->bt_size*sizeof(double));
	    p->tbl = Realloc(p->tbl, p->bg_size*p->bt_size*sizeof(double));
	}
    }
    Fclose(fp);
    p->bt_size = i;
    p->tau_tbl = Realloc(p->tau_tbl, p->bt_size*sizeof(double));
    p->z_tbl = Realloc(p->z_tbl, p->bt_size*sizeof(double));
    p->t_tbl = Realloc(p->t_tbl, p->bt_size*sizeof(double));
    p->tbl = Realloc(p->tbl, p->bg_size*p->bt_size*sizeof(double));

    p->d2tau_dz2_tbl = Malloc(p->bt_size*sizeof(double));
    p->d2tau_dt2_tbl = Malloc(p->bt_size*sizeof(double));
    p->d2b_dtau2_tbl = Malloc(p->bg_size*p->bt_size*sizeof(double));

    array_spline_table_lines(p->z_tbl,
			     p->bt_size,
			     p->tau_tbl,
			     1,
			     p->d2tau_dz2_tbl,
			     _SPLINE_EST_DERIV_,
			     p->error_message);
    array_spline_table_lines(p->t_tbl,
			     p->bt_size,
			     p->tau_tbl,
			     1,
			     p->d2tau_dt2_tbl,
			     _SPLINE_EST_DERIV_,
			     p->error_message);
    array_spline_table_lines(p->tau_tbl,
			     p->bt_size,
			     p->tbl,
			     p->bg_size,
			     p->d2b_dtau2_tbl,
			     _SPLINE_EST_DERIV_,
			     p->error_message);

    c->private = p;

    /* Function pointers */
    c->background_at_z = tbl_background_at_z;
    c->background_at_t = tbl_background_at_t;
    c->background_at_tau = tbl_background_at_tau;
    c->t_at_z = tbl_t_at_z;
    c->z_at_t = tbl_z_at_t;
    c->a_at_t = tbl_a_at_t;
    c->t_at_a = tbl_t_at_a;
    c->H_at_z = tbl_H_at_z;
    c->H_at_t = tbl_H_at_t;
    c->conformal_distance_at_z = tbl_conformal_distance_at_z;
    c->conformal_distance_at_t = tbl_conformal_distance_at_t;
    c->angular_diameter_distance_at_z = tbl_angular_diameter_distance_at_z;
    c->angular_diameter_distance_at_t = tbl_angular_diameter_distance_at_t;
    c->luminosity_distance_at_z = tbl_luminosity_distance_at_z;
    c->luminosity_distance_at_t = tbl_luminosity_distance_at_t;
    c->growthfac_at_z = tbl_growthfac_at_z;
    c->growthfac_at_t = tbl_growthfac_at_t;
    c->velfac_at_z = tbl_velfac_at_z;
    c->velfac_at_t = tbl_velfac_at_t;
    c->kick_t0_t1 = tbl_kick_t0_t1;
    c->drift_t0_t1 = tbl_drift_t0_t1;
    c->free = tbl_free;
}
