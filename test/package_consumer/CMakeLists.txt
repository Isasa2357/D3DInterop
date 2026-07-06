cmake_minimum_required(VERSION 3.20)

project(D3DInteropPackageConsumer
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(D3DInterop CONFIG REQUIRED)

add_executable(d3dinterop_package_consumer
    main.cpp
)

target_link_libraries(d3dinterop_package_consumer
    PRIVATE
        D3DInterop::D3DInterop
)

if(MSVC)
    target_compile_options(d3dinterop_package_consumer PRIVATE /utf-8 /EHsc)
endif()

enable_testing()
add_test(NAME D3DInteropPackageConsumerRun COMMAND d3dinterop_package_consumer)
