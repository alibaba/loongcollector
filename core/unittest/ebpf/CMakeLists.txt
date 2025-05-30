cmake_minimum_required(VERSION 3.22)
project(ebpf_unittest)

include(GoogleTest)

set(EBPF_DRIVER_DIR "${CMAKE_BINARY_DIR}/ebpf/driver")
set(COOLBPF_DIR "${CMAKE_BINARY_DIR}/_thirdparty/coolbpf/src")

function(add_driver_unittest target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} ${UT_BASE_TARGET} eBPFDriver)
    gtest_discover_tests(${target_name})
endfunction()

function(add_unittest target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} ${UT_BASE_TARGET})
    gtest_discover_tests(${target_name})
    if(target_name STREQUAL "ebpf_server_unittest")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${EBPF_DRIVER_DIR}/libeBPFDriver.so $<TARGET_FILE_DIR:${target_name}>
            COMMAND ${CMAKE_COMMAND} -E copy
            ${COOLBPF_DIR}/libcoolbpf.so.1.0.0 $<TARGET_FILE_DIR:${target_name}>
        )
    endif()
endfunction()

add_unittest(aggregator_unittest AggregatorUnittest.cpp)
add_unittest(ebpf_server_unittest EBPFServerUnittest.cpp)
add_unittest(sampler_unittest SamplerUnittest.cpp)
add_unittest(table_unittest TableUnittest.cpp)
add_unittest(protocol_parser_unittest ProtocolParserUnittest.cpp)
add_unittest(manager_unittest ManagerUnittest.cpp)
add_unittest(common_util_unittest CommonUtilUnittest.cpp)
add_unittest(trace_id_benchmark TraceIdBenchmark.cpp)
add_unittest(networkobserver_event_unittest NetworkObserverEventUnittest.cpp)
add_unittest(networkobserver_unittest NetworkObserverUnittest.cpp)
add_unittest(connection_unittest ConnectionUnittest.cpp)
add_unittest(connection_manager_unittest ConnectionManagerUnittest.cpp)
add_unittest(process_cache_unittest ProcessCacheUnittest.cpp)
add_unittest(process_cache_value_unittest ProcessCacheValueUnittest.cpp)
add_unittest(process_cache_manager_unittest ProcessCacheManagerUnittest.cpp)
add_unittest(process_data_map_unittest ProcessDataMapUnittest.cpp)
add_unittest(process_cleanup_retryable_event_unittest ProcessCleanupRetryableEventUnittest.cpp)
add_unittest(process_clone_retryable_event_unittest ProcessCloneRetryableEventUnittest.cpp)
add_unittest(process_execve_retryable_event_unittest ProcessExecveRetryableEventUnittest.cpp)
add_unittest(process_exit_retryable_event_unittest ProcessExitRetryableEventUnittest.cpp)
add_unittest(process_sync_retryable_event_unittest ProcessSyncRetryableEventUnittest.cpp)
add_unittest(retryable_event_unittest RetryableEventUnittest.cpp)

add_driver_unittest(id_allocator_unittest IdAllocatorUnittest.cpp)
add_driver_unittest(ebpf_driver_unittest eBPFDriverUnittest.cpp)
add_driver_unittest(bpf_map_traits_unittest BPFMapTraitsUnittest.cpp)
add_driver_unittest(bpf_wrapper_unittest BPFWrapperUnittest.cpp)
