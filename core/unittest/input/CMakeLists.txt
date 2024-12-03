# Copyright 2023 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.22)
project(input_unittest)

add_executable(input_file_unittest InputFileUnittest.cpp)
target_link_libraries(input_file_unittest ${UT_BASE_TARGET})

add_executable(input_container_stdio_unittest InputContainerStdioUnittest.cpp)
target_link_libraries(input_container_stdio_unittest ${UT_BASE_TARGET})

add_executable(input_prometheus_unittest InputPrometheusUnittest.cpp)
target_link_libraries(input_prometheus_unittest ${UT_BASE_TARGET})

add_executable(input_ebpf_file_security_unittest InputFileSecurityUnittest.cpp)
target_link_libraries(input_ebpf_file_security_unittest unittest_base)

add_executable(input_ebpf_process_security_unittest InputProcessSecurityUnittest.cpp)
target_link_libraries(input_ebpf_process_security_unittest unittest_base)

add_executable(input_ebpf_network_security_unittest InputNetworkSecurityUnittest.cpp)
target_link_libraries(input_ebpf_network_security_unittest unittest_base)

add_executable(input_ebpf_network_observer_unittest InputNetworkObserverUnittest.cpp)
target_link_libraries(input_ebpf_network_observer_unittest unittest_base)

add_executable(input_host_meat_unittest InputHostMetaUnittest.cpp)
target_link_libraries(input_host_meat_unittest unittest_base)

include(GoogleTest)
gtest_discover_tests(input_file_unittest)
gtest_discover_tests(input_container_stdio_unittest)
gtest_discover_tests(input_prometheus_unittest)
gtest_discover_tests(input_ebpf_file_security_unittest)
gtest_discover_tests(input_ebpf_process_security_unittest)
gtest_discover_tests(input_ebpf_network_security_unittest)
gtest_discover_tests(input_ebpf_network_observer_unittest)
gtest_discover_tests(input_host_meat_unittest)
