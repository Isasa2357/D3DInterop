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

if(MSVC)
    target_compile_options(test_fence_roundtrip PRIVATE /utf-8 /EHsc)
    target_compile_options(test_shared_texture_11_to_12 PRIVATE /utf-8 /EHsc)
    target_compile_options(test_texture_channel_11_to_12 PRIVATE /utf-8 /EHsc)
endif()

add_test(NAME FenceRoundTrip COMMAND test_fence_roundtrip)
set_tests_properties(FenceRoundTrip PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME SharedTexture11To12 COMMAND test_shared_texture_11_to_12)
set_tests_properties(SharedTexture11To12 PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME TextureChannel11To12 COMMAND test_texture_channel_11_to_12)
set_tests_properties(TextureChannel11To12 PROPERTIES SKIP_RETURN_CODE 77)
