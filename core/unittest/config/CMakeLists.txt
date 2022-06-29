# Copyright 2022 iLogtail Authors
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

cmake_minimum_required (VERSION 2.8)
project (config_unittest)

add_executable (config_match_unittest ConfigMatchUnittest.cpp)
target_link_libraries (config_match_unittest unittest_base)

add_executable (config_updator_unittest ConfigUpdatorUnittest.cpp)
target_link_libraries (config_updator_unittest unittest_base)

add_executable (config_manager_base_unittest ConfigManagerBaseUnittest.cpp)
target_link_libraries (config_manager_base_unittest unittest_base)

add_executable (config_yaml_to_json_unittest ConfigYamlToJsonUnittest.cpp)
target_link_libraries (config_yaml_to_json_unittest unittest_base)

if (UNIX)
    file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/testConfigDir)
    file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/testConfigDir/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/testConfigDir/)
elseif (MSVC)
    file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/testConfigDir)
    file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/testConfigDir/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/testConfigDir/)
endif ()