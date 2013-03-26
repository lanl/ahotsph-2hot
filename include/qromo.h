float qromo(float (*func)(float), float a, float b,
	float (*choose)(float (*)(float), float, float, int));
float midpnt(float (*func)(float), float a, float b, int n);

double qromod(double (*func)(double), double a, double b,
	double (*choose)(double (*)(double), double, double, int));
double midpntd(double (*func)(double), double a, double b, int n);

