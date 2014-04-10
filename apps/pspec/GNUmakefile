TREEHOME=../..
##### Application-specific stuff goes here
treedir=$(TREEHOME)

EXTRACFLAGS=-I$(FFTW_INC) -I$(MPICH_DIR)/include
PRELIBS=-L$(FFTW_DIR) $(CRAY_FFTW_DIR)/sandybridge/lib/libcray_memcpy.so -lfftw3f_mpi -lfftw3f

prog=powspec

src=$(prog).c
programname=$(prog)

##### End of application-specific setup

include $(treedir)/Make-common/Make.$(ARCH)

include $(treedir)/Make-common/Make.generic

# DO NOT DELETE THIS LINE -- make depend depends on it.
