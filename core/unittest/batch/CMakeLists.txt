# Copyright 2024 iLogtail Authors
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
project(batch_unittest)

add_executable(flush_strategy_unittest FlushStrategyUnittest.cpp)
target_link_libraries(flush_strategy_unittest unittest_base)

add_executable(batch_status_unittest BatchStatusUnittest.cpp)
target_link_libraries(batch_status_unittest unittest_base)

add_executable(batch_item_unittest BatchItemUnittest.cpp)
target_link_libraries(batch_item_unittest unittest_base)

add_executable(batcher_unittest BatcherUnittest.cpp)
target_link_libraries(batcher_unittest unittest_base)

add_executable(timeout_flush_manager_unittest TimeoutFlushManagerUnittest.cpp)
target_link_libraries(timeout_flush_manager_unittest unittest_base)

include(GoogleTest)
gtest_discover_tests(flush_strategy_unittest)
gtest_discover_tests(batch_status_unittest)
gtest_discover_tests(batch_item_unittest)
gtest_discover_tests(batcher_unittest)
gtest_discover_tests(timeout_flush_manager_unittest)
