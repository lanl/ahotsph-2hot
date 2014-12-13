void Integrate(const float *inmass, const float *inpos, const float *invel, 
	       const float *inacc, const float *inphi, const int instride,
	       float *outpos, float *outvel, const int outstride,
	       const int n, const double dt, const double dtv,
	       double *tpos, double *tvel,  double *kep, double *pep);

void ConvertV(float *vel, int stride, int n, double scale, int to_physical);

void CosmoIntegrate(const float *inmass, const float *inpos, const float *invel, 
		    const float *inacc, const float *inphi, const int instride,
		    float *outpos, float *outvel, const int outstride,
		    const int n, const double dt, const double dtv,
		    cosmology *cosmo, 
		    double *tpos, double *tvel, double *kep, double *pep);
