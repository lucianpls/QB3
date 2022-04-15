cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0076 NEW)

project(QB3 
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CSS_STANDARD_REQUIRED ON)

find_package(libicd CONFIG REQUIRED)

add_subdirectory(QB3lib)
add_executable(test_qb3 test_qb3.cpp)
target_link_libraries(test_qb3 PRIVATE AHTSE::libicd libQB3)
