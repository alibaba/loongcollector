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
project (common_unittest)

add_executable (common_simple_utils_unittest SimpleUtilsUnittest.cpp
    FileSystemUtilUnittest.h
    DevInodeUnittest.h
    TimeUtilUnittest.h
    StringToolsUnittest.h)
target_link_libraries (common_simple_utils_unittest unittest_base)

add_executable (common_logfileoperator_unittest LogFileOperatorUnittest.cpp)
target_link_libraries (common_logfileoperator_unittest unittest_base)

add_executable (common_sender_queue_unittest SenderQueueUnittest.cpp)
target_link_libraries (common_sender_queue_unittest unittest_base)

add_executable (common_queue_manager_unittest QueueManagerUnittest.cpp)
target_link_libraries (common_queue_manager_unittest unittest_base)

add_executable (common_sliding_window_counter_unittest SlidingWindowCounterUnittest.cpp)
target_link_libraries (common_sliding_window_counter_unittest unittest_base)
