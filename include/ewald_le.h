void ewald_background(
    const float xx[NDIM], float mass, double phicorr, double acc[NDIM], double *phi);
void ewald_le(const float xx[NDIM], double acc[NDIM], double *phi, float *Q, int nimage);
void calculate_cartesian_moments(body *btab, int nobj, double L, float *Q, int msb);
void cube_acc(const float m, const float *f, double a, double *acc);
void cubic_acc(const float *f, float a, float *acc);
void cubic_accd(const float *f, float a, double *accd);
void kubic_acc(const float *f, float a, float *acc);
void kubic_accd(const float *f, float a, double *accd);
