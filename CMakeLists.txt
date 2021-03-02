cmake_minimum_required(VERSION 3.0.0)
project(smvp-csr VERSION 0.3.0)

include(CTest)
enable_testing()

# Use the package PkgConfig to detect GTK+ headers/library files
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

# Setup CMake to use GTK+, tell the compiler where to look for headers
# and to the linker where to look for libraries
INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})

# Add other flags to the compiler
ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

include_directories(${PROJECT_SOURCE_DIR})
add_executable(smvp-toolkit-gui main-gui.c)
add_executable(smvp-toolkit-cli main-cli.c mmio.c)
add_executable(mmio-readtest mmio-readtest.c mmio.c)
add_executable(mmio-writetest mmio-writetest.c mmio.c)

# Link the target to the GTK+ libraries
TARGET_LINK_LIBRARIES(smvp-toolkit-gui ${GTK3_LIBRARIES})

target_compile_options(smvp-toolkit-gui PUBLIC -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_options(smvp-toolkit-cli PUBLIC -Wall -Wextra -Wpedantic -Werror -Wconversion)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
