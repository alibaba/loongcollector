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

cmake_minimum_required(VERSION 3.22)
project(event_unittest)

add_executable(event_unittest EventUnittest.cpp)
target_link_libraries(event_unittest ${UT_BASE_TARGET})

add_executable(blocked_event_manager_unittest BlockedEventManagerUnittest.cpp)
target_link_libraries(blocked_event_manager_unittest ${UT_BASE_TARGET})

include(GoogleTest)
gtest_discover_tests(event_unittest)
gtest_discover_tests(blocked_event_manager_unittest)
