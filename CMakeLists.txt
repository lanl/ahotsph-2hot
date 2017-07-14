cmake_minimum_required(VERSION 2.6)

project(2hot)

# Cmake modules/macros are in a subdirectory to keep this file cleaner
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMakeModules)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In source build is not supported, do 'git clean` and build out of source")
endif()

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

# Find SSE4
include(FindSSE)
FindSSE()

if(SSE4_A_FOUND)
	message(STATUS "SSE4A Detected")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4a")
elseif(SSE4_2_FOUND)
	message(STATUS "SSE4.2 Detected")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2")
elseif(SSE4_1_FOUND)
	message(STATUS "SSE4.1 Detected")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1")
elseif(SSSE3_FOUND)
	message(STATUS "SSE3 Detected")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse3")
else()
 	message(FATAL_ERROR "No SSE3 or SSE4 Detected")
endif()


# include(CheckFortranCompilerFlag)
# TODO: Check each flag we use

########################################################################
# User input options                                                   #
########################################################################
SET(PROCS_PER_NODE "16" CACHE STRING "Number of Processors per Node (Integer)")

# TODO: Figure out if we care about ARCH outside of version.c
SET(CONFIG_ARCH "cmake" CACHE STRING "String Representing Architecture/Target Machine")

SET(CONFIG_PAROS "mpi" CACHE STRING "Parallelization Scheme")

SET(CONFIG_MPIIO true CACHE BOOLEAN "Use MPIIO")

SET(CONFIG_USEHWCLK true CACHE BOOLEAN "Use Hardware Clock")

SET(CONFIG_USEGETTIME false CACHE BOOLEAN "Use gettime()")


# TODO: Add params for each flag we want to set at cmake time
# COMMENT: Do we even care? It sounds like all inputs were machine specific (CMake will find libraries) or arch specific (Compiler Tests?)

########################################################################
# General and Machine-Specific Definitions                             #
########################################################################
# Use gnu99 standard, not c99, for M_PI.
add_definitions(-D_XOPEN_SOURCE=500)
add_definitions(-D_GNU_SOURCE)
# Specify number of processors per node for libraries
add_definitions(-DPROCS_PER_NODE=${PROCS_PER_NODE})

# If OSX, we need an extra define
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	add_definitions(-D_DARWIN_C_SOURCE)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Set mpmy Variables
if(CONFIG_USEHWCLK)
	message(STATUS "Hardware Clock Enabled")
	add_definitions(-DUSE_HWCLOCK)
elseif(CONFIG_USEGETTIME)
	message(STATUS "gettime() based Clock Enabled")
	# message(FATAL_ERROR "./libmpmy/timers_gettime.c incomplete, Build will Fail")
	add_definitions(-DUSE_GETTIME)
endif()

if (${CONFIG_PAROS} STREQUAL "mpi")
	if(CONFIG_MPIIO)
		message(STATUS "MPI I/O Enabled")
		add_definitions(-DUSE_MPIIO)
	endif(CONFIG_MPIIO)

	if(NOT CONFIG_USEHWCLK AND NOT CONFIG_USEGETTIME)
		message(STATUS "MPMY Timers Enabled")
		# message(FATAL_ERROR "./libmpmy/timers_mpi.c incomplete, Build will Fail")
		add_definitions(-DHAVE_MPMY_TIMERS)
	endif()
endif(${CONFIG_PAROS} STREQUAL "mpi")

# TODO: Are the rest optimizations or required?

########################################################################
# Find external packages/libraries/binaries                            #
########################################################################
# Find libssl
# TODO: Do other SSL implementations do libssl?
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIRS})

# Find libunwind if not OSX
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_package(LIBUNWIND REQUIRED)
	include_directories(${LIBUNWIND_INCLUDE_DIRS})
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Find MPI: Only required if PAROS=mpi
if(${CONFIG_PAROS} STREQUAL "mpi")
	find_package(MPI REQUIRED)
endif()

