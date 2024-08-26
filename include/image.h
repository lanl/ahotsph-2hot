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
