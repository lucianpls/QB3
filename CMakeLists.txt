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

find_package(libicd CONFIG REQUIRED)
add_subdirectory(QB3lib)
add_executable(cqb3 cqb3.cpp)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif (MSVC)

# On AMD64 uncomment to use avx2 -> faster code
# target_compile_options(cqb3 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>)
# target_compile_options(cqb3 PRIVATE $<$<CXX_COMPILER_ID:GNU>:-mavx2>)

target_link_libraries(cqb3 PRIVATE AHTSE::libicd libQB3)
