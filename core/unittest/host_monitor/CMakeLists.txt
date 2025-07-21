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
project(host_monitor_unittest)

add_executable(process_entity_collector_unittest ProcessEntityCollectorUnittest.cpp)
target_link_libraries(process_entity_collector_unittest ${UT_BASE_TARGET})

add_executable(host_monitor_input_runner_unittest HostMonitorInputRunnerUnittest.cpp)
target_link_libraries(host_monitor_input_runner_unittest ${UT_BASE_TARGET})

add_executable(cpu_collector_unittest CPUCollectorUnittest.cpp)
target_link_libraries(cpu_collector_unittest ${UT_BASE_TARGET})

add_executable(system_collector_unittest SystemCollectorUnittest.cpp)
target_link_libraries(system_collector_unittest ${UT_BASE_TARGET})

add_executable(metric_calculate_unittest MetricCalculateUnittest.cpp)
target_link_libraries(metric_calculate_unittest ${UT_BASE_TARGET})

add_executable(system_interface_unittest SystemInterfaceUnittest.cpp)
target_link_libraries(system_interface_unittest ${UT_BASE_TARGET})

if (LINUX)
    add_executable(linux_system_interface_unittest LinuxSystemInterfaceUnittest.cpp)
    target_link_libraries(linux_system_interface_unittest ${UT_BASE_TARGET})
endif()
add_executable(mem_collector_unittest MemCollectorUnittest.cpp)
target_link_libraries(mem_collector_unittest ${UT_BASE_TARGET})

include(GoogleTest)
gtest_discover_tests(process_entity_collector_unittest)
gtest_discover_tests(host_monitor_input_runner_unittest)
gtest_discover_tests(cpu_collector_unittest)
gtest_discover_tests(metric_calculate_unittest)
gtest_discover_tests(system_interface_unittest)
if (LINUX)
    gtest_discover_tests(linux_system_interface_unittest)
endif()
gtest_discover_tests(system_collector_unittest)
gtest_discover_tests(mem_collector_unittest)
