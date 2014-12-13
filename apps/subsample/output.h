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
