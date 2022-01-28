cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0076 NEW)

project(QB3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CSS_STANDARD_REQUIRED ON)

add_library(QB3lib SHARED)
target_sources(QB3lib PRIVATE QB3encode.cpp QB3encode.h QB3decode.cpp QB3decode.h QB3tables.cpp QB3tables.h QB3common.h)
target_sources(QB3lib PUBLIC QB3.h)
set_target_properties(QB3lib PROPERTIES PUBLIC_HEADER QB3.h)
install(TARGETS QB3lib
	PUBLIC_HEADER
)
