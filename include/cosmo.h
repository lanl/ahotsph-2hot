/* Hide the cosmological parameters in here.
   Keep them self-consistent... */

struct cosmo_s {
    double t;
    double a;
    double H0;
    double Omega0;
    double Omega_m;
    double Omega_r;
    double Omega_de;
    double w0;
    double wa;
    double Lambda;
    double Gnewt;
    double Zel_f;/* the 'f' factor for linearly growing modes */
};

/* new struct for CLASS interface */
/* some names have changed definition (Omega_m/Omega_r not constants now) */
typedef struct cosmology {
    double z;
    double t;
    double tau;
    double a;
    double H;
    double Omega_m;
    double Omega_r;
    double conf_distance;
    double kick;		/* union with tau? */
    double drift;
    double growthfac;
    double velfac;
    double velfac2;
    /* constants */
    double h_100;
    double H0;
    double Omega0;		/* m+r+lambda+fld */
    double Omega0_m;		/* cdm+b+ur+some ncdm */
    double Omega0_r;		/* g+ur+rest of ncdm */
    double Omega0_lambda;
    double Omega0_cdm;
    double Omega0_ncdm_tot;
    double Omega0_b;
    double Omega0_g;
    double Omega0_ur;
    double Omega0_fld;
    double w0_fld;
    double wa_fld;
    double Gnewt;
    double age;
    void *private;
    void (*background_at_z)(struct cosmology *c, double z);
    void (*background_at_t)(struct cosmology *c, double t);
    void (*background_at_tau)(struct cosmology *c, double tau);
    double (*t_at_z)(struct cosmology *c, double z);
    double (*z_at_t)(struct cosmology *c, double t);
    double (*a_at_t)(struct cosmology *c, double t);
    double (*t_at_a)(struct cosmology *c, double a);
    double (*H_at_z)(struct cosmology *c, double z);
    double (*H_at_t)(struct cosmology *c, double t);
    double (*conformal_distance_at_z)(struct cosmology *c, double z);
    double (*conformal_distance_at_t)(struct cosmology *c, double t);
    double (*angular_diameter_distance_at_z)(struct cosmology *c, double z);
    double (*angular_diameter_distance_at_t)(struct cosmology *c, double t);
    double (*luminosity_distance_at_z)(struct cosmology *c, double z);
    double (*luminosity_distance_at_t)(struct cosmology *c, double t);
    double (*growthfac_at_z)(struct cosmology *c, double z);
    double (*growthfac_at_t)(struct cosmology *c, double t);
    double (*velfac_at_z)(struct cosmology *c, double z);
    double (*velfac_at_t)(struct cosmology *c, double t);
    double (*kick_t0_t1)(struct cosmology *c, double t0, double t1);
    double (*drift_t0_t1)(struct cosmology *c, double t0, double t1);
    void (*free)(struct cosmology *c);
} cosmology;

void class_init(cosmology *c, char *class_ini, char *class_pre, double zmax);
void class_params(cosmology *c, char *class_ini);
void tbl_init(cosmology *c, char *tbl);

double Anow(struct cosmo_s *c, double time);
double Znow(struct cosmo_s *c, double time);
double Hnow(struct cosmo_s *c, double time);
double growthfac_from_Z(struct cosmo_s *c, double z);
double velfac_from_Z(struct cosmo_s *c, double z);
double velfac_approx_from_Z(struct cosmo_s *c, double z);
double t_from_Z(struct cosmo_s *c, double z);
double comoving_distance_from_Z(struct cosmo_s *c, double z);
double dp_from_Z(struct cosmo_s *c, double z);
double hubble_from_Z(struct cosmo_s *c, double z);
double kick_delta(struct cosmo_s *c, double t0, double t1);
double drift_delta(struct cosmo_s *c, double t0, double t1);
void CosmoPush(struct cosmo_s *c, double time);

#define one_kpc 3.08567802e16 /* km */
#define one_Gyr 3.1558149984e16 /* sec */
#define cm_kpc  3.08567802e21
#define sec_Gyr 3.1558149984e16
#define g_Msol  1.98892e33
#define g_Msol10 1.98892e43	/* 10^10 Msol */
/* http://physics.nist.gov/cgi-bin/cuu/Value?bg|search_for=G */
/* #define G_cgs (6.67384e-8) cm3 g-1 s-2 */ 
#define G_cgs (6.67259e-8)
#define speed_of_light (299792.458) /* km/sec */
/* Gaussian Gravitational Constant */
#define k_cgs 0.01720209895
#define GM_cgs 1.32712442099e26
/* GM_cgs*sec_Gyr*sec_Gyr/cm_kpc*cm_kpc*cm_kpc */
#define GNEWT 44986.564




