cmake_minimum_required(VERSION 3.0)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/genTreeVersion.sh" "${CONFIG_ARCH}" "${CMAKE_CURRENT_BINARY_DIR}/version.c"
    COMMENT "Generating version.c for libtree..."
)

add_library(libtree
    tree.c pqsort.c traverse.c walk.c decomp.c image.c
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
)

target_link_libraries(libtree
    libsw
)
