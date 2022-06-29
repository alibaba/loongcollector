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

cmake_minimum_required(VERSION 2.8)
project(event_listener)

if (UNIX)
    file(GLOB LIB_SOURCE_FILES *_Linux.cpp *_Linux.h)
elseif (MSVC)
    set (LIB_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/EventListener_Windows.h
        ${CMAKE_CURRENT_SOURCE_DIR}/EventListener_Windows.cpp)
endif()
list (APPEND LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/EventListener.h)

append_source_files (LIB_SOURCE_FILES)
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCE_FILES})
target_link_libraries (${PROJECT_NAME} common)
target_link_libraries (${PROJECT_NAME} logger)
target_link_libraries (${PROJECT_NAME} polling)
target_link_libraries (${PROJECT_NAME} event)
target_link_libraries (${PROJECT_NAME} profiler)
target_link_libraries (${PROJECT_NAME} controller)