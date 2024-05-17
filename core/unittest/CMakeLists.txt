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

add_definitions(-DAPSARA_UNIT_TEST_MAIN)
set(NO_TCMALLOC TRUE)
add_definitions(-DLOGTAIL_NO_TC_MALLOC)
if (MSVC)
    add_definitions(-DNOMINMAX)
endif ()
if (LINUX AND WITHOUTSPL)
    add_library(${PROJECT_NAME} SHARED ${ALL_SOURCE_FILES})
else ()
    add_library(${PROJECT_NAME} STATIC ${ALL_SOURCE_FILES})
endif()
if(MSVC)
    # For MSVC, use /W4 for a high level of warnings, and treat warnings as errors.
    # target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
endif()
link_jsoncpp(${PROJECT_NAME})
link_yamlcpp(${PROJECT_NAME})
link_boost(${PROJECT_NAME})
link_gflags(${PROJECT_NAME})
link_lz4(${PROJECT_NAME})
link_zlib(${PROJECT_NAME})
link_zstd(${PROJECT_NAME})
link_unwind(${PROJECT_NAME})
link_gtest(${PROJECT_NAME})
link_re2(${PROJECT_NAME})
link_protobuf(${PROJECT_NAME})
link_cityhash(${PROJECT_NAME})
link_leveldb(${PROJECT_NAME})
link_asan(${PROJECT_NAME})
if (LINUX AND NOT WITHOUTSPL)
    link_spl(${PROJECT_NAME})
endif ()
link_curl(${PROJECT_NAME})
link_ssl(${PROJECT_NAME}) # must after link_spl
link_crypto(${PROJECT_NAME}) # must after link_spl
link_tcmalloc(${PROJECT_NAME})

if (UNIX)
    link_uuid(${PROJECT_NAME})
    target_link_libraries(${PROJECT_NAME} dl)
    if (LINUX)
        target_link_libraries(${PROJECT_NAME} pthread)
    endif ()
    if (ENABLE_COMPATIBLE_MODE)
        target_link_libraries(${PROJECT_NAME} rt -static-libstdc++ -static-libgcc)
    endif ()
elseif (MSVC)
    target_link_libraries(${PROJECT_NAME} "ws2_32.lib")
    target_link_libraries(${PROJECT_NAME} "Rpcrt4.lib")
    target_link_libraries(${PROJECT_NAME} "Shlwapi.lib")
endif ()

add_subdirectory(app_config)
add_subdirectory(aggregator)
add_subdirectory(checkpoint)
add_subdirectory(common)
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

if (LINUX)
    if (NOT WITHOUTSPL)
        add_subdirectory(spl)
    endif ()
    add_subdirectory(observer)
endif ()