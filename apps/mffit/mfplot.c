/* Make histograms from bgc2.sdf (so200b) or .halos (FOF) file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <values.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_histogram.h>
#include "SDF.h"
#include "Malloc.h"
#include "macr.h"
#include "version.h"

/* These can fail for int arguments */
#define pow2(x) ({ __typeof__ (x) _x = (x); _x*_x; })
#define pow3(x) ({ __typeof__ (x) _x = (x); _x*_x*_x; })

#define one_kpc 3.08567802e16 /* km */
#define one_Gyr 3.1558149984e16 /* sec */
#define SQRT2_PI 0.7978845608028654

typedef struct pdf_params_s {
    double A, a, b, c, d;
    double rho0;
    gsl_interp_accel *acc, *dacc;
    gsl_spline *spline, *dspline;
    double (*pdf)(double m, void *params);
    char name[64];
} pdf_params_s;


double
pdf_powlaw(double m, void *params)
{
    pdf_params_s *p = (pdf_params_s *)params;
    double A = p->A;
    double a = p->a;

    return A * pow(m, -a);
}


double
pdf_lanl(double m, void *params)
{
    pdf_params_s *p = (pdf_params_s *)params;
    double A = p->A;
    double a = p->a;
    double b = p->b;
    double c = p->c;
    double d = p->d;

    double logm = log(m);
    double sigma = gsl_spline_eval(p->spline, logm, p->acc);
    double dsdm = gsl_spline_eval(p->dspline, logm, p->dacc);
    /* extra factor of m in dsdm should be taken out along with 1/m in rhomm */
    m = pow(m, d);
    double rhomm = A*(p->rho0/m)/m;
    double f = rhomm * (pow(sigma, -a) + b) * exp(-c / (sigma * sigma)) * dsdm;
    if (f < 0.0 || !isfinite(f)) Error("Bad f %g\n", f);
    else if (f == 0.0) return DBL_MIN;
    else return f;
}

double
pdf_tinker(double m, void *params)
{
    pdf_params_s *p = (pdf_params_s *)params;
    double A = p->A;
    double a = p->a;
    double b = p->b;
    double c = p->c;
    double d = p->d;

    double logm = log(m);
    double sigma = gsl_spline_eval(p->spline, logm, p->acc);
    double dsdm = gsl_spline_eval(p->dspline, logm, p->dacc);
    m = pow(m, d);
    double rhomm = A*(p->rho0/m)/m;
    double f = rhomm * (pow(sigma/b, -a) + 1.0) * exp(-c / (sigma * sigma)) * dsdm;
    if (f < -1e-12 || !isfinite(f)) Error("Bad f %g\n", f);
    else if (f <= 0.0) return DBL_MIN;
    else return f;
}

double
pdf_bhat(double m, void *params)
{
    pdf_params_s *pp = (pdf_params_s *)params;
    double A = pp->A;
    double a = pp->a;
    double p = pp->b;
    double q0 = pp->c;
    double delta_c = 1.68647;

    double logm = log(m);
    double sigma = gsl_spline_eval(pp->spline, logm, pp->acc);
    double dsdm = gsl_spline_eval(pp->dspline, logm, pp->dacc);
    double rhomm = A * SQRT2_PI * (pp->rho0/m) / m;
    double f = rhomm * (1.0 + pow(sigma * sigma / (a * delta_c * delta_c), p))
	* pow(delta_c * sqrt(a) / sigma, q0)
	* exp(-a * delta_c * delta_c /(2.0 * sigma * sigma)) * dsdm;
    if (f < 0.0 || !isfinite(f)) Error("Bad f %g\n", f);
    else if (f == 0.0) return DBL_MIN;
    else return f;
}


#define MTAB_INCREMENT 1000000

