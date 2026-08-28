/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

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
        printf("%p : (%s+0x%x) [%p]\n", pc, fname, offset, pc);
    }
}
