cmake_minimum_required(VERSION 3.22)
project(ebpf_unittest)

add_executable(aggregator_unittest AggregatorUnittest.cpp)
target_link_libraries(aggregator_unittest ${UT_BASE_TARGET})

add_executable(ebpf_server_unittest eBPFServerUnittest.cpp)
target_link_libraries(ebpf_server_unittest ${UT_BASE_TARGET})

add_executable(sampler_unittest SamplerUnittest.cpp)
target_link_libraries(sampler_unittest ${UT_BASE_TARGET})

add_executable(consumer_unittest ConsumerUnittest.cpp)
target_link_libraries(consumer_unittest ${UT_BASE_TARGET})

include(GoogleTest)

gtest_discover_tests(ebpf_server_unittest)
gtest_discover_tests(aggregator_unittest)
gtest_discover_tests(sampler_unittest)
gtest_discover_tests(consumer_unittest)

