cmake_minimum_required(VERSION 3.0.0)
project(smvp-csr VERSION 0.3.0)

include(CTest)
enable_testing()

include_directories(${PROJECT_SOURCE_DIR})
add_executable(smvp-toolkit main.c mmio.c)
add_executable(mmio-readtest mmio-readtest.c mmio.c)
add_executable(mmio-writetest mmio-writetest.c mmio.c)

target_compile_options(smvp-toolkit PUBLIC -Wall -Wextra -Wpedantic -Werror -Wconversion)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
