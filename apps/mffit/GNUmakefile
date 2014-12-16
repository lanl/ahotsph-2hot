TREEHOME=../..
##### Application-specific stuff goes here
treedir=../..

EXTRACFLAGS=$(GSL_INCLUDE_OPTS)
PRELIBS=$(GSL_LIB)
prog=mffit
src=$(prog).c version.c
programname=$(prog)

##### End of application-specific setup

include $(treedir)/Make-common/Make.$(ARCH)

include $(treedir)/Make-common/Make.generic

.PHONY: version.proto
version.proto:
	@echo char mffit_version\[\] = "\"`git describe --tags  --dirty` \"__DATE__\" \"__TIME__;" > version.proto

version.c: version.proto
	@cmp -s $< $@ || cp -p $< $@


# DO NOT DELETE THIS LINE -- make depend depends on it.
