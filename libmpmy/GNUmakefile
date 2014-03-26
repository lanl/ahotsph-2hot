# Make.$(ARCH) sets many of the variables used below including:
# CC, CFLAGS, AS, RANLIB, objdir, objsuf, asmdir

# This Makefile is 'non-standard' because we are trying to create a .o
# file in the '$(libdir)' directory.  Furthermore, the name of the
# file we are creating can't be determined until after we have 'include'd
# the $(ARCH)-specific makefile (so we can use the default PAROS).  
#
# The dependencies are even more unreliable than usual.
#
# It's amazing that it works at all...

treedir=..
treedir_sed=\.\.
appexcludes=
libname=NO_NAME
# This is here only to allow 'make depends' to work.
src=mpmy_seq.c mpmy_lsv.c mpmy_nx.c mpmy_vertex.c mpmy_sunmos.c mpmy_mpi.c mpmy_eui.c mpmy_pvm.c mpmy_craypvm.c

special_rule_for_all=YES
all: special

include $(treedir)/Make-common/Make.$(ARCH)

include $(treedir)/Make-common/Make.generic

override CFLAGS:=$(PAROSCFLAGS) $(CFLAGS) 

# I think these should be handled with PAROSCFLAGS
# So sue me!
#ifeq ($(PAROS),eui)
#CFLAGS:=-I/usr/lpp/euih/eui $(CFLAGS)
#endif

#ifeq ($(PAROS),mpi)
#CFLAGS:=-I/usr/lpp/mpif $(CFLAGS)
#endif

#ifeq ($(PAROS),srv)
#CFLAGS:=-I$(SRVHOME)/include/cros3 $(CFLAGS)
#endif

#ifeq ($(PAROS),pvm)
#CFLAGS:=-I$(PVM_ROOT)/include $(CFLAGS)
#endif

# order is important here.  We need to make $(libdir)
special: $(libdir)/mpmy_$(PAROS)$(objsuf)

$(libdir)/mpmy_$(PAROS)$(objsuf) : $(objdir)/mpmy_$(PAROS)$(objsuf) $(libdir)
	cp $< $@

$(objdir)/mpmy_$(PAROS)$(objsuf) : $(objdir)

# If there's a default PAROS for this architecture, then we put
# mpmy_$(PAROS).o into libsw.a.  To override it, it is
# necessary to list mpmy_$(alternative).o AHEAD of libsw.a in the
# link command.  But if you just want the defaults (typically _seq), then
# the only thing to link against is libsw.a.
ifeq ($(PAROS),$(defaultPAROS))
special: $(libdir)/libsw$(libext)($(objdir)/mpmy_$(PAROS)$(objsuf))
	$(RANLIB) $(libdir)/libsw$(libext)

$(libdir)/libsw$(libext)($(objdir)/mpmy_$(PAROS)$(objsuf)) : $(libdir)

endif

# DO NOT DELETE THIS LINE -- make depend depends on it.

$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf): $(treedir)/include/Msgs.h $(treedir)/include/gccextensions.h
$(objdir)/mpmy_seq$(objsuf): $(treedir)/include/mpmy.h $(treedir)/include/timers.h
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf): $(treedir)/include/Assert.h $(treedir)/include/error.h
$(objdir)/mpmy_seq$(objsuf): $(treedir)/include/mpmy_io.h
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf): $(treedir)/include/mpmy_time.h $(treedir)/include/mpmy_abnormal.h
$(objdir)/mpmy_seq$(objsuf): timers_hwclock.c $(treedir)/include/Malloc.h $(treedir)/include/chn.h
$(objdir)/mpmy_seq$(objsuf): $(treedir)/include/cpu.h
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf): mpmy_io.c
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf): $(treedir)/include/protos.h
$(objdir)/mpmy_seq$(objsuf): iozero.h iozero.c io_generic.c mpmy_abnormal.c
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf):
$(objdir)/mpmy_seq$(objsuf): $(treedir)/include/singlio.h
$(objdir)/mpmy_seq$(objsuf): mpmy_generic.c
$(objdir)/mpmy_mpi$(objsuf): /opt/cray/mpt/6.2.0/gni/mpich2-gnu/48/include/mpi.h
$(objdir)/mpmy_mpi$(objsuf): /opt/cray/mpt/6.2.0/gni/mpich2-gnu/48/include/mpio.h
$(objdir)/mpmy_mpi$(objsuf): $(treedir)/include/mpmy_abnormal.h $(treedir)/include/gccextensions.h
$(objdir)/mpmy_mpi$(objsuf): $(treedir)/include/Malloc.h $(treedir)/include/error.h $(treedir)/include/chn.h
$(objdir)/mpmy_mpi$(objsuf): $(treedir)/include/mpmy.h $(treedir)/include/timers.h
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf): $(treedir)/include/Assert.h
$(objdir)/mpmy_mpi$(objsuf): $(treedir)/include/Msgs.h $(treedir)/include/memfile.h
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf): timers_hwclock.c $(treedir)/include/mpmy_time.h $(treedir)/include/cpu.h
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf): mpmy_mpiio.c
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf): $(treedir)/include/protos.h
$(objdir)/mpmy_mpi$(objsuf): $(treedir)/include/mpmy_io.h io_generic.c mpmy_abnormal.c
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf):
$(objdir)/mpmy_mpi$(objsuf): $(treedir)/include/singlio.h
$(objdir)/mpmy_mpi$(objsuf): mpmy_generic.c
