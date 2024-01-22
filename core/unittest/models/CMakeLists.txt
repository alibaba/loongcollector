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
project(models_unittest)

add_executable(pipeline_event_unittest PipelineEventUnittest.cpp)
target_link_libraries(pipeline_event_unittest unittest_base)

add_executable(log_event_unittest LogEventUnittest.cpp)
target_link_libraries(log_event_unittest unittest_base)

add_executable(pipeline_event_ptr_unittest PipelineEventPtrUnittest.cpp)
target_link_libraries(pipeline_event_ptr_unittest unittest_base)

add_executable(pipeline_event_group_unittest PipelineEventGroupUnittest.cpp)
target_link_libraries(pipeline_event_group_unittest unittest_base)

include(GoogleTest)
gtest_discover_tests(pipeline_event_unittest)
gtest_discover_tests(log_event_unittest)
gtest_discover_tests(pipeline_event_ptr_unittest)
gtest_discover_tests(pipeline_event_group_unittest)
