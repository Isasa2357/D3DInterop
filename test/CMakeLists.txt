add_executable(test_fence_roundtrip
    test_fence_roundtrip.cpp
)

target_link_libraries(test_fence_roundtrip
    PRIVATE
        D3DInterop::D3DInterop
)

if(MSVC)
    target_compile_options(test_fence_roundtrip PRIVATE /utf-8 /EHsc)
endif()

add_test(NAME FenceRoundTrip COMMAND test_fence_roundtrip)
set_tests_properties(FenceRoundTrip PROPERTIES SKIP_RETURN_CODE 77)
