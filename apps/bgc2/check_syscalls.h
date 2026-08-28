/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#ifndef CHECK_SYSCALLS_H
#define CHECK_SYSCALLS_H
#include <stdio.h>
#include <stdlib.h>

void system_error(char *errmsg);
FILE *check_fopen(char *filename, char *mode);
FILE *check_popen(char *command, char *mode);
void *check_realloc(void *ptr, size_t size, char *reason);
size_t check_fread(void *ptr, size_t size, size_t nitems, FILE *stream);
size_t check_fwrite(void *ptr, size_t size, size_t nitems, FILE *stream);
void check_fseeko(FILE *stream, off_t offset, int whence);
char *check_fgets(char *ptr, size_t size, FILE *stream);
FILE *check_rw_socket(char *command, pid_t *pid);
void rw_socket_close(FILE *res, pid_t pid);
void *check_mmap_file(char *filename, char mode, int64_t *length);

#define check_fprintf(file, ...) { if (fprintf(file, __VA_ARGS__) <= 0)	{  \
      fprintf(stderr, "[Error] Failed printf to fileno %d!\n", fileno(file)); \
      perror("[Error] Reason"); \
      exit(1); \
    }}

#define check_realloc_s(x,y,z) { x = check_realloc((x),(y)*(z), "Reallocating " #x ); }

#endif /* CHECK_SYSCALLS_H */
