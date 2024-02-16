cmake_minimum_required(VERSION 3.0)

#Add self as include directory for later
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#Run (F)lex and (Bison)yacc
FLEX_TARGET(SDFlex SDF-lex.l  ${CMAKE_CURRENT_BINARY_DIR}/SDF-lex.c)
BISON_TARGET(SDFparse SDF-parse.y ${CMAKE_CURRENT_BINARY_DIR}/SDF-parse.c)
set(BISON_SDFparse_COMPILE_FLAGS "${BISON_SDFparse_COMPILE_FLAGS} -y")
ADD_FLEX_BISON_DEPENDENCY(SDFlex SDFparse)

# COMMENT: Baseline added these to libsw. We make a separate library
# TODO: Verify that the lexer's output is completely ignored
add_library(libSDF
    SDF-parse.c 	SDFfuncs.c	SDFget.c	SDFhdrio.c
    # SDF-lex.c
    # ${FLEX_SDFlex_OUTPUTS}
    ${BISON_SDFparse_OUTPUTS}
)
