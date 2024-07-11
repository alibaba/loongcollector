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
project(unittest_base)

include(${CMAKE_SOURCE_DIR}/links.cmake)
include(${CMAKE_SOURCE_DIR}/common/links.cmake)
include(${CMAKE_SOURCE_DIR}/processor/links.cmake)
include(${CMAKE_SOURCE_DIR}/input/links.cmake)
include(${CMAKE_SOURCE_DIR}/flusher/links.cmake)

add_definitions(-DAPSARA_UNIT_TEST_MAIN)
set(NO_TCMALLOC TRUE)
add_definitions(-DLOGTAIL_NO_TC_MALLOC)
if (MSVC)
    add_definitions(-DNOMINMAX)
endif ()
if (LINUX AND WITHOUTSPL)
    add_library(${PROJECT_NAME} SHARED ${UNIX_SOURCE_FILES})
else ()
    add_library(${PROJECT_NAME} STATIC ${UNIX_SOURCE_FILES})
endif()
if(MSVC)
    # For MSVC, use /W4 for a high level of warnings, and treat warnings as errors.
    # target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
endif()

input_link(${PROJECT_NAME})
processor_link(${PROJECT_NAME})
flusher_link(${PROJECT_NAME})
all_link(${PROJECT_NAME})
common_link(${PROJECT_NAME})

add_subdirectory(app_config)
add_subdirectory(batch)
add_subdirectory(checkpoint)
add_subdirectory(common)
add_subdirectory(compression)
add_subdirectory(config)
add_subdirectory(config_sdk)
add_subdirectory(container_manager)
add_subdirectory(controller)
add_subdirectory(event)
add_subdirectory(event_handler)
add_subdirectory(file_source)
add_subdirectory(flusher)
add_subdirectory(input)
add_subdirectory(log_pb)
add_subdirectory(models)
add_subdirectory(monitor)
add_subdirectory(pipeline)
add_subdirectory(plugin)
add_subdirectory(polling)
add_subdirectory(processor)
add_subdirectory(queue)
add_subdirectory(reader)
add_subdirectory(sdk)
add_subdirectory(sender)
add_subdirectory(serializer)

if (LINUX)
    if (NOT WITHOUTSPL)
        add_subdirectory(spl)
    endif ()
    add_subdirectory(observer)
endif ()