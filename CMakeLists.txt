# This file creates project 'AsioAmqp' with one library targets 'aiso_amqp'.
# After installation this project can be found
# by 'find_package(... CONFIG)' command:
#
#    find_package(AsioAmqp CONFIG REQUIRED)
#    target_link_libraries(... asio_amqp)
#
# Note that requirements propagated automatically, for example:
#   * <prefix>/include added to header search path
#   * SECRWEBSERVER_DEBUG=1 added on Debug

####
# Set minimum version of CMake. Since command 'project' use
# VERSION sub-option we need 3.2.2
cmake_minimum_required(VERSION 3.3.2)

include(${CMAKE_SOURCE_DIR}/cmake/add_sources.cmake)

####
# Set variables:
#   * PROJECT_NAME
#   * PROJECT_VERSION
project(AsioAmqp VERSION 1.1.0)

####
# Get hunter to fetch and build dependencies and 
# third party libraries if required
sanity_require(LIBRARY openssl VERSION any)
sanity_require(LIBRARY boost VERSION any COMPONENTS program_options)
sanity_require(LIBRARY gtest VERSION any)



####
# Find all packages required for this project.
# hunter will ensure they have been downloaded and built

interface_tree(include)
source_tree(src)
test_tree(tests)

####
# Create targets
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/src")
add_library(asio_amqp ${INTERFACE_FILES} ${SOURCE_FILES})

target_link_libraries(asio_amqp PUBLIC ${CMAKE_THREAD_LIBS_INIT} 
    amqpcpp
    ValueLib_debug
    ValueLib_data
    ValueLib_stdext
    sanity::openssl
    boost::program_options
)

####
# Create tests
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/tests")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/tests")
include_directories(${GTEST_INCLUDE_DIRS})
add_executable(asio_amqp_tests ${TEST_FILES})
target_link_libraries(asio_amqp_tests asio_amqp sanity::gtest::main)

add_test(AllTestsInAsioAmqp asio_amqp_tests)

####
# Properties of targets

# Add definitions for targets
# Values:
#   * Debug: -DSECRWEBSERVER_DEBUG=1
#   * Release: -DSECRWEBSERVER_DEBUG=0
#   * other: -DSECRWEBSERVER_DEBUG=0
target_compile_definitions(asio_amqp PUBLIC "ASIO_AMQP_DEBUG=$<CONFIG:Debug>")

# Generate:
#   * ${CMAKE_CURRENT_BINARY_DIR}/secrwebserver_export.h with SECRWEBSERVER_EXPORT
include(GenerateExportHeader)
generate_export_header(asio_amqp)

# Global includes. Used by all targets
# Note:
#   * header location in project: secrwebserver/include/secrwebserver/server.h
#   * header can be included by C++ code `#include <secrwebserver/server.h>`
#   * header location in project: ${CMAKE_CURRENT_BINARY_DIR}/secrwebserver_export.h
#   * header can be included by: `#include <secrwebserver_export.h>`
target_include_directories(
    asio_amqp PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
#    PRIVATE SYSTEM ${Boost_INCLUDE_DIRS}
)

####
# Installation

# Layout. This works for all platforms:
#   * <prefix>/lib/cmake/<PROJECT-NAME>
#   * <prefix>/lib/
#   * <prefix>/include/
set(config_install_dir "lib/cmake/${PROJECT_NAME}")
set(include_install_dir "include")

set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Configuration
set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(targets_export_name "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

# Include module with function 'write_basic_package_version_file'
include(CMakePackageConfigHelpers)

# Configure '<PROJECT-NAME>ConfigVersion.cmake'
# Note: PROJECT_VERSION is used as a VERSION
write_basic_package_version_file(
    "${version_config}" COMPATIBILITY SameMajorVersion
)

# Configure '<PROJECT-NAME>Config.cmake'
# Use variables:
#   * targets_export_name
#   * PROJECT_NAME
configure_package_config_file(
    "cmake/Config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

# Targets:
#   * <prefix>/lib/libsecrwebserver.a
#   * header location after install: <prefix>/include/secrwebserver/server.h
#   * headers can be included by C++ code `#include <secrwebserver/server.h>`
install(
    TARGETS asio_amqp
    EXPORT "${targets_export_name}"
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
    RUNTIME DESTINATION "bin"
    INCLUDES DESTINATION "${include_install_dir}"
)


# Headers:
#   * include/secrwebserver/server.h -> <prefix>/include/secrwebserver/server.h
install(
    DIRECTORY "include/asio_amqp"
    DESTINATION "${include_install_dir}"
    FILES_MATCHING PATTERN "*.h"
)

# Export headers:
#   * ${CMAKE_CURRENT_BINARY_DIR}/secrwebserver_export.h -> <prefix>/include/secrwebserver_export.h
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/asio_amqp_export.h"
    DESTINATION "${include_install_dir}"
)

# Config
#   * <prefix>/lib/cmake/secrwebserver/SECRWebServerConfig.cmake
#   * <prefix>/lib/cmake/secrwebserver/SECRWebServerConfigVersion.cmake
install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

# Config
#   * <prefix>/lib/cmake/asio_amqp/SECRWebServerTargets.cmake
install(
    EXPORT "${targets_export_name}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)

