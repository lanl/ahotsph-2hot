/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifdef __cplusplus
extern "C" {
#endif
void Image(float *x,
           float *y,
           float *val,
           int stride,
           int nobj,
           float *rmin,
           float *rmax,
           int x_pixels,
           int y_pixels,
           int pixmin,
           int pixmax,
           int do_log,
           char *name);
void ImageSlice(float *x,
                float *y,
                float *z,
                float *val,
                int stride,
                int nobj,
                float *rmin,
                float *rmax,
                int x_pixels,
                int y_pixels,
                int pixmin,
                int pixmax,
                int do_log,
                char *name);
#ifdef __cplusplus
}
#endif
