add_executable(sample_ping_pong_feedback_11_to_12
    ping_pong_feedback_11_to_12.cpp
)

target_link_libraries(sample_ping_pong_feedback_11_to_12
    PRIVATE
        D3DInterop::D3DInterop
)

add_executable(sample_gpu_wait_11_to_12
    gpu_wait_11_to_12.cpp
)

target_link_libraries(sample_gpu_wait_11_to_12
    PRIVATE
        D3DInterop::D3DInterop
)

add_executable(sample_ring_async_no_readback_11_to_12
    ring_async_no_readback_11_to_12.cpp
)

target_link_libraries(sample_ring_async_no_readback_11_to_12
    PRIVATE
        D3DInterop::D3DInterop
)

if(MSVC)
    target_compile_options(sample_ping_pong_feedback_11_to_12 PRIVATE /utf-8 /EHsc)
    target_compile_options(sample_gpu_wait_11_to_12 PRIVATE /utf-8 /EHsc)
    target_compile_options(sample_ring_async_no_readback_11_to_12 PRIVATE /utf-8 /EHsc)
endif()

if(D3DINTEROP_BUILD_TESTS)
    add_test(NAME PingPongFeedback11To12 COMMAND sample_ping_pong_feedback_11_to_12)
    set_tests_properties(PingPongFeedback11To12 PROPERTIES SKIP_RETURN_CODE 77)

    add_test(NAME GpuWait11To12 COMMAND sample_gpu_wait_11_to_12)
    set_tests_properties(GpuWait11To12 PROPERTIES SKIP_RETURN_CODE 77)

    add_test(NAME RingAsyncNoReadback11To12 COMMAND sample_ring_async_no_readback_11_to_12)
    set_tests_properties(RingAsyncNoReadback11To12 PROPERTIES SKIP_RETURN_CODE 77)
endif()
