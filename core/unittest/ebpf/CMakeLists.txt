cmake_minimum_required(VERSION 3.22)
project(ebpf_unittest)

# add_executable(aggregator_unittest AggregatorUnittest.cpp)
# target_link_libraries(aggregator_unittest ${UT_BASE_TARGET})

# add_executable(ebpf_server_unittest eBPFServerUnittest.cpp)
# target_link_libraries(ebpf_server_unittest ${UT_BASE_TARGET})

# add_executable(sampler_unittest SamplerUnittest.cpp)
# target_link_libraries(sampler_unittest ${UT_BASE_TARGET})

# add_executable(consumer_unittest ConsumerUnittest.cpp)
# target_link_libraries(consumer_unittest ${UT_BASE_TARGET})

# add_executable(id_allocator_unittest IdAllocatorUnittest.cpp)
# target_link_libraries(id_allocator_unittest ${UT_BASE_TARGET})

# add_executable(table_unittest TableUnittest.cpp)
# target_link_libraries(table_unittest ${UT_BASE_TARGET})

# add_executable(protocol_parser_unittest ProtocolParserUnittest.cpp)
# target_link_libraries(protocol_parser_unittest ${UT_BASE_TARGET})

# include(GoogleTest)

# gtest_discover_tests(ebpf_server_unittest)
# gtest_discover_tests(aggregator_unittest)
# gtest_discover_tests(sampler_unittest)
# gtest_discover_tests(consumer_unittest)
# gtest_discover_tests(id_allocator_unittest)
# gtest_discover_tests(table_unittest)
# gtest_discover_tests(protocol_parser_unittest)


include(GoogleTest)

function(add_unittest target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} ${UT_BASE_TARGET})
    gtest_discover_tests(${target_name})
endfunction()

add_unittest(aggregator_unittest AggregatorUnittest.cpp)
add_unittest(ebpf_server_unittest eBPFServerUnittest.cpp)
add_unittest(sampler_unittest SamplerUnittest.cpp)
add_unittest(consumer_unittest ConsumerUnittest.cpp)
add_unittest(id_allocator_unittest IdAllocatorUnittest.cpp)
add_unittest(table_unittest TableUnittest.cpp)
add_unittest(protocol_parser_unittest ProtocolParserUnittest.cpp)
