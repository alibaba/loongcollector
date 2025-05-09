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


if (LINUX)
    add_executable(input_prometheus_unittest InputPrometheusUnittest.cpp)
    target_link_libraries(input_prometheus_unittest ${UT_BASE_TARGET})

    add_executable(input_container_stdio_unittest InputContainerStdioUnittest.cpp)
    target_link_libraries(input_container_stdio_unittest ${UT_BASE_TARGET})

    # add_executable(input_ebpf_file_security_unittest InputFileSecurityUnittest.cpp)
    # target_link_libraries(input_ebpf_file_security_unittest unittest_base)

    add_executable(input_ebpf_process_security_unittest InputProcessSecurityUnittest.cpp)
    target_link_libraries(input_ebpf_process_security_unittest unittest_base)

    add_executable(input_ebpf_network_security_unittest InputNetworkSecurityUnittest.cpp)
    target_link_libraries(input_ebpf_network_security_unittest unittest_base)

    add_executable(input_ebpf_network_observer_unittest InputNetworkObserverUnittest.cpp)
    target_link_libraries(input_ebpf_network_observer_unittest unittest_base)
	
    add_executable(input_host_meta_unittest InputHostMetaUnittest.cpp)
    target_link_libraries(input_host_meta_unittest unittest_base)
endif()

add_executable(input_internal_alarms_unittest InputInternalAlarmsUnittest.cpp)
target_link_libraries(input_internal_alarms_unittest ${UT_BASE_TARGET})

add_executable(input_internal_metrics_unittest InputInternalMetricsUnittest.cpp)
target_link_libraries(input_internal_metrics_unittest ${UT_BASE_TARGET})

if (LINUX)
    set(EBPF_DRIVER_DIR "${CMAKE_BINARY_DIR}/ebpf/driver")
    set(COOLBPF_DIR "${CMAKE_BINARY_DIR}/_thirdparty/coolbpf/src")

    add_custom_command(TARGET input_ebpf_process_security_unittest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${EBPF_DRIVER_DIR}/libeBPFDriver.so $<TARGET_FILE_DIR:input_ebpf_process_security_unittest>
        COMMAND ${CMAKE_COMMAND} -E copy
        ${COOLBPF_DIR}/libcoolbpf.so.1.0.0 $<TARGET_FILE_DIR:input_ebpf_process_security_unittest>
    )

    add_executable(input_host_monitor_unittest InputHostMonitorUnittest.cpp)
    target_link_libraries(input_host_monitor_unittest unittest_base)
endif()

include(GoogleTest)
gtest_discover_tests(input_file_unittest)
gtest_discover_tests(input_internal_alarms_unittest)
gtest_discover_tests(input_internal_metrics_unittest)
if (LINUX)
    gtest_discover_tests(input_prometheus_unittest)
    gtest_discover_tests(input_host_meta_unittest)
    gtest_discover_tests(input_container_stdio_unittest)
    # gtest_discover_tests(input_ebpf_file_security_unittest)
    gtest_discover_tests(input_ebpf_process_security_unittest)
    gtest_discover_tests(input_ebpf_network_security_unittest)
    gtest_discover_tests(input_ebpf_network_observer_unittest)
    gtest_discover_tests(input_host_meta_unittest)
    gtest_discover_tests(input_host_monitor_unittest)
endif()