void
init_spline(char *fname, gsl_interp_accel **acc, gsl_spline **spline, gsl_interp_accel **dacc, gsl_spline **dspline)
{
    int msn = 0;
    int msn_alloc = MTAB_INCREMENT;
    double *mm = Malloc(msn_alloc * sizeof(double));
    double *ss = Malloc(msn_alloc * sizeof(double));
    double *ds = Malloc(msn_alloc * sizeof(double));
    char buffer[1024];
    FILE *input;
    Fopen(input, fname, "r");
    while (fgets(buffer, 1024, input)) {
	if (buffer[0] == '#') continue;
	/* # r*h(Mpc) mass*h sigma_tophat sigma_gaussian ln(1/sigma) f_PS f_ST f_Jenkins f_LANL f_Bhat f_Reed f_Tinker ds_dm */
	int ret = sscanf(buffer, "%*f %lf %lf %*f %*f %*f %*f %*f %*f %*f %*f %*f %lf\n", mm+msn, ss+msn, ds+msn);
	if (ret != 3) {
	    fprintf(stderr, "Did not parse line %d, %s\n", msn, buffer);
	    exit(1);
	}
	msn++;
	if (!(msn % MTAB_INCREMENT)) {
	    msn_alloc += MTAB_INCREMENT;
	    mm = Realloc(mm, msn_alloc * sizeof(double));
	    ss = Realloc(ss, msn_alloc * sizeof(double));
	    ds = Realloc(ds, msn_alloc * sizeof(double));
	}
    }
    Fclose(input);
    mm = Realloc(mm, msn * sizeof(double));
    ss = Realloc(ss, msn * sizeof(double));
    ds = Realloc(ds, msn * sizeof(double));

    for (int i = 0; i < msn; i++) {
	mm[i] = log(1e10*mm[i]);
    }

    *acc = gsl_interp_accel_alloc();
    *spline = gsl_spline_alloc(gsl_interp_cspline, msn);
    gsl_spline_init(*spline, mm, ss, msn);
    *dacc = gsl_interp_accel_alloc();
    *dspline = gsl_spline_alloc(gsl_interp_cspline, msn);
    gsl_spline_init(*dspline, mm, ds, msn);

    Free(mm);
    Free(ss);
    Free(ds);
}

gsl_histogram *
histogramf(float *mtab, int64_t n, double pmass, int bins_per_decade, double err_sigma)
{
    int ilogm_start = 8;
    int ilogm_end = 17;
    int nbins = (ilogm_end-ilogm_start)*bins_per_decade;
    gsl_histogram *h = gsl_histogram_alloc(nbins);
    double range[nbins+1];

    for (int i = 0; i < nbins+1; i++) {
	double x;
	int bin = i+ilogm_start*bins_per_decade;
	/* place bins between particle masses */
	x = floor(pow(10.0, bin/(double)bins_per_decade-0.5/bins_per_decade)/pmass);
	range[i] = pow(10.0, (log10(x*pmass)+log10((x+1.0)*pmass))/2.0);
    }

    gsl_histogram_set_ranges(h, range, nbins+1);

    if (err_sigma == 0.0) {
	for (int64_t i = 0; i < n; i++) {
	    gsl_histogram_increment(h, mtab[i]);
	}
    } else {
	double sum = 0.0;
	double binwidth = err_sigma / 10.0;
	for (int64_t i = 0; i < n; i++) {
	    for (int j = 0; j < 80; j++) {
		double x = (j - 39.5) * binwidth;
		double y = gsl_ran_gaussian_pdf(x, err_sigma) * binwidth;
		gsl_histogram_accumulate(h, mtab[i] * (1.0 + x), y);
		if (i == 0) sum += y;
	    }
	    if (i == 0 && fabs(sum-1.0) > 0.001) {
		Error("err_sigma normalization error is too large (%g)\n", sum-1.0);
	    }
	}
    }
    return h;
}

void
write_header2(FILE *output, int64_t N, pdf_params_s *p, char *sigma_fname)
{
    fprintf(output, "# Version %s\n", mffit_version);
    /* fprintf(output, "# libSDF Version %s\n", libSDF_version); */
    fprintf(output, "# Using %s pdf\n", p->name);
    fprintf(output, "# sigma = %s\n", sigma_fname);
    fprintf(output, "# A = %.5f a = %.5f b = %.5f c = %.5f d = %.5f\n", p->A, p->a, p->b, p->c, p->d);
}

