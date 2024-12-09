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
project(flusher_unittest)

add_executable(flusher_sls_unittest FlusherSLSUnittest.cpp)
target_link_libraries(flusher_sls_unittest ${UT_BASE_TARGET})

add_executable(pack_id_manager_unittest PackIdManagerUnittest.cpp)
target_link_libraries(pack_id_manager_unittest ${UT_BASE_TARGET})

if (ENABLE_ENTERPRISE)
    add_executable(enterprise_sls_client_manager_unittest EnterpriseSLSClientManagerUnittest.cpp)
    target_link_libraries(enterprise_sls_client_manager_unittest ${UT_BASE_TARGET})
    add_executable(enterprise_flusher_sls_monitor_unittest EnterpriseFlusherSLSMonitorUnittest.cpp)
    target_link_libraries(enterprise_flusher_sls_monitor_unittest ${UT_BASE_TARGET})
endif ()

include(GoogleTest)
gtest_discover_tests(flusher_sls_unittest)
gtest_discover_tests(pack_id_manager_unittest)
if (ENABLE_ENTERPRISE)
    gtest_discover_tests(enterprise_sls_client_manager_unittest)
    gtest_discover_tests(enterprise_flusher_sls_monitor_unittest)
endif ()
