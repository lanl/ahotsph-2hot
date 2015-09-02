cmake_minimum_required(VERSION 2.6)

project(2hot)

# Cmake modules/macros are in a subdirectory to keep this file cleaner
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMakeModules)

# TODO: Add fortran flags
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS)
  #release comes with -O3 by default
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
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

# include(CheckFortranCompilerFlag)
# TODO: Check each flag we use

########################################################################
# User input options                                                   #
########################################################################
# TODO: Add params for each flag we want to set at cmake time
# COMMENT: Do we even care? It sounds like all inputs were machine specific (CMake will find libraries) or arch specific (Compiler Tests?)

########################################################################
# General and Machine-Specific Definitions                             #
########################################################################
add_definitions(-D_XOPEN_SOURCE=500)
add_definitions(-D_GNU_SOURCE)
# TODO: Are the rest optimizations or required?


########################################################################
#Find external packages/libraries
########################################################################
# Find libssl
# TODO: Do other SSL implementations do libssl?
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIRS})

# Find libunwind
find_package(Libunwind REQUIRED)
include_directories(${LIBUNWIND_INCLUDE_DIRS})

# Find MPI: Only required if PAROS=mpi
find_package(MPI)

# Find libdl
# TODO: Seems like a system library, so same as libm (sans MKL)?

# HOT_CLASS Stuff
find_package(Hotclass REQUIRED)
include_directories(${HOTCLASS_INCLUDE_DIRS})

########################################################################
# Basic system tests (standard libraries, headers, functions, types)   #
########################################################################
# Find libm
set(MATH_LIBRARIES "m" CACHE STRING "math library")
mark_as_advanced( MATH_LIBRARIES )
include(CheckLibraryExists)
# TODO: Add funcs we need
foreach(FUNC sqrt)
  check_library_exists(${MATH_LIBRARIES} ${FUNC} "" FOUND_${FUNC}_${MATH_LIBRARIES})
  if(NOT FOUND_${FUNC}_${MATH_LIBRARIES})
    message(FATAL_ERROR "Could not find needed math function - ${FUNC}")
  endif(NOT FOUND_${FUNC}_${MATH_LIBRARIES})
endforeach(FUNC)

######################################
# Actual Targets                     #
######################################