typedef struct options_s {
    double L0;
    double Omega0_m;
    double h_100;
    double redshift;
    double rho_crit;
    double part_mass;
    double err_sigma;
    double vol_frac;
    double x_min, y_min, z_min;
    double x_max, y_max, z_max;
    double r_min, r_max;
    double light_cone_x0;
    double light_cone_y0;
    double light_cone_z0;
    int64_t npart;
    int nx;
    int fof;
    int so200b;
    int units_rockstar;
    int bins_per_decade;
    char mass_name[64];
    char parent_name[64];
    char model[64];
    char sigma[FILENAME_MAX];
    char hdr[FILENAME_MAX];
    char in[FILENAME_MAX];
    char out[FILENAME_MAX];
} options_s;

#define scan(s, v, f) sscanf(s, #v "=" #f, &opt->v)
#define scans(s, v, f) sscanf(s, #v "=" #f, opt->v)

void
parse_opt(int argc, char *argv[], options_s *opt)
{
    for (int i = 1; i < argc; i++) {
	char *p = argv[i];
	int ret;
	ret = 0;
	ret += scan(p, L0, %lf);
	ret += scan(p, Omega0_m, %lf);
	ret += scan(p, h_100, %lf);
	ret += scan(p, redshift, %lf);
	ret += scan(p, rho_crit, %lf);
	ret += scan(p, part_mass, %lf);
	ret += scan(p, err_sigma, %lf);
	ret += scan(p, vol_frac, %lf);
	ret += scan(p, x_min, %lf);
	ret += scan(p, y_min, %lf);
	ret += scan(p, z_min, %lf);
	ret += scan(p, x_max, %lf);
	ret += scan(p, y_max, %lf);
	ret += scan(p, z_max, %lf);
	ret += scan(p, r_min, %lf);
	ret += scan(p, r_max, %lf);
	ret += scan(p, light_cone_x0, %lf);
	ret += scan(p, light_cone_y0, %lf);
	ret += scan(p, light_cone_z0, %lf);
	ret += scan(p, npart, %ld);
	ret += scan(p, nx, %d);
	ret += scan(p, fof, %d);
	ret += scan(p, so200b, %d);
	ret += scan(p, units_rockstar, %d);
	ret += scan(p, bins_per_decade, %d);
	ret += scans(p, mass_name, %64s);
	ret += scans(p, parent_name, %64s);
	ret += scans(p, model, %64s);
	ret += scans(p, sigma, %256s);
	ret += scans(p, hdr, %256s);
	ret += scans(p, in, %256s);
	ret += scans(p, out, %256s);
	if (ret != 1) fprintf(stderr, "Failed to parse %s\n", p);
    }
}

float *
read_filtered(SDF *inpt, struct options_s *opt, int64_t *nn)
{
    int64_t N = *nn;
    float *mtab;
    int chunk = 128*1024*1024;
    
    if (!opt->parent_name) {
	mtab = Malloc(N * sizeof(float));
	for (int64_t off = 0; off < N; off += chunk) {
	    int n = (N-off > chunk) ? chunk : N-off;
	    int ret = SDFseekrdvecs(inpt,
				    opt->mass_name, off, n, mtab, sizeof(float),
				    NULL);
	    if (ret != 0) Error("%s", SDFerrstring);
	}
    } else if (!SDFhasname(opt->parent_name, inpt)) {
	Error("File does not have %s\n", opt->parent_name);
    } else if ((opt->x_max - opt->x_min == 0.0) && (opt->r_max - opt->r_min == 0.0)) {
	mtab = Malloc(N * sizeof(float));
	int64_t *stab = Malloc(chunk * sizeof(int64_t));
	int64_t nparents = 0;
	for (int64_t off = 0; off < N; off += chunk) {
	    int n = (N-off > chunk) ? chunk : N-off;
	    /* SDFseekrdvecs can't read more than 2G elements at a time */
	    int ret = SDFseekrdvecs(inpt,
				    opt->mass_name, off, n, mtab+off, sizeof(float),
				    opt->parent_name, off, n, stab, sizeof(int64_t),
				    NULL);
	    if (ret != 0) Error("%s", SDFerrstring);
	    for (int i = 0; i < n; i++) {
		if (stab[i] == -1) {
		    mtab[nparents++] = mtab[off+i];
		}
	    }
	    fprintf(stderr, "chunk %ld\n", 1+off/chunk);
	}
	Free(stab);
	fprintf(stderr, "Removed %ld subhalos (%.1f%%)\n", 
		N-nparents, (N-nparents)/(N*0.01));
	N = nparents;
    } else {
	float *mbuf = Malloc(chunk * sizeof(float));
	int64_t *stab = Malloc(chunk * sizeof(int64_t));
	float *x = Malloc(chunk * sizeof(float));
	float *y = Malloc(chunk * sizeof(float));
	float *z = Malloc(chunk * sizeof(float));
	int64_t ninside = 0;
	int64_t nparents = 0;
	int64_t msize = 2*chunk;
	mtab = Malloc(msize * sizeof(float));
	for (int64_t off = 0; off < N; off += chunk) {
	    int n = (N-off > chunk) ? chunk : N-off;
	    /* SDFseekrdvecs can't read more than 2G elements at a time */
	    if (nparents + n >= msize) {
		msize += chunk;
		mtab = Realloc(mtab, msize*sizeof(float));
	    }
	    int ret = SDFseekrdvecs(inpt,
				    "x", off, n, x, sizeof(float),
				    "y", off, n, y, sizeof(float),
				    "z", off, n, z, sizeof(float),
				    opt->mass_name, off, n, mbuf, sizeof(float),
				    opt->parent_name, off, n, stab, sizeof(int64_t),
				    NULL);
	    if (ret != 0) Error("%s", SDFerrstring);
	    if (opt->x_max - opt->x_min > 0.0) {
		for (int i = 0; i < n; i++) {
		    if (x[i] < opt->x_min || y[i] < opt->y_min || z[i] < opt->z_min) continue;
		    if (x[i] >= opt->x_max || y[i] >= opt->y_max || z[i] >= opt->z_max) continue;
		    ninside++;
		    if (stab[i] == -1) {
			mtab[nparents++] = mbuf[i];
		    }
		}
	    } else if (opt->r_max - opt->r_min > 0.0) {
		for (int i = 0; i < n; i++) {
		    float xx[3] = {x[i] - opt->light_cone_x0,
				   y[i] - opt->light_cone_y0,
				   z[i] - opt->light_cone_z0};
		    float r = sqrtf(xx[0]*xx[0] + xx[1]*xx[1] + xx[2]*xx[2]);
		    if (r < opt->r_min || r >= opt->r_max) continue;
		    ninside++;
		    if (stab[i] == -1) {
			mtab[nparents++] = mbuf[i];
		    }
		}
	    }
	    fprintf(stderr, "chunk %ld\n", 1+off/chunk);
	}
	Free(z); Free(y); Free(x);
	Free(stab);
	Free(mbuf);
	fprintf(stderr, "%ld halos in spatial selection\n", ninside);
	fprintf(stderr, "Removed %ld subhalos (%.1f%%)\n", 
		ninside-nparents, (ninside-nparents)/(ninside*0.01));
	N = nparents;
    }
    *nn = N;
    mtab = Realloc(mtab, N * sizeof(float));
    return mtab;
}

int
main(int argc, char *argv[])
{
    options_s opt = {.model = "T08code", .bins_per_decade = 20, .mass_name = "m200b", .parent_name = "pid",
    		     .vol_frac = 1.0, .rho_crit = 2.7753666e+11, /* 3H^2/8piG in [Msun / h] / [Mpc / h]^3 */
    };
    if (argc < 3) {
	fprintf(stderr, "Required arguments: in=filename sigma=filename\n");
	fprintf(stderr, "Optional arguments: model hdr mass_name parent_name fof units_rockstar part_mass L0 Omega0_m h_100 redshift\n");
	fprintf(stderr, "Optional arguments: [xyz]_min [xyz]_max light_cone_[xyz]0\n");
	exit(1);
    } else {
	parse_opt(argc, argv, &opt);
    }

    FILE *sfile;
    Fopen(sfile, opt.sigma, "r"); /* fail early if bad filename */
    Fclose(sfile);

    int nmodels = 6;
    pdf_params_s p[] = {[0].A = 0.19968, [0].a = 1.69609, [0].b = 0.54733, [0].c = 1.21417, [0].d = 0.982,
			[0].name = "lanl", [0].pdf = &pdf_lanl,
			[1].A = 0.10929, [1].a = 1.69605, [1].b = 1.42670, [1].c = 1.21417, [1].d = 0.982,
			[1].name = "tinker", [1].pdf = &pdf_tinker,
			[2].A = 0.1858659, [2].a = 1.466904, [2].b = 2.571104, [2].c = 1.193958, [2].d = 1.0,
			[2].name = "T08code", [2].pdf = &pdf_tinker,
			[3].A = 0.186, [3].a = 1.47, [3].b = 2.57, [3].c = 1.19, [3].d = 1.0,
			[3].name = "T08paper", [3].pdf = &pdf_tinker,
			[4].A = 0.72646, [4].a = 1.6394, [4].b = 0.25436, [4].c = 1.2023, [4].d = 1.0,
			[4].name = "W06", [4].pdf = &pdf_lanl,
			[5].A = 0.333, [5].a = 0.788, [5].b = 0.807, [5].c = 1.795, [5].d = 0.0,
			[5].name = "bhat11", [5].pdf = &pdf_bhat
    };

    int mn = -1;
    for (int i = 0; i < nmodels; i++) {
	if (!strcmp(opt.model, p[i].name)) {
	    mn = i;
	}
    }
    if (mn == -1) Error("Unknown model\n");

    if (opt.redshift > 0.0 && mn == 2) {
	double delta_halo = 200.0;
	double Tinker_alpha = -pow(0.75/log10(delta_halo/75.0), 1.2);
	double r = (opt.redshift < 3.0) ? opt.redshift : 3.0;
	p[mn].A *= pow(1.0+r, -0.14);
	p[mn].a *= pow(1.0+r, -0.06);
	Tinker_alpha = pow(10.0, Tinker_alpha);
	p[mn].b *= pow(1.0+r, -Tinker_alpha);
    }

    SDF *inpt = SDFopen(opt.hdr, opt.in);
    if (inpt == NULL) Error("Sorry, couldn't SDFopen %s\n%s\n", opt.in, SDFerrstring);

    int64_t N;
    if (SDFhasname("nhalos", inpt)) {
	SDFgetint64OrDie(inpt, "nhalos", &N);
    } else {
	N = SDFnrecs(opt.mass_name, inpt);
    }
    fprintf(stderr, "Reading %ld halos\n", N);
    double pmass, L0, Omega0_m, h_100;
    int units_rockstar, fof;
    /* Try to find data in inpt, or else default to values in opt */
    SDFgetintOrDefault(inpt, "fof", &fof, opt.fof);
    SDFgetintOrDefault(inpt, "rockstar_units", &units_rockstar, opt.units_rockstar);
    SDFgetintOrDefault(inpt, "units_rockstar", &units_rockstar, units_rockstar);
    SDFgetdouble(inpt, "light_cone_x0", &opt.light_cone_x0);
    SDFgetdouble(inpt, "light_cone_y0", &opt.light_cone_y0);
    SDFgetdouble(inpt, "light_cone_z0", &opt.light_cone_z0);
    if (units_rockstar) {
	SDFgetdoubleOrDefault(inpt, "part_mass",  &pmass, opt.part_mass);
	SDFgetdoubleOrDefault(inpt, "BOX_SIZE",  &L0, opt.L0);
	if (SDFgetdouble(inpt, "Omega0_m",  &Omega0_m)) {
	    SDFgetdoubleOrDefault(inpt, "Omega0",  &Omega0_m, opt.Omega0_m);
	}
    } else {
	/* Change to 1/h units */
	int64_t npart;
	double H0;
	SDFgetdoubleOrDefault(inpt, "part_mass",  &pmass, opt.part_mass);
	SDFgetdoubleOrDefault(inpt, "particle_mass",  &pmass, pmass);
	SDFgetdoubleOrDefault(inpt, "L0",  &L0, opt.L0);
	if (SDFgetdouble(inpt, "Omega0_m",  &Omega0_m)) {
	    SDFgetdoubleOrDefault(inpt, "Omega0",  &Omega0_m, opt.Omega0_m);
	}
	SDFgetdoubleOrDefault(inpt, "H0",  &H0, 0.0);
	SDFgetdoubleOrDefault(inpt, "h_100",  &h_100, opt.h_100);
	SDFgetint64OrDefault(inpt, "original_npart", &npart, opt.npart);
	if (h_100 == 0.0) {
	    if (H0 == 0.0) Error("Can't deduce h_100\n");
	    h_100 = 10.0*H0*(one_kpc/one_Gyr);
	}
	L0 *= h_100/1000.0;
	opt.light_cone_x0 *= h_100/1000.0;
	opt.light_cone_y0 *= h_100/1000.0;
	opt.light_cone_z0 *= h_100/1000.0;
        if (pmass == 0.0) {
            if (npart == 0LL) Error("Can't deduce pmass\n");
            pmass = Omega0_m * opt.rho_crit * pow(L0, 3.0) / npart;
        } else {
	    pmass *= 1e10*h_100;
	}

    }
    double mmin = 19.5*pmass;
    float *mtab = read_filtered(inpt, &opt, &N);
    double vol = L0*L0*L0*opt.vol_frac;
    if (opt.x_max-opt.x_min > 0.0) {
	vol = opt.x_max - opt.x_min;
	vol *= opt.y_max - opt.y_min;
	vol *= opt.z_max - opt.z_min;
    } else if (opt.r_max-opt.r_min > 0.0) {
	vol = (4.0 * M_PI / 3.0) * (pow3(opt.r_max) - pow3(opt.r_min));
    }

    if (!units_rockstar) {
	for (int64_t i = 0; i < N; i++) {
	    mtab[i] *= 1e10*h_100;
	}
    }
    if (!strcmp(opt.mass_name, "vmax")) {
	for (int64_t i = 0; i < N; i++) {
	    mtab[i] = pow(36.5*mtab[i], 3.17);
	}
    }
    if (fof) {
	for (int64_t i = 0; i < N; i++) {
	    /* This messes up the bin edges (no longer between masses) */
	    mtab[i] *= (1.0-pow(mtab[i]/pmass, -0.6)); /* FOF correction */
	}
    }

    p[mn].rho0 = Omega0_m*opt.rho_crit;

    init_spline(opt.sigma, &p[mn].acc, &p[mn].spline, &p[mn].dacc, &p[mn].dspline);
	
    FILE *output;
    char buffer[1024];
	
    gsl_histogram *h = histogramf(mtab, N, pmass, opt.bins_per_decade, opt.err_sigma);
    Free(mtab);

    gsl_integration_workspace *w = gsl_integration_workspace_alloc(1000);
    gsl_function fn;
    double result, error;
    fn.function = p[mn].pdf;
    fn.params = &p[mn];

    char *b;
    char suffix[32];
    if (!strcmp(opt.parent_name, "all")) {
	snprintf(suffix, sizeof(suffix), ".hist%d_all", opt.bins_per_decade);
    } else if (opt.err_sigma != 0.0) {
	snprintf(suffix, sizeof(suffix), ".hist%d_err%03d", opt.bins_per_decade,
		 (int)(opt.err_sigma*1000.0));
    } else if (opt.redshift != 0.0) {
	snprintf(suffix, sizeof(suffix), ".hist%dz_", opt.bins_per_decade);
    } else {
	snprintf(suffix, sizeof(suffix), ".hist%d_", opt.bins_per_decade);
    }
    if (opt.out[0]) {
	b = opt.out;
    } else {
	strncpy(buffer, opt.in, sizeof(buffer));
	if (strrchr(buffer, '/')) b = strrchr(buffer, '/')+1;
	else b = buffer;
    }
    strcat(b, suffix);
    strcat(b, opt.mass_name);
    Fopen(output, b, "w");
    
    gsl_set_error_handler_off();
    write_header2(output, N, &p[mn], opt.sigma);
    fprintf(output, "# bin_center_mass dn/dlnM sigma dlogsdlogm lower/pmass n expected dm ds dlnm dlns\n");
    fprintf(output, "# L0 %.8g\n", L0);
    fprintf(output, "# volume %.8g [Mpc^3/h^3]\n", vol);
    if (opt.light_cone_x0 != 0.0 || opt.light_cone_y0 != 0.0 || opt.light_cone_z0 != 0.0) {
	fprintf(output, "# light_cone_x %.8f %.8g %.8g [Mpc/h^3]\n", 
		opt.light_cone_x0, opt.light_cone_y0, opt.light_cone_z0);
    }
    if (opt.x_max-opt.x_min > 0.0) { 
	fprintf(output, "# x_min %.8g y_min %.8g z_min %.8g [Mpc/h]\n", opt.x_min, opt.y_min, opt.z_min);
	fprintf(output, "# x_max %.8g y_max %.8g z_max %.8g [Mpc/h]\n", opt.x_max, opt.y_max, opt.z_max);
    } else if (opt.r_max-opt.r_min > 0.0) { 
	fprintf(output, "# r_min %.8g r_max %.8g [Mpc/h]\n", opt.r_min, opt.r_max);
    }
    fprintf(output, "# Omega0_m %.8g\n", Omega0_m);
    fprintf(output, "# redshift %.8g\n", opt.redshift);
    fprintf(output, "# bins_per_decade %d\n", opt.bins_per_decade);
    fprintf(output, "# err_sigma %g\n", opt.err_sigma);
    fprintf(output, "# using mass label %s parent label %s\n", opt.mass_name, opt.parent_name);
    for (int i = 0; i < gsl_histogram_bins(h); i++) {
	double lower, upper;
	gsl_histogram_get_range(h, i, &lower, &upper);
	double center = pow(10.0, (log10(lower)+log10(upper))/2.0);
	double count = gsl_histogram_get(h, i);
	if (count > 0.0 && lower >= mmin) {
	    double lower_sigma = gsl_spline_eval(p[mn].spline, log(lower), p[mn].acc);
	    double upper_sigma = gsl_spline_eval(p[mn].spline, log(upper), p[mn].acc);
	    if (gsl_integration_qag(&fn, lower, upper, 0.1, 0.0, 1000, 3, w, &result, &error)) {
		fprintf(stderr, "gsl_integration failed\n");
		result = 0.0;
	    }
	    fprintf(output, "%14.8lg %14.8lg %14.8lg %14.8lg %12.2lf %12.3lf %12.3f %14.8lg %14.8lg %14.8lg %14.8lg\n", 
		    center, count*(log(upper)-log(lower))/vol, 
		    gsl_spline_eval(p[mn].spline, log(center), p[mn].acc),
		    gsl_spline_eval(p[mn].dspline, log(center), p[mn].dacc),
		    lower/pmass, count, result*vol, upper-lower, lower_sigma-upper_sigma,
		    log(upper)-log(lower), log(lower_sigma)-log(upper_sigma));
	}
    }
    Fclose(output);
    
    gsl_histogram_free(h);
}
