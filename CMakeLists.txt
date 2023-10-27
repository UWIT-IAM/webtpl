# WebTPL: Web Templating Library for C
#
# Deprecated, please use CTL

# Build flags
set(VERSION "1.6" CACHE STRING "Version of the library")

# Silence errors
#
# TODO: Remove as many of these as possible after we get this building.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-function-declaration")
endif()

add_library(webtpl
    webtpl.c
)
target_compile_definitions(webtpl PRIVATE VERSION="${VERSION}")
