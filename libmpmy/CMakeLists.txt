cmake_minimum_required(VERSION 2.6)

# Determine which version to build, based on PAROS
if (${CONFIG_PAROS} STREQUAL "mpi")
    include_directories(${MPI_C_INCLUDE_PATH})
    file(GLOB LIBMPMY_SOURCES mpmy_mpi.c)
elseif(${CONFIG_PAROS} STREQUAL "seq")
    file(GLOB LIBMPMY_SOURCES mpmy_seq.c)
endif ()

# Compile: Does not seem that baseline used io*.c or io*.h
add_library(libmpmy
     ${LIBMPMY_SOURCES}
)

if(${CONFIG_PAROS} STREQUAL "mpi")
    target_link_libraries(libmpmy
        ${MPI_C_LIBRARIES}
    )
endif()
