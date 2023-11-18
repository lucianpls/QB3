cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0076 NEW)

# Has to be before project()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose type of build, options are: Debug Release RelWithDebInfo MinSizeRel")
endif(NOT CMAKE_BUILD_TYPE)


project(QB3
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CSS_STANDARD_REQUIRED ON)

add_subdirectory(QB3lib)

option(BUILD_CQB3 "Build QB3 image conversion utility" ON)
option(QB3_DEV "QB3 development utility, internal use only" OFF)

# The executables need libicd to read and write other formats
# From https://github.com/lucianpls/libicd

if (${BUILD_CQB3})
add_executable(cqb3 cqb3.cpp)
find_package(libicd CONFIG REQUIRED)
if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif (MSVC)
target_link_libraries(cqb3 PRIVATE AHTSE::libicd libQB3)
endif()

if (${QB3_DEV})
add_executable(test_qb3 test_qb3.cpp)
find_package(libicd CONFIG REQUIRED)

# On AMD64 uncomment to use avx2 -> faster code
# target_compile_options(cqb3 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>)
# target_compile_options(cqb3 PRIVATE $<$<CXX_COMPILER_ID:GNU>:-mavx2>)

target_link_libraries(test_qb3 PRIVATE AHTSE::libicd libQB3)
endif()
