cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0076 NEW)

project(libQB3 
    DESCRIPTION "QB3 Raster Compression Library"
    VERSION 0.9.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_SHARED_LIBS "Library style" ON)

set(namespace "QB3")

add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME} 
    PRIVATE QB3encode.cpp QB3encode.h QB3decode.cpp QB3decode.h QB3tables.cpp QB3tables.h QB3common.h bitstream.h QB3.h
)
set_target_properties(${PROJECT_NAME} PROPERTIES 
    PUBLIC_HEADER QB3.h
    DEBUG_POSTFIX "d"
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