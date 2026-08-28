/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef _SDFWriteDOTh
#define _SDFWriteDOTh

#ifdef __cplusplus
extern "C" {
#endif
void SDFwrite(const char *filename,
              int gnobj,
              int nobj,
              const void *btab,
              int bsize,
              const char *bodydesc,
              /* const char *name, SDF_type_enum type, <type> val */...);
void SDFwrite64(const char *filename,
                int64_t gnobj,
                int64_t nobj,
                const void *btab,
                int bsize,
                const char *bodydesc,
                /* const char *name, SDF_type_enum type, <type> val */...);
void SDFappend64(const char *filename,
                 int64_t gnobj,
                 int64_t nobj,
                 const void *btab,
                 int bsize,
                 const char *bodydesc,
                 /* const char *name, SDF_type_enum type, <type> val */...);

/* A trivial special case, just don't write any body data */
void SDFwritehdr(const char *filename,
                 const char *bodydesc,
                 /* const char *name, SDF_type_enum type, <type> val */...);

void SDFunsetwroteheader(void);
void SDFsetwroteheader(void);

#ifdef __cplusplus
}
#endif

#endif
