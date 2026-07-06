cmake_minimum_required(VERSION 3.20)

project(D3DInterop
    VERSION 0.1.0
    LANGUAGES CXX
)

option(D3DINTEROP_BUILD_TESTS "Build D3DInterop tests" ON)
option(D3DINTEROP_BUILD_SAMPLES "Build D3DInterop samples" ON)
option(D3DINTEROP_ENABLE_WARNINGS "Enable compiler warnings for D3DInterop" ON)

# Dependency policy:
#   1. If D3D11Helper::D3D11Helper / D3D12Helper::D3D12Helper already exist, use them.
#   2. Else, if D3DINTEROP_D3D*HELPER_ROOT is set, add_subdirectory() that local checkout.
#   3. Else, if D3DINTEROP_FETCH_HELPERS is ON, fetch from GitHub by FetchContent.
option(D3DINTEROP_FETCH_HELPERS "Fetch D3D11Helper/D3D12Helper from GitHub when local targets are missing" ON)
set(D3DINTEROP_D3D11HELPER_ROOT "" CACHE PATH "Optional local path to D3D11Helper root")
set(D3DINTEROP_D3D12HELPER_ROOT "" CACHE PATH "Optional local path to D3D12Helper root")
set(D3DINTEROP_D3D11HELPER_GIT_REPOSITORY "https://github.com/Isasa2357/D3D11Helper.git" CACHE STRING "D3D11Helper Git repository")
set(D3DINTEROP_D3D12HELPER_GIT_REPOSITORY "https://github.com/Isasa2357/D3D12Helper.git" CACHE STRING "D3D12Helper Git repository")
set(D3DINTEROP_D3D11HELPER_GIT_TAG "main" CACHE STRING "D3D11Helper Git tag/branch/commit")
set(D3DINTEROP_D3D12HELPER_GIT_TAG "main" CACHE STRING "D3D12Helper Git tag/branch/commit")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(WARNING "D3DInterop is a Direct3D interop library and can only be built on Windows. No targets will be generated on this platform.")
    return()
endif()

include(FetchContent)

# Keep helper dependencies lightweight when they are pulled as subprojects.
set(D3D11HELPER_BUILD_SAMPLES OFF CACHE BOOL "Build D3D11Helper samples" FORCE)
set(D3D11HELPER_BUILD_TESTS   OFF CACHE BOOL "Build D3D11Helper tests" FORCE)
set(D3D12HELPER_BUILD_SAMPLES OFF CACHE BOOL "Build D3D12Helper samples" FORCE)
set(D3D12HELPER_BUILD_TESTS   OFF CACHE BOOL "Build D3D12Helper tests" FORCE)

function(d3dinterop_resolve_helper target_name local_root git_name git_repository git_tag)
    if(TARGET ${target_name})
        message(STATUS "D3DInterop: using existing target ${target_name}")
        return()
    endif()

    if(local_root)
        message(STATUS "D3DInterop: using local ${git_name}: ${local_root}")
        add_subdirectory("${local_root}" "${CMAKE_BINARY_DIR}/_deps/${git_name}-local")
        if(NOT TARGET ${target_name})
            message(FATAL_ERROR "${target_name} was not created by local ${git_name}: ${local_root}")
        endif()
        return()
    endif()

    if(D3DINTEROP_FETCH_HELPERS)
        message(STATUS "D3DInterop: fetching ${git_name} from ${git_repository} (${git_tag})")
        FetchContent_Declare(${git_name}
            GIT_REPOSITORY "${git_repository}"
            GIT_TAG        "${git_tag}"
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(${git_name})
        if(NOT TARGET ${target_name})
            message(FATAL_ERROR "${target_name} was not created after fetching ${git_name}")
        endif()
        return()
    endif()

    message(FATAL_ERROR
        "${target_name} is missing. Either add the helper target before D3DInterop, "
        "set the corresponding D3DINTEROP_D3D*HELPER_ROOT, or enable D3DINTEROP_FETCH_HELPERS.")
endfunction()

d3dinterop_resolve_helper(
    D3D11Helper::D3D11Helper
    "${D3DINTEROP_D3D11HELPER_ROOT}"
    D3D11Helper
    "${D3DINTEROP_D3D11HELPER_GIT_REPOSITORY}"
    "${D3DINTEROP_D3D11HELPER_GIT_TAG}"
)

d3dinterop_resolve_helper(
    D3D12Helper::D3D12Helper
    "${D3DINTEROP_D3D12HELPER_ROOT}"
    D3D12Helper
    "${D3DINTEROP_D3D12HELPER_GIT_REPOSITORY}"
    "${D3DINTEROP_D3D12HELPER_GIT_TAG}"
)

set(D3DINTEROP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(D3DINTEROP_PUBLIC_INCLUDE_DIR "${D3DINTEROP_INCLUDE_DIR}/D3DInterop")
set(D3DINTEROP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(D3DINTEROP_SOURCES
    src/SharedFence.cpp
    src/SharedTexture.cpp
    src/D3D11Endpoint.cpp
    src/D3D11TextureEndpoint.cpp
    src/D3D12Endpoint.cpp
    src/D3D12TextureEndpoint.cpp
)

set(D3DINTEROP_PUBLIC_HEADERS
    include/D3DInterop/D3DInteropCommon.hpp
    include/D3DInterop/SharedFence.hpp
    include/D3DInterop/SharedTexture.hpp
    include/D3DInterop/D3D11Endpoint.hpp
    include/D3DInterop/D3D12Endpoint.hpp
    include/D3DInterop/D3DInterop.hpp
)

add_library(D3DInterop STATIC ${D3DINTEROP_SOURCES} ${D3DINTEROP_PUBLIC_HEADERS})
add_library(D3DInterop::D3DInterop ALIAS D3DInterop)

target_include_directories(D3DInterop
    PUBLIC
        "${D3DINTEROP_INCLUDE_DIR}"
        "${D3DINTEROP_PUBLIC_INCLUDE_DIR}"
    PRIVATE
        "${D3DINTEROP_PUBLIC_INCLUDE_DIR}"
)

target_compile_definitions(D3DInterop
    PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_link_libraries(D3DInterop
    PUBLIC
        D3D11Helper::D3D11Helper
        D3D12Helper::D3D12Helper
        d3d11
        d3d12
        dxgi
        dxguid
)

if(MSVC)
    target_compile_options(D3DInterop PRIVATE /utf-8 /EHsc)
    if(D3DINTEROP_ENABLE_WARNINGS)
        target_compile_options(D3DInterop PRIVATE /W4 /permissive-)
    endif()
else()
    if(D3DINTEROP_ENABLE_WARNINGS)
        target_compile_options(D3DInterop PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${D3DINTEROP_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header" FILES ${D3DINTEROP_PUBLIC_HEADERS})

if(D3DINTEROP_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(test)
endif()

if(D3DINTEROP_BUILD_SAMPLES)
    add_subdirectory(sample)
endif()
