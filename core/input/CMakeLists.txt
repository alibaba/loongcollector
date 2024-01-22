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
project(input)

file(GLOB LIB_SOURCE_FILES *.cpp *.h)
if (NOT LINUX)
    list(REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/InputObserverNetwork.h ${CMAKE_CURRENT_SOURCE_DIR}/InputObserverNetwork.cpp)
    if (ENABLE_ENTERPRISE)
        list(REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/InputStream.h ${CMAKE_CURRENT_SOURCE_DIR}/InputStream.cpp)
    endif ()
endif ()
append_source_files(LIB_SOURCE_FILES)
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} config)
target_link_libraries(${PROJECT_NAME} file_server)
target_link_libraries(${PROJECT_NAME} reader)
target_link_libraries(${PROJECT_NAME} container_manager)
if (LINUX)
    target_link_libraries(${PROJECT_NAME} observer)
    if (ENABLE_ENTERPRISE)
        target_link_libraries(${PROJECT_NAME} streamlog)
    endif()
endif()
