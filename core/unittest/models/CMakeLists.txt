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
target_link_libraries(pipeline_event_unittest ${UT_BASE_TARGET})

add_executable(log_event_unittest LogEventUnittest.cpp)
target_link_libraries(log_event_unittest ${UT_BASE_TARGET})

add_executable(metric_value_unittest MetricValueUnittest.cpp)
target_link_libraries(metric_value_unittest ${UT_BASE_TARGET})

add_executable(metric_event_unittest MetricEventUnittest.cpp)
target_link_libraries(metric_event_unittest ${UT_BASE_TARGET})

add_executable(span_event_unittest SpanEventUnittest.cpp)
target_link_libraries(span_event_unittest ${UT_BASE_TARGET})

add_executable(pipeline_event_ptr_unittest PipelineEventPtrUnittest.cpp)
target_link_libraries(pipeline_event_ptr_unittest ${UT_BASE_TARGET})

add_executable(pipeline_event_group_unittest PipelineEventGroupUnittest.cpp)
target_link_libraries(pipeline_event_group_unittest ${UT_BASE_TARGET})

include(GoogleTest)
gtest_discover_tests(pipeline_event_unittest)
gtest_discover_tests(log_event_unittest)
gtest_discover_tests(metric_value_unittest)
gtest_discover_tests(metric_event_unittest)
gtest_discover_tests(span_event_unittest)
gtest_discover_tests(pipeline_event_ptr_unittest)
gtest_discover_tests(pipeline_event_group_unittest)

add_executable(event_group_benchmark EventGroupBenchmark.cpp)
target_link_libraries(event_group_benchmark ${UT_BASE_TARGET})
