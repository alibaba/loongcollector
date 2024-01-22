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
project(common)

file(GLOB LIB_SOURCE_FILES *.cpp *.h)
list(REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/BoostRegexValidator.cpp)
list(REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/GetUUID.cpp)
if (MSVC)
    add_definitions(-DNOMINMAX)
    if (ENABLE_ENTERPRISE)
        list(REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/LinuxDaemonUtil.h)
        list(REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/LinuxDaemonUtil.cpp)
    endif()
elseif (LINUX)
    # needed by observer
    file(GLOB PICOHTTPPARSER_SOURCE_FILES protocol/picohttpparser/*.c protocol/picohttpparser/*.h)
    list(APPEND LIB_SOURCE_FILES ${PICOHTTPPARSER_SOURCE_FILES})
endif ()
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    file(GLOB XX_HASH_SOURCE_FILES xxhash/*.c xxhash/*.h)
else ()
    file(GLOB XX_HASH_SOURCE_FILES xxhash/xxhash.c xxhash/xxhash.h)
endif ()
list(APPEND LIB_SOURCE_FILES ${XX_HASH_SOURCE_FILES})

append_source_files(LIB_SOURCE_FILES)
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} logger)
target_link_libraries(${PROJECT_NAME} fuse)
link_jsoncpp(${PROJECT_NAME})
link_yamlcpp(${PROJECT_NAME})
link_boost(${PROJECT_NAME})
link_gflags(${PROJECT_NAME})
link_lz4(${PROJECT_NAME})
link_zlib(${PROJECT_NAME})
link_zstd(${PROJECT_NAME})
link_unwind(${PROJECT_NAME})
link_asan(${PROJECT_NAME})
if (UNIX)
    link_uuid(${PROJECT_NAME})
    if (LINUX)
        target_link_libraries(${PROJECT_NAME} pthread)
    endif ()
elseif (MSVC)
    target_link_libraries(${PROJECT_NAME} "ws2_32.lib")
    target_link_libraries(${PROJECT_NAME} "Rpcrt4.lib")
    target_link_libraries(${PROJECT_NAME} "Shlwapi.lib")
endif ()

if (LINUX)
    add_executable(BoostRegexValidator BoostRegexValidator.cpp)
    link_jsoncpp(BoostRegexValidator)
    link_boost(BoostRegexValidator)
endif ()
