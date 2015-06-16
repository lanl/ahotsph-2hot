defaultCC:=gcc

CC_SPECIFIC:=-g -Wall -std=c99 -D_XOPEN_SOURCE=500 -D_GNU_SOURCE
ARCH_SPECIFIC:=-march=native -DSTK_FORCE_ALIGNMENT=4 -D_FILE_OFFSET_BITS=64 -DUSE_SYSTEM_MALLOC -DUSE_MPIIO -DUSE_HWCLOCK -DPROCS_PER_NODE=16
OPTIMIZE=-O2
AGGRESSIVE_OPT=-Ofast
LDFLAGS=-g
LEX:=flex
YACC:=bison -y

include $(treedir)/Make-common/Make.default

swsrc:=lsv.c swampi.c
asmdir:=asm-sse
asmsrc=do_grav_sse64_noswiz.s do_grav_sse64_noswiz_eps.s do_grav_sse64_nr.s
cppasmsrc=do_grav_sse16_ivec.S

LOADLIBES=-L$(HOT_CLASS) -lclass -lrt

ifeq ($(PAROS),mpi)
LOADLIBES:=-L$(HOT_CLASS) -L$(MPI_ROOT)/lib -L$(MPI_ROOT)/lib64 -lclass -lmpi -lslurm
PAROSCFLAGS:=-I$(MPI_ROOT)/include
endif

ifeq ($(PAROS),mvapich2)
LOADLIBES:=-L/usr/projects/packages/hpctools/mustang/mvapich2/1.7-gcc/lib64 -lmpich -lmpl -lslurm
PAROSCFLAGS:=-I/usr/projects/packages/hpctools/mustang/mvapich2/1.7-gcc/include
endif

