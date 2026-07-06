cmake_minimum_required(VERSION 3.20)

# RunInstalledPackageConsumerTest.cmake
#
# This script is invoked by CTest. It validates that the already-built
# D3DInterop project can be installed, then consumed from an independent
# external CMake project through find_package(D3DInterop CONFIG REQUIRED).

function(d3dinterop_strip_outer_quotes out_var in_value)
    set(_v "${in_value}")
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" _v "${_v}")
    set(${out_var} "${_v}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED D3DINTEROP_SOURCE_DIR)
    message(FATAL_ERROR "D3DINTEROP_SOURCE_DIR is not defined")
endif()
if(NOT DEFINED D3DINTEROP_MAIN_BINARY_DIR)
    message(FATAL_ERROR "D3DINTEROP_MAIN_BINARY_DIR is not defined")
endif()
if(NOT DEFINED D3DINTEROP_CONSUMER_SOURCE_DIR)
    message(FATAL_ERROR "D3DINTEROP_CONSUMER_SOURCE_DIR is not defined")
endif()
if(NOT DEFINED TEST_CONFIGURATION OR TEST_CONFIGURATION STREQUAL "")
    set(TEST_CONFIGURATION "Debug")
endif()

# Be defensive against old add_test() definitions that accidentally passed
# quoted strings as literal argument values, e.g. -DVAR="C:/path".
d3dinterop_strip_outer_quotes(D3DINTEROP_SOURCE_DIR "${D3DINTEROP_SOURCE_DIR}")
d3dinterop_strip_outer_quotes(D3DINTEROP_MAIN_BINARY_DIR "${D3DINTEROP_MAIN_BINARY_DIR}")
d3dinterop_strip_outer_quotes(D3DINTEROP_CONSUMER_SOURCE_DIR "${D3DINTEROP_CONSUMER_SOURCE_DIR}")
d3dinterop_strip_outer_quotes(TEST_CONFIGURATION "${TEST_CONFIGURATION}")

set(_work_root "${D3DINTEROP_MAIN_BINARY_DIR}/_installed_package_consumer_test")
set(_install_prefix "${_work_root}/install")
set(_consumer_build "${_work_root}/consumer_build")
set(_d3dinterop_dir "${_install_prefix}/lib/cmake/D3DInterop")

file(REMOVE_RECURSE "${_work_root}")
file(MAKE_DIRECTORY "${_work_root}")

message(STATUS "D3DInterop package consumer test: installing current build")
message(STATUS "  source         = ${D3DINTEROP_SOURCE_DIR}")
message(STATUS "  main build     = ${D3DINTEROP_MAIN_BINARY_DIR}")
message(STATUS "  install prefix = ${_install_prefix}")
message(STATUS "  package dir    = ${_d3dinterop_dir}")
message(STATUS "  config         = ${TEST_CONFIGURATION}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            --install "${D3DINTEROP_MAIN_BINARY_DIR}"
            --config "${TEST_CONFIGURATION}"
            --prefix "${_install_prefix}"
    RESULT_VARIABLE _install_result
)
if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR "cmake --install failed with exit code ${_install_result}")
endif()

if(NOT EXISTS "${_d3dinterop_dir}/D3DInteropConfig.cmake")
    message(FATAL_ERROR
        "D3DInteropConfig.cmake was not installed at expected path: ${_d3dinterop_dir}")
endif()

message(STATUS "D3DInterop package consumer test: configuring external consumer")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${D3DINTEROP_CONSUMER_SOURCE_DIR}"
            -B "${_consumer_build}"
            "-DD3DInterop_DIR=${_d3dinterop_dir}"
            "-DCMAKE_PREFIX_PATH=${_install_prefix}"
            "-DCMAKE_BUILD_TYPE=${TEST_CONFIGURATION}"
            "-DD3DINTEROP_PACKAGE_FETCH_HELPERS=ON"
    RESULT_VARIABLE _configure_result
)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR "external package consumer configure failed with exit code ${_configure_result}")
endif()

message(STATUS "D3DInterop package consumer test: building external consumer")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
            --build "${_consumer_build}"
            --config "${TEST_CONFIGURATION}"
    RESULT_VARIABLE _build_result
)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR "external package consumer build failed with exit code ${_build_result}")
endif()

message(STATUS "D3DInterop package consumer test: running external consumer")
execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}"
            --test-dir "${_consumer_build}"
            -C "${TEST_CONFIGURATION}"
            --output-on-failure
    RESULT_VARIABLE _test_result
)
if(NOT _test_result EQUAL 0)
    message(FATAL_ERROR "external package consumer test failed with exit code ${_test_result}")
endif()

message(STATUS "D3DInterop package consumer test passed")
