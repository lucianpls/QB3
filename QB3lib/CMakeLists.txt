cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0076 NEW)

# Has to be before project()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose type of build, options are: Debug Release RelWithDebInfo MinSizeRel")
endif(NOT CMAKE_BUILD_TYPE)

project(libQB3 
    DESCRIPTION "QB3 Raster Compression Library"
    VERSION 1.3.2
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_SHARED_LIBS "Library style" ON)

set(namespace "QB3")
add_library(${PROJECT_NAME})

include(CheckCXXCompilerFlag)
if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" AND ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    check_cxx_compiler_flag("-mavx2" HAVE_AVX2)
    if (HAVE_AVX2)
        option(ENABLE_AVX2 "Enable AVX2 optimizations" ON)
        if (ENABLE_AVX2)
            target_compile_options(${PROJECT_NAME} PRIVATE "-mavx2")
        endif()
    else()
        check_cxx_compiler_flag("-msse4" HAVE_MSSE4)
        if (HAVE_MSSE4)
            option(ENABLE_SSE4 "Enable SSE4 optimizations" ON)
            if (ENABLE_SSE4)
                target_compile_options(${PROJECT_NAME} PRIVATE "-msse4")
            endif()
        endif()
    endif()
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "AMD64" AND MSVC)
    option(ENABLE_AVX2 "Enable AVX2 optimizations" ON)
    if (ENABLE_AVX2)
        target_compile_options(${PROJECT_NAME} PRIVATE "/arch:AVX2")
    endif()
endif()

target_sources(${PROJECT_NAME} 
    PRIVATE QB3encode.cpp QB3encode.h QB3decode.cpp QB3decode.h QB3common.h bitstream.h QB3.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER QB3.h
    DEBUG_POSTFIX "d"
    PREFIX ""
)

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME})
target_include_directories(${PROJECT_NAME}  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ${PROJECT_NAME}
    EXPORT "${PROJECT_NAME}Targets"
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT "${PROJECT_NAME}Targets"
    FILE "${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${namespace}::
    DESTINATION cmake
)

# generate the version file for the config file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION "${version}"
    COMPATIBILITY AnyNewerVersion
)
# create config file
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION cmake
)
# install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION cmake
)
# generate the export targets for the build tree
export(EXPORT "${PROJECT_NAME}Targets"
    FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${namespace}::
)
