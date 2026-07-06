add_executable(test_fence_roundtrip
    test_fence_roundtrip.cpp
)

target_link_libraries(test_fence_roundtrip
    PRIVATE
        D3DInterop::D3DInterop
)

add_executable(test_shared_texture_11_to_12
    test_shared_texture_11_to_12.cpp
)

target_link_libraries(test_shared_texture_11_to_12
    PRIVATE
        D3DInterop::D3DInterop
)

add_executable(test_texture_channel_11_to_12
    test_texture_channel_11_to_12.cpp
)

target_link_libraries(test_texture_channel_11_to_12
    PRIVATE
        D3DInterop::D3DInterop
)

add_executable(test_texture_ring_channel_11_to_12
    test_texture_ring_channel_11_to_12.cpp
)

target_link_libraries(test_texture_ring_channel_11_to_12
    PRIVATE
        D3DInterop::D3DInterop
)

add_executable(test_keyed_mutex_11_to_11
    test_keyed_mutex_11_to_11.cpp
)

target_link_libraries(test_keyed_mutex_11_to_11
    PRIVATE
        D3DInterop::D3DInterop
)

add_executable(test_unsupported_12_to_11
    test_unsupported_12_to_11.cpp
)

target_link_libraries(test_unsupported_12_to_11
    PRIVATE
        D3DInterop::D3DInterop
)

if(MSVC)
    target_compile_options(test_fence_roundtrip PRIVATE /utf-8 /EHsc)
    target_compile_options(test_shared_texture_11_to_12 PRIVATE /utf-8 /EHsc)
    target_compile_options(test_texture_channel_11_to_12 PRIVATE /utf-8 /EHsc)
    target_compile_options(test_texture_ring_channel_11_to_12 PRIVATE /utf-8 /EHsc)
    target_compile_options(test_keyed_mutex_11_to_11 PRIVATE /utf-8 /EHsc)
    target_compile_options(test_unsupported_12_to_11 PRIVATE /utf-8 /EHsc)
endif()

add_test(NAME FenceRoundTrip COMMAND test_fence_roundtrip)
set_tests_properties(FenceRoundTrip PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME SharedTexture11To12 COMMAND test_shared_texture_11_to_12)
set_tests_properties(SharedTexture11To12 PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME TextureChannel11To12 COMMAND test_texture_channel_11_to_12)
set_tests_properties(TextureChannel11To12 PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME TextureRingChannel11To12 COMMAND test_texture_ring_channel_11_to_12)
set_tests_properties(TextureRingChannel11To12 PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME KeyedMutexD3D11ToD3D11 COMMAND test_keyed_mutex_11_to_11)
set_tests_properties(KeyedMutexD3D11ToD3D11 PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME UnsupportedD3D12ToD3D11 COMMAND test_unsupported_12_to_11)
set_tests_properties(UnsupportedD3D12ToD3D11 PROPERTIES SKIP_RETURN_CODE 77)

option(D3DINTEROP_BUILD_INSTALLED_PACKAGE_TEST "Build CTest that validates installed D3DInterop package consumption" ON)
if(D3DINTEROP_BUILD_INSTALLED_PACKAGE_TEST)
    add_test(
        NAME InstalledPackageConsumer
        COMMAND "${CMAKE_COMMAND}"
            -DD3DINTEROP_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DD3DINTEROP_MAIN_BINARY_DIR=${CMAKE_BINARY_DIR}
            -DD3DINTEROP_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package_consumer
            -DTEST_CONFIGURATION=$<CONFIG>
            -P ${CMAKE_SOURCE_DIR}/cmake/RunInstalledPackageConsumerTest.cmake
    )
endif()
