cmake_minimum_required(VERSION 2.6)

project(2hot)

# Cmake modules/macros are in a subdirectory to keep this file cleaner
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMakeModules)

# TODO: Add fortran flags
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS)
	#release comes with -O3 by default
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build,
	options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS)
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

enable_language(C)
include(CMakeDetermineCCompiler)

enable_language(Fortran)
include(CMakeDetermineFortranCompiler)


######################################################################
# Compiler Tests
#####################################################################

include(CheckCCompilerFlag)
check_c_compiler_flag("-std=c99" HAVE_C99)
if(NOT HAVE_C99)
	    message(FATAL_ERROR "flag '-std=c99' does not work")
		endif(NOT HAVE_C99)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

		check_c_compiler_flag("-march=native" HAVE_MARCH_NATIVE)
		if(NOT HAVE_MARCH_NATIVE)
		    message(FATAL_ERROR "flag '-march=native' does not work")
endif(NOT HAVE_MARCH_NATIVE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

# include(CheckFortranCompilerFlag)
# TODO: Check each flag we use

########################################################################
# User input options                                                   #
########################################################################
# option(PROCS_PER_NODE "Processors Per Node" 16)
SET(PROCS_PER_NODE "16" CACHE STRING "Number of
Processors per Node (Integer)")
# TODO: Figure out if we care about ARCH outside of
# version.c
SET(CONFIG_ARCH "cmake" CACHE STRING "String
Representing Architecture/Target Machine")
SET(CONFIG_PAROS "seq" CACHE STRING "Parallelization
Scheme")
# TODO: Add params for each flag we want to set at
# cmake time
# COMMENT: Do we even care? It sounds like all inputs
# were machine specific (CMake will find libraries) or
# arch specific (Compiler Tests?)

########################################################################
# General and Machine-Specific Definitions                             #
########################################################################
# Use gnu99 standard, not c99, for M_PI.
add_definitions(-D_XOPEN_SOURCE=500)
add_definitions(-D_GNU_SOURCE)
# Specify number of processors per node for libraries
add_definitions(-DPROCS_PER_NODE=${PROCS_PER_NODE})
# TODO: Are the rest optimizations or required?


########################################################################
# Find external packages/libraries/binaries                            #
########################################################################
# Find libssl
# TODO: Do other SSL implementations do libssl?
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIRS})

# Find libunwind
find_package(Libunwind REQUIRED)
include_directories(${LIBUNWIND_INCLUDE_DIRS})

# Find MPI: Only required if PAROS=mpi
if(${CONFIG_PAROS} STREQUAL "mpi")
find_package(MPI REQUIRED)
endif()

# HOT_CLASS Stuff
find_package(Hotclass REQUIRED)

# Flex
find_package(FLEX REQUIRED)

# Bison
find_package(BISON REQUIRED)

########################################################################
# Basic system tests (standard libraries, headers, functions, types)   #
########################################################################
include(CheckLibraryExists)
# Find libm
set(MATH_LIBRARIES "m" CACHE STRING "math
library")
mark_as_advanced( MATH_LIBRARIES )
# TODO: Add funcs we need
foreach(FUNC sqrt)
	check_library_exists(${MATH_LIBRARIES}
		${FUNC} ""
		FOUND_${FUNC}_${MATH_LIBRARIES})
	if(NOT FOUND_${FUNC}_${MATH_LIBRARIES})
		message(FATAL_ERROR "Could not find needed math function - ${FUNC}")
	endif(NOT
FOUND_${FUNC}_${MATH_LIBRARIES})
endforeach(FUNC)

# Findlibdl
# TODO: Probably should do this here?

######################################
# Actual Targets                     #
######################################
# First add include
# directory as everyone will
# need it
include_directories(include)

# libsw
add_subdirectory(libsw)

# libSDF
add_subdirectory(libSDF)

# libtree
add_subdirectory(libtree)

# libmpmy
add_subdirectory(libmpmy)

# nln driver
IF(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/nln)
	message(STATUS "nlna driver enabled")

	# nln driver's Version.c
	file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/nln)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/nln/version.c
		COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/genNLNVersion.sh" "${CONFIG_ARCH}" "${CMAKE_CURRENT_BINARY_DIR}/nln/version.c"
		COMMENT "Generating version.c for nln ..."
	)

	# nlna driver
	add_executable(nlna
		${CMAKE_SOURCE_DIR}/nln/cofm.c
		${CMAKE_SOURCE_DIR}/nln/grav.c
		${CMAKE_SOURCE_DIR}/nln/mac.c
		${CMAKE_SOURCE_DIR}/nln/main.c
		${CMAKE_SOURCE_DIR}/nln/physics.c
		${CMAKE_SOURCE_DIR}/nln/print.c
		${CMAKE_SOURCE_DIR}/nln/output.c
		${CMAKE_SOURCE_DIR}/nln/integrate.c
		${CMAKE_SOURCE_DIR}/nln/ewald_le2.c
		${CMAKE_SOURCE_DIR}/nln/do_grav_sse4.c
		${CMAKE_SOURCE_DIR}/nln/do_grav_avx8.c
		${CMAKE_SOURCE_DIR}/nln/pgrav_vec.c
		${CMAKE_SOURCE_DIR}/nln/ewald.c
		${CMAKE_SOURCE_DIR}/nln/grav_n2.c
		${CMAKE_BINARY_DIR}/nln/version.c
	)

	target_link_libraries(nlna
		libsw libtree libSDF libmpmy
		${LIBUNWIND_LIBRARIES} ${OPENSSL_LIBRARIES} ${HOTCLASS_LIBRARIES}
		-ldl -lm -lrt
	)
ENDIF()

# sph+nln driver
IF(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/sph+nln)
	message(STATUS "sph+nln driver enabled")

	# sph+nln driver
	add_executable(sph+nln
		${CMAKE_SOURCE_DIR}/sph+nln/cofm.c ${CMAKE_SOURCE_DIR}/sph+nln/eos.c
		${CMAKE_SOURCE_DIR}/sph+nln/grav.c ${CMAKE_SOURCE_DIR}/sph+nln/mac.c
		${CMAKE_SOURCE_DIR}/sph+nln/main.c
		${CMAKE_SOURCE_DIR}/sph+nln/physics.c
		${CMAKE_SOURCE_DIR}/sph+nln/print.c
		${CMAKE_SOURCE_DIR}/sph+nln/integrate.c
		${CMAKE_SOURCE_DIR}/sph+nln/sphcofm.c ${CMAKE_SOURCE_DIR}/sph+nln/sph.c
		${CMAKE_SOURCE_DIR}/sph+nln/sphinit.c
		${CMAKE_SOURCE_DIR}/sph+nln/shrink.c
		${CMAKE_SOURCE_DIR}/sph+nln/physics_sph.c
		${CMAKE_SOURCE_DIR}/sph+nln/sphprint.c
		${CMAKE_SOURCE_DIR}/sph+nln/sphplus.c ${CMAKE_SOURCE_DIR}/sph+nln/wind.c
	)

	target_link_libraries(sph+nln
		libsw libtree libSDF libmpmy
		${LIBUNWIND_LIBRARIES} ${OPENSSL_LIBRARIES} ${HOTCLASS_LIBRARIES}
		-ldl -lm -lrt
	)
ENDIF()

# snsph driver
IF(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/snsph)
	message(STATUS "snsph driver enabled")

	# TODO: What does -mp1 do and should it be an FFLAG?

	# sph+nln driver
	add_executable(snsph
		${CMAKE_SOURCE_DIR}/snsph/cofm.c ${CMAKE_SOURCE_DIR}/snsph/grav.c
		${CMAKE_SOURCE_DIR}/snsph/mac.c ${CMAKE_SOURCE_DIR}/snsph/main.c
		${CMAKE_SOURCE_DIR}/snsph/physics.c
		${CMAKE_SOURCE_DIR}/snsph/print.c ${CMAKE_SOURCE_DIR}/snsph/integrate.c
		${CMAKE_SOURCE_DIR}/snsph/sphcofm.c
		${CMAKE_SOURCE_DIR}/snsph/sph.c ${CMAKE_SOURCE_DIR}/snsph/sphinit.c
		${CMAKE_SOURCE_DIR}/snsph/physics_sph.c
		${CMAKE_SOURCE_DIR}/snsph/shrink.c
		${CMAKE_SOURCE_DIR}/snsph/sphprint.c
		${CMAKE_SOURCE_DIR}/snsph/sphplus.c ${CMAKE_SOURCE_DIR}/snsph/sn.c
		${CMAKE_SOURCE_DIR}/snsph/eos3.f ${CMAKE_SOURCE_DIR}/snsph/sleos.f
		${CMAKE_SOURCE_DIR}/snsph/nse4d.f ${CMAKE_SOURCE_DIR}/snsph/ocean.f
		${CMAKE_SOURCE_DIR}/snsph/eosgen.f
	)

	target_link_libraries(snsph
		libsw libtree libSDF libmpmy
		${LIBUNWIND_LIBRARIES} ${OPENSSL_LIBRARIES} ${HOTCLASS_LIBRARIES}
		-ldl -lm -lrt
	)
ENDIF()
