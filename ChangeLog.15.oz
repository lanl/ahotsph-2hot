Mon Sep 26 18:54:41 1994  John Salmon  (johns@mullet.anu.edu.au)

	* Merged it all back into tree16, along with msw's edits
	from August and September.

Sat Sep 24 10:26:02 1994  John Salmon  (johns@mullet.anu.edu.au)

	* include/*.h : Made everything C++-friendly by surrounding
	all the externs with:

 	#ifdef __cplusplus
	extern "C"{
        #endif
        .....
        #ifdef __cplusplus
        }
        #endif

	Hopefully I didn't misspell anything.  The files with extensive
	inlining technology may not be right, but I don't think I did
	any damage that would affect C compilation...  A couple of files
	(tree.h, heap.h) required some additional shuffling but most just
	got the above lines inserted.

	* libmpmy/mpmy_{unix|par}io.c : check for NULL arguments.
	Set errno = EINVAL and return -1 if detected.

Fri Sep 23 10:02:30 1994  John Salmon  (johns@mullet.anu.edu.au)

	* libsw/timers.c:  #undef some things to prevent conflicts.

	* libSDF/SDF_Hdrio.c:  don't raise assertions.  Just exit with
	an error code if something goes wrong.

	* libSDF/SDFfuncs.c:  Build a hash table for the names in
	the sdf header.  It was hoped that this would speed up sequential
	access to SDF files.  There is still the overhead in SDFrdvecs,
	but a large fraction of the time was spent in lookup, which should
	be much faster now.  Sadly, this was not the hoped-for panacea.
	It's much faster, but it's still too slow.  More speed is going
	to require some kind of 'SDFrdagain' function that bypasses all the
	setup machinery in SDFrdvecs and just does the last step over again.
	The code was not written with this in mind - to say the least.

	* libmpmy/mpmy_unixio.c (MPMY_*): Make sure that fp is non-null
	before doing anything else.  It's uncool for the MPMY functions to
	segfault.  This should get repaired in the other mpmy_*io.c as well.
	It would be nice to set errno too...

Wed Sep 21 14:57:36 1994  John Salmon  (johns@mullet.anu.edu.au)

	* libmpmy/mpmy_{nx|cm5}.c: both now #include "mpmy_pario.c", which
	is a merged io.c for both CM5 and NX.  As far as NX is concerned,
	it should be indistinguishable from paragonio.c (which was largely
	untested anyway.)  Cubix/srv support could easily be added.  Any
	other "parallel I/O" models?

	* libmpmy/mpmy_unixio.c:  The comment asked "is this a good idea".
	The answer is "no".  Static "seekptr" no longer maintained.  Just
	call tell0 to find out where we are in the file, and seek0 to put
	everyone at the right place.  Slow but (maybe) correct.

Mon Sep 19 09:12:12 1994  John Salmon  (johns@mullet.anu.edu.au)

	* libmpmy/mpmy_*.c:  eliminate CommAlloc.  Use ChnAlloc instead.
	This means a) there's one less level of indirection and b) the
	comm structures don't move once they're allocated.  It also means
	that they may take a bite out of the middle of memory...

	* libmpmy/mpmy_cm5.c:  use mpmy_unixio.c.  This may be a bad idea,
	but the cm5io.c that we were using wasn't doing any fancy cmmd stuff
	anyway.  It might be worth putting it back, as in paragonio.c.

	* libmpmy/mpmy_{unix|paragon}io.c : split out the 'read0,open0, etc.'
	functions into iozero.c.

	* libsw/walk.c: added include "Assert.h", appeased sgi's compiler
	complaint about illegal automatic variable initializers.

	* include/fastflpt.h:  don't do the pragma in the mips branch.  It's
	done in <math.h> now.  Is math.h required by fastflpt.h?  Should it
	be included at the top?

	* Make-common/Make.IP22{gcc} : updated.  The gcc version appears not
	to work, but lacking a debugger, it's hard to tell why.

Thu Sep  8 13:48:07 1994  John Salmon  (johns@mullet.anu.edu.au)

	* libsw:  removed fseekrd.c.

	* Make-common/Make.generic:  define a defaultPAROS in each of
	the Make.$(ARCH) files.  Put the defaultPAROS' mpmy_???.o into
	$(ARCH)libsw.a.  This means that it is no longer necessary to
	name mpmy_???.o in the link command for the default PAROS.  It
	is thus much easier to link, e.g., SDF without using the whole
	machinery of our makefile madness.  If you want to use a non-default
	PAROS, you have to list it AHEAD of libsw.a, but you've had to
	do that all along to avoid unresolved externals.

Wed Sep  7 01:01:33 1994  John Salmon  (johns@mullet.anu.edu.au)

	* libsw/Msgs.c (Msg_test): Added an #undef Msg_test, to allow
	Msgs.c to be compiled with -DNO_MSGS.

	*  libsw/SDFwrite.c:  Moved SDFwrite2.c to SDFwrite.c.  Convert
	it to use the MPMY_Fopen style of IO rather than the MPMY_Global
	style.  Attempt to recover from certain types of writing errors,
	in particular ETIMEDOUT, which >might< be a transient NFS failure.

Mon Sep  5 15:38:55 1994  John Salmon  (johns@mullet.anu.edu.au)

	*  libmpmy/mpmy_unixio.c:  Call Shout instead of Error in 
	MPMY_Fwrite and MPMY_Fread.  Be a little more careful about
	broadcasting the # bytes actually read from node 0 in Fread.

	* libmpmy/mpmy_unixio.c (MPMY_Ftell): Removed an #ifndef __DELTA__
	that was causing problems.  Why was it there???

Thu Sep  1 11:15:03 1994  John Salmon  (johns@mullet.anu.edu.au)

	* libmpmy/mpmy_lsv.c: Added MPMY_Flick.  This may be a bad idea.

	* libsw/lsv.c : Bumped MAXDEFER to 1000.  Will this allow
	100k bodies to run on a 4 processor solaris machine??

	 * Tried to repair the dates on the source code by extracting a tar
	 file.  Possible make confusion to ensue

	 * libmpmy/mpmy_*io.c:  Fixed the second argument to Bcast, which
	 should usually be 1, NOT sizeof(arg1).

Mon Aug 29 00:01:00 1994  John Salmon  (johns@mullet.anu.edu.au)

	* Makefile:  build relerr after all the libraries, by default.

Fri Aug 26 01:55:30 1994  John Salmon  (johns@macdougal)

	* libSDF: Added code to SDF-lex.l and stdio.h so they're
	compatible with Flex.  Cosmetic edit to SDF-parse.y.

	* MANY OTHER CHANGES in the last few days which weren't recorded.
	Most were in libmpmy, which is all-new anyway...

Tue Aug 23 21:10:28 1994  John Salmon  (johns@macdougal)

	* Makefile, include/protos.h, Make-common/Make.generic,
	libmpmy/*cm5*.c, libsw/lsv.c, libsw/sigio_dump.c: minor tweaks to
	make it compile on the ANU cm5.  ARCH must be set to sun4proto, which
	alters some of the prototypes.

Mon Aug 15 11:44:09 1994  John Salmon  (johns@minuet)

	* mpmy_*.c: Add MPMY_Flick to replace uSleep.

	* libtree/nlcomm.c: Change uSleep to MPMY_Flick.

	* include/getparam.h (Getfparam): Use MPMY_Bcast instead of x_concat.

	* libtree/tree.c:  Replace tree.c with tree2.c from relerr.

	* libsw/collective.[ch]:  History.

Mon Aug 15 11:09:28 1994  John Salmon  (johns@merlin)

	* libmpmy/mpmy_unixio.c: Use lseek instead of tell.  lseek
	is posix.  tell is bsd-only?

Fri Aug 12 16:59:46 1994  John Salmon  (johns@mdwarf)

	* mpmy_nx[x].c:  Merged the paragon and delta versions.  There's
	 a non-trivial difference because the paragon has irecvx which is
	 MUCH more useful than irecv.  The nx.c (delta) version had a lot
	 of defensive programming from our last bug-hunting expedition which
	 are now in the paragon code too.

	* libmpmy:  removed sysdep.  All of the sysdep_* files were
	almost identical, so I moved them into libsw/singlio.c and 
	libsw/msgdirinit.c.  The truly system-dependent stuff is now
	in 'mpmy_$(PAROS).o', and the directory is called libmpmy.
	If I got all the fixes in the Make.generic, this shouldn't even
	affect compilation of 'applications'.

Wed Aug 10 09:08:43 1994  John Salmon  (johns@mawson)

	*  libSDF: Adopt the new version that uses MPMY_IO stuff.

	* libSDF/GNUmakefile:  try to use an ARCH-dependent yacc and lex.
	This has been a real headache in portability.  Maybe this will fix it.

Sat Aug  6 16:01:47 1994  John Salmon  (johns@macdougal)

	* lsv/lsvstart: a better default for the lsv_start_prog variable.

	* lsv/lsvstart: Don't trap signal 18.  It's SIGTSTP on Sunos (bsd?)
	systems, but it's SIGCHLD on Solaris (sysV?).  It's just too 
	much trouble to try to toggle.  Sending ^Z to an lsv will be
	different, but what was it before??

	* libsw/lsv.c: create an initial test that decides whether to
	set HAVE_SIGVEC.  Then use that in various places throughout.

	* libsw/Make.sun5: need bcopy.c, and lsv.c, don't need sigio_dump.c

	* include/timers.h: SUN5 doesn't have getrusage.  THIS IS A MESS!

	* lsv/lsv.c: don't try to fprintf a NULL string around line 151.

