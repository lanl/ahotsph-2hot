#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <stdio.h>

void do_backtrace(void) {
    unw_cursor_t cursor;
    unw_context_t uc;

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    
    while (unw_step(&cursor) > 0) {
	unw_word_t offset, pc;
	char fname[64] = {.[0] = '\0'};
	
	unw_get_reg(&cursor, UNW_REG_IP, &pc);
	unw_get_proc_name(&cursor, fname, sizeof(fname), &offset);
	printf ("%p : (%s+0x%x) [%p]\n", pc, fname, offset, pc);
    }
}