# HOT_CLASS Stuff
find_package(HOTCLASS)
if(NOT HOTCLASS_FOUND)
	message(STATUS "Downloading Cosmic Linear Anisotropy Solving System")
	include(ExternalProject)
	if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		set(PATCH_COMMAND patch ${CMAKE_CURRENT_BINARY_DIR}/HOTCLASS_BUILD-prefix/src/HOTCLASS_BUILD/Makefile < ${CMAKE_SOURCE_DIR}/patches/hotclass_osxNoOMP.patch)
	endif()
	ExternalProject_Add(HOTCLASS_BUILD URL https://github.com/lesgourg/class_public/archive/v2.2.0.tar.gz
		URL_MD5 965a76dfa29bd0072698f8c7aef8991c
		BUILD_IN_SOURCE 1 CONFIGURE_COMMAND "" INSTALL_COMMAND ""
		PATCH_COMMAND ${PATCH_COMMAND})
	add_library(hotclass STATIC IMPORTED GLOBAL)
	set_target_properties(hotclass PROPERTIES IMPORTED_LOCATION
		${CMAKE_CURRENT_BINARY_DIR}/HOTCLASS_BUILD-prefix/src/HOTCLASS_BUILD/libclass.a)
	add_dependencies(hotclass HOTCLASS_BUILD)
	set(HOTCLASS_LIBRARIES hotclass)
	set(HOTCLASS_INCLUDE_DIRS
		${CMAKE_CURRENT_BINARY_DIR}/HOTCLASS_BUILD-prefix/src/HOTCLASS_BUILD/include)
endif()

# Flex
find_package(FLEX REQUIRED)

# Bison
find_package(BISON REQUIRED)

#Get obstack.h if OSX
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	message(STATUS "Downloading obstack.h")
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/obstack")
	file(DOWNLOAD "https://raw.githubusercontent.com/lattera/glibc/master/malloc/obstack.h" "${CMAKE_CURRENT_BINARY_DIR}/obstack/obstack.h")
	include_directories(${CMAKE_CURRENT_BINARY_DIR}/obstack)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

#Set libraries for OSX or Not OSX
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	SET(OSSPECIFIC_LIBRARIES ${LIBUNWIND_LIBRARIES} -lrt)
else()
	SET(OSSPECIFIC_LIBRARIES )
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")


########################################################################
# Basic system tests (standard libraries, headers, functions, types)   #
########################################################################
include(CheckLibraryExists)
# Find libm
set(MATH_LIBRARIES "m" CACHE STRING "math library")
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
# First add include directory as everyone will need it
include_directories(include)

# libsw
add_subdirectory(libsw)

# libSDF
add_subdirectory(libSDF)

# libtree
add_subdirectory(libtree)

# libmpmy
add_subdirectory(libmpmy)

# nln driver: Does not compile with Intel Compiler as is... or Debug...
IF(NOT ${CMAKE_Fortran_COMPILER_ID} STREQUAL "Intel")
	IF(NOT ${CMAKE_BUILD_TYPE} MATCHES "DEBUG")
		IF(EXISTS ${CMAKE_SOURCE_DIR}/nln/main.c)
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
				${OPENSSL_LIBRARIES} ${HOTCLASS_LIBRARIES}
				-ldl -lm ${OSSPECIFIC_LIBRARIES}
			)
		ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/nln/main.c)
	ENDIF(NOT ${CMAKE_BUILD_TYPE} MATCHES "DEBUG")
ENDIF(NOT ${CMAKE_Fortran_COMPILER_ID} STREQUAL "Intel")

