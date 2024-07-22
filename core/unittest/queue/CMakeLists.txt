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
project(queue_unittest)

add_executable(queue_key_manager_unittest QueueKeyManagerUnittest.cpp)
target_link_libraries(queue_key_manager_unittest unittest_base)

add_executable(process_queue_unittest ProcessQueueUnittest.cpp)
target_link_libraries(process_queue_unittest unittest_base)

add_executable(process_queue_manager_unittest ProcessQueueManagerUnittest.cpp)
target_link_libraries(process_queue_manager_unittest unittest_base)

add_executable(sender_queue_unittest SenderQueueUnittest.cpp)
target_link_libraries(sender_queue_unittest unittest_base)

add_executable(sender_queue_manager_unittest SenderQueueManagerUnittest.cpp)
target_link_libraries(sender_queue_manager_unittest unittest_base)

add_executable(exactly_once_sender_queue_unittest ExactlyOnceSenderQueueUnittest.cpp)
target_link_libraries(exactly_once_sender_queue_unittest unittest_base)

add_executable(exactly_once_queue_manager_unittest ExactlyOnceQueueManagerUnittest.cpp)
target_link_libraries(exactly_once_queue_manager_unittest unittest_base)

include(GoogleTest)
gtest_discover_tests(queue_key_manager_unittest)
gtest_discover_tests(process_queue_unittest)
gtest_discover_tests(process_queue_manager_unittest)
gtest_discover_tests(sender_queue_unittest)
gtest_discover_tests(sender_queue_manager_unittest)
gtest_discover_tests(exactly_once_sender_queue_unittest)
gtest_discover_tests(exactly_once_queue_manager_unittest)
