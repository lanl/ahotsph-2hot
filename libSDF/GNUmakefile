# Make.$(ARCH) sets many of the variables used below including:
# CC, CFLAGS, AS, RANLIB, objdir, objsuf, asmdir

treedir=..
treedir_sed=\.\.
appexcludes=
# Put everything into the libsw library!!!
libname=libsw

# The "SDF" library
src:= \
SDF-parse.c 	SDFfuncs.c	SDFget.c	SDFhdrio.c

include $(treedir)/Make-common/Make.$(ARCH)

include $(treedir)/Make-common/Make.generic

# These rules are slight modifications of the builtin ones
$(objdir)/%.c : %.y
	$(YACC.y) $<
	mv -f y.tab.c $@

$(objdir)/%.c : %.l
#  commands to execute (built-in):
	@$(RM) $@ 
	$(LEX.l) $< > $@

# Makedepends can't pick these up...
$(objdir)/SDF-parse$(objsuf): SDF-private.h $(objdir)/SDF-lex.c $(objdir)/SDF-parse.c
	(cd $(objdir); $(CC) $(CFLAGS) -I../../../include -I../.. -c SDF-parse.c)

# DO NOT DELETE THIS LINE -- make depend depends on it.

$(objdir)/SDFfuncs$(objsuf):
$(objdir)/SDFfuncs$(objsuf):
$(objdir)/SDFfuncs$(objsuf):
$(objdir)/SDFfuncs$(objsuf): $(treedir)/include/Msgs.h $(treedir)/include/gccextensions.h stdio.h
$(objdir)/SDFfuncs$(objsuf): $(treedir)/include/mpmy_io.h SDF-private.h
$(objdir)/SDFfuncs$(objsuf): $(treedir)/include/SDF.h
$(objdir)/SDFfuncs$(objsuf): $(treedir)/include/byteswap.h $(treedir)/include/Malloc.h
$(objdir)/SDFfuncs$(objsuf): $(treedir)/include/error.h $(treedir)/include/protos.h
$(objdir)/SDFget$(objsuf): stdio.h
$(objdir)/SDFget$(objsuf): $(treedir)/include/mpmy_io.h $(treedir)/include/error.h
$(objdir)/SDFget$(objsuf): $(treedir)/include/gccextensions.h $(treedir)/include/SDF.h
$(objdir)/SDFhdrio$(objsuf):
$(objdir)/SDFhdrio$(objsuf): SDF-private.h stdio.h
$(objdir)/SDFhdrio$(objsuf): $(treedir)/include/mpmy_io.h
$(objdir)/SDFhdrio$(objsuf): $(treedir)/include/SDF.h
