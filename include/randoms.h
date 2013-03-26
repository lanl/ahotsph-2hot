#ifndef Randoms2DOTh
#define Randoms2DOTh

#define NTAB 32

typedef struct{
    long idum, idum2;
    long iy, iv[NTAB];
    int did_init;
    int next_norml_ok;
    float next_norml;
} ran_state;

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
void ran_init(int seed, ran_state *st);
float uniform_rand(ran_state *s);
float normal_rand(ran_state *s);
float sphere_rand(ran_state *st, int ndim, float *x);
float cube_rand(ran_state *st, int ndim, float *x);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
