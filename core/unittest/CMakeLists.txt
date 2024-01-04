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

cmake_minimum_required(VERSION 3.10)
project(unittest_base)

add_definitions(-DAPSARA_UNIT_TEST_MAIN)
add_definitions(-DLOGTAIL_NO_TC_MALLOC)
if (MSVC)
    add_definitions(-DNOMINMAX)
endif ()
add_library(${PROJECT_NAME} STATIC ${ALL_SOURCE_FILES})
link_jsoncpp(${PROJECT_NAME})
link_yamlcpp(${PROJECT_NAME})
link_boost(${PROJECT_NAME})
link_gflags(${PROJECT_NAME})
link_curl(${PROJECT_NAME})
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
link_spl(${PROJECT_NAME})
link_tcmalloc(${PROJECT_NAME})

if (UNIX)
    target_link_libraries(${PROJECT_NAME} pthread uuid dl)
    if (ENABLE_COMPATIBLE_MODE)
        target_link_libraries(${PROJECT_NAME} rt)
        target_link_libraries(${PROJECT_NAME} -static-libstdc++ -static-libgcc)
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
add_subdirectory(reader)
add_subdirectory(sdk)
add_subdirectory(sender)
add_subdirectory(spl)

if (UNIX)
    add_subdirectory(observer)
endif ()