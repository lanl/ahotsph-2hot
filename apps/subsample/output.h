/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

void output(const char *outnamebase, int64_t gnobj, int nobj, body *btab, int iter, 
	    double dt, double dtv, 
	    cosmology *cosmo, double tpos, double tvel, 
	    int cosmology, int do_periodic, 
	    float eps, float this_eps_scaled, int force_smoothing_type,
	    float this_tol, float frac_tol, float frac_tol0, 
	    const float *R, const int *N, int write_nfiles, double *ke, double *pe, 
	    int do_output, int identsort_output, int ic_Nmesh, double ic_growthfac,
	    double subsample_fraction, int subsample_random_seed,
	    char *version_nln, char *compiled_date_nln, char *compiled_time_nln
	    );
