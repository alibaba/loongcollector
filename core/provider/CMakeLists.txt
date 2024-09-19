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
project(provider)

file(GLOB LIB_SOURCE_FILES *.cpp *.h)

set(PROVIDER_SUB_DIRECTORIES_LIST    
    profile_sender config/feedbacker config/provider config/common_provider protobuf/config_server/v1 protobuf/config_server/v2
)

foreach(DIR_NAME IN LISTS PROVIDER_SUB_DIRECTORIES_LIST)
    file(GLOB TEMP_SOURCE_FILES 
        "${CMAKE_CURRENT_SOURCE_DIR}/../${DIR_NAME}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../${DIR_NAME}/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/../${DIR_NAME}/*.c"
        )
    list(APPEND LIB_SOURCE_FILES ${TEMP_SOURCE_FILES})
endforeach()

add_library(${PROJECT_NAME} STATIC ${LIB_SOURCE_FILES})

# message(STATUS "PROVIDER SOURCE FILES: ${LIB_SOURCE_FILES}")

foreach(DIR_NAME IN LISTS PROVIDER_SUB_DIRECTORIES_LIST)
    target_include_directories(provider PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../${DIR_NAME})
endforeach()