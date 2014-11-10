TREEHOME=../..
##### Application-specific stuff goes here
treedir=$(TREEHOME)

override LD:=gcc

EXTRACFLAGS=-I$(FFTW_INC) -I$(MPICH_DIR)/include
PRELIBS=-L$(FFTW_DIR) -lfftw3f_mpi -lfftw3f

prog=powspec

src=$(prog).c
programname=$(prog)

##### End of application-specific setup

include $(treedir)/Make-common/Make.$(ARCH)

include $(treedir)/Make-common/Make.generic

# DO NOT DELETE THIS LINE -- make depend depends on it.
