#ifndef _MemfilEdotH
#define _MemfilEdotH

#ifdef __cplusplus
extern "C"{
#endif
void memfile_init(int sz) ;
void memfile_delete(void) ;
void memfile_vfprintf(void *junk, const char *fmt, va_list args) ;
void PrintMemfile(void); 
#ifdef __cplusplus
}
#endif

#endif
