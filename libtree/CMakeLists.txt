cmake_minimum_required(VERSION 2.6)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
    COMMAND "source" "${CMAKE_CURRENT_SOURCE_DIR}/genVersion.sh" "${CONFIG_ARCH}" "${CMAKE_CURRENT_BINARY_DIR}/version.c"
    COMMENT "Generating version.c ..."
)

add_library(libtree
    tree.c pqsort.c traverse.c walk.c decomp.c image.c
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
)
