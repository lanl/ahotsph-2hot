cmake_minimum_required(VERSION 2.6)

#Add HOT_CLASS dirs as they are generic enough to be dangerous
include_directories(${HOTCLASS_INCLUDE_DIRS})

# If OSX, also need to build extra file and download header
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_library(libsw
        Malloc.c 	key.c
        Msgs.c		error.c 	files.c
        chn.c		dll.c
        stk.c		heap.c		randoms.c
        sw_malloc.c	gc.c		byteswap.c
        timers.c	SDFwrite.c	SDFread.c
        ring.c		qromo.c		qromod.c
        cosmo.c		mpmy_combine.c	counters.c
        mpmy_gather.c	memfile.c 	rsort.c
        msgdirinit.c	singlio.c	abm.c
        mpi_bcast.c	mpi_reduce.c	keycvt.c
        peano.c		SDFreadf.c	hwclock.c
        class_params.c	cosmo_tbl.c
        obstack.c
    )
else()
    add_library(libsw
        Malloc.c 	key.c
        Msgs.c		error.c 	files.c
        chn.c		dll.c
        stk.c		heap.c		randoms.c
        sw_malloc.c	gc.c		byteswap.c
        timers.c	SDFwrite.c	SDFread.c
        ring.c		qromo.c		qromod.c
        cosmo.c		mpmy_combine.c	counters.c
        mpmy_gather.c	memfile.c 	rsort.c
        msgdirinit.c	singlio.c	abm.c
        mpi_bcast.c	mpi_reduce.c	keycvt.c
        peano.c		SDFreadf.c	hwclock.c
        class_params.c	cosmo_tbl.c
    )
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

target_link_libraries(libsw
	libmpmy ${OPENSSL_LIBRARIES}
)