# sph+nln driver
IF(EXISTS ${CMAKE_SOURCE_DIR}/sph+nln/main.c)
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
		${CMAKE_SOURCE_DIR}/sph+nln/nrutil.c
		${CMAKE_SOURCE_DIR}/sph+nln/coolandburn.c
        ${CMAKE_SOURCE_DIR}/sph+nln/params.c
		${CMAKE_SOURCE_DIR}/sph+nln/find.f ${CMAKE_SOURCE_DIR}/sph+nln/getfkt.f
		${CMAKE_SOURCE_DIR}/sph+nln/abinit.f
		${CMAKE_SOURCE_DIR}/sph+nln/masses.f
		${CMAKE_SOURCE_DIR}/sph+nln/sparse_ma28.f
		${CMAKE_SOURCE_DIR}/sph+nln/naray.f
		${CMAKE_SOURCE_DIR}/sph+nln/weakread.f
		${CMAKE_SOURCE_DIR}/sph+nln/build.f
		${CMAKE_SOURCE_DIR}/sph+nln/locate.f
		${CMAKE_SOURCE_DIR}/sph+nln/partfun.f
		${CMAKE_SOURCE_DIR}/sph+nln/bilenshort.f
		${CMAKE_SOURCE_DIR}/sph+nln/weakintp.f
		${CMAKE_SOURCE_DIR}/sph+nln/rate.f
		${CMAKE_SOURCE_DIR}/sph+nln/rhside.f
		${CMAKE_SOURCE_DIR}/sph+nln/jacob.f
		${CMAKE_SOURCE_DIR}/sph+nln/dtnuc.f
		${CMAKE_SOURCE_DIR}/sph+nln/nse.f
		${CMAKE_SOURCE_DIR}/sph+nln/leqs.f
		${CMAKE_SOURCE_DIR}/sph+nln/rated.f
		${CMAKE_SOURCE_DIR}/sph+nln/solven.f
	)

	IF(${CMAKE_Fortran_COMPILER_ID} STREQUAL "Intel")
	    set_target_properties(sph+nln PROPERTIES LINK_FLAGS -nofor-main)
    ENDIF(${CMAKE_Fortran_COMPILER_ID} STREQUAL "Intel")

	target_link_libraries(sph+nln
		libsw libtree libSDF libmpmy
		${OPENSSL_LIBRARIES} ${HOTCLASS_LIBRARIES}
		-ldl -lm ${OSSPECIFIC_LIBRARIES}
	)
ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/sph+nln/main.c)

# snsph driver - Doesn't build with Debug...
IF(NOT ${CMAKE_BUILD_TYPE} MATCHES "DEBUG")
	IF(EXISTS ${CMAKE_SOURCE_DIR}/snsph/main.c)
		message(STATUS "snsph driver enabled")

		# TODO: What does -mp1 do and should it be an FFLAG?

		# snsph driver
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

		IF(${CMAKE_Fortran_COMPILER_ID} STREQUAL "Intel")
			set_target_properties(snsph PROPERTIES LINK_FLAGS -nofor-main)
		ENDIF(${CMAKE_Fortran_COMPILER_ID} STREQUAL "Intel")

		target_link_libraries(snsph
			libsw libtree libSDF libmpmy
			${OPENSSL_LIBRARIES} ${HOTCLASS_LIBRARIES}
			-ldl -lm ${OSSPECIFIC_LIBRARIES}
		)
	ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/snsph/main.c)
ENDIF(NOT ${CMAKE_BUILD_TYPE} MATCHES "DEBUG")

# interp_cylgrid driver/utility
IF(EXISTS ${CMAKE_SOURCE_DIR}/wvt/wvt.c)
	message(STATUS "interp_cylgrid driver/utility enabled")

	# interp_cylgrid driver
	add_executable(interp_cylgrid
		${CMAKE_SOURCE_DIR}/wvt/
		${CMAKE_SOURCE_DIR}/wvt/grav.c ${CMAKE_SOURCE_DIR}/wvt/interp_cylgrid.c
		${CMAKE_SOURCE_DIR}/wvt/mac.c ${CMAKE_SOURCE_DIR}/wvt/physics.c
		${CMAKE_SOURCE_DIR}/wvt/physics_sph.c
		${CMAKE_SOURCE_DIR}/wvt/sphcofm.c ${CMAKE_SOURCE_DIR}/wvt/sphinit.c
		${CMAKE_SOURCE_DIR}/wvt/sph.c ${CMAKE_SOURCE_DIR}/wvt/sphprint.c
		${CMAKE_SOURCE_DIR}/wvt/sphplus.c
		${CMAKE_SOURCE_DIR}/wvt/ghosts.c ${CMAKE_SOURCE_DIR}/wvt/com.c
		${CMAKE_SOURCE_DIR}/wvt/ranlib.c ${CMAKE_SOURCE_DIR}/wvt/wvt.c
		${CMAKE_SOURCE_DIR}/wvt/polint.c ${CMAKE_SOURCE_DIR}/wvt/initial.c
	)

	target_link_libraries(interp_cylgrid
		libsw libmpmy libtree libSDF
		${OPENSSL_LIBRARIES} ${HOTCLASS_LIBRARIES}
		-ldl -lm ${OSSPECIFIC_LIBRARIES}
	)
ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/wvt/wvt.c)
