cmake_minimum_required(VERSION 3.22)
project(ebpf_unittest)

include(GoogleTest)

set(EBPF_DRIVER_DIR "${CMAKE_BINARY_DIR}/ebpf/driver")
set(COOLBPF_DIR "${CMAKE_BINARY_DIR}/third_party/coolbpf/src")

function(add_unittest target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} ${UT_BASE_TARGET})
    gtest_discover_tests(${target_name})
    if(target_name STREQUAL "ebpf_server_unittest")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${EBPF_DRIVER_DIR}/libEbpfDriver.so $<TARGET_FILE_DIR:${target_name}>
            COMMAND ${CMAKE_COMMAND} -E copy
            ${COOLBPF_DIR}/libcoolbpf.so.1.0.0 $<TARGET_FILE_DIR:${target_name}>
        )
    endif()
endfunction()

add_unittest(aggregator_unittest AggregatorUnittest.cpp)
add_unittest(ebpf_server_unittest eBPFServerUnittest.cpp)
add_unittest(sampler_unittest SamplerUnittest.cpp)
add_unittest(consumer_unittest ConsumerUnittest.cpp)
add_unittest(id_allocator_unittest IdAllocatorUnittest.cpp)
add_unittest(table_unittest TableUnittest.cpp)
add_unittest(protocol_parser_unittest ProtocolParserUnittest.cpp)
add_unittest(manager_unittest ManagerUnittest.cpp)
add_unittest(common_util_unittest CommonUtilUnittest.cpp)
add_unittest(networkobserver_event_unittest NetworkObserverEventUnittest.cpp)
add_unittest(conntracker_unittest ConnTrackerUnittest.cpp)
add_unittest(conntracker_manager_unittest ConnTrackerManagerUnittest.cpp)
add_unittest(networkobserver_unittest NetworkObserverUnittest.cpp)
add_unittest(worker_unittest WorkerUnittest.cpp)
