TREEHOME=../../
##### Application-specific stuff goes here
treedir=$(TREEHOME)

EXTRACFLAGS=$(GSL_INCLUDE_OPTS)
PRELIBS=$(GSL_LIB)
prog=lcjoin_iter
src=$(prog).c version.c
programname=$(prog)

##### End of application-specific setup

include $(treedir)/Make-common/Make.$(ARCH)

include $(treedir)/Make-common/Make.generic

.PHONY: version.proto
version.proto:
	@echo char Version\[\] = \"`git describe --dirty --tags`\"\; > version.proto
	@echo char Compiled_date\[\] = __DATE__\; >> version.proto
	@echo char Compiled_time\[\] = __TIME__\; >> version.proto

version.c: version.proto
	@cmp -s $< $@ || cp -p $< $@


# DO NOT DELETE THIS LINE -- make depend depends on it.
