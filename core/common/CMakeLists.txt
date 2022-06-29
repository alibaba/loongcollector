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
project(common)

file(GLOB LIB_SOURCE_FILES *.cpp *.h)
list(REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/BoostRegexValidator.cpp)
list(REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/GetUUID.cpp)
if (MSVC)
    add_definitions (-DNOMINMAX)
    list (REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/LinuxDaemonUtil.h)
    list (REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/LinuxDaemonUtil.cpp)
    set (LIB_SOURCE_FILES ${LIB_SOURCE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/strptime.c)
elseif (UNIX)
    list (REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/WinUuid.h)
    list (REMOVE_ITEM LIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/WinUuid.cpp)
    file (GLOB DMI_UUID_SOURCE_FILES dmi_uuid/*.c dmi_uuid/*.h)
    list (APPEND LIB_SOURCE_FILES ${DMI_UUID_SOURCE_FILES})
endif ()
append_source_files (LIB_SOURCE_FILES)
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCE_FILES})
target_link_libraries (${PROJECT_NAME} logger)
target_link_libraries (${PROJECT_NAME} fuse)
link_jsoncpp    (${PROJECT_NAME})
link_yamlcpp    (${PROJECT_NAME})
link_boost      (${PROJECT_NAME})
link_gflags     (${PROJECT_NAME})
link_lz4        (${PROJECT_NAME})
link_zlib       (${PROJECT_NAME})
link_unwind	    (${PROJECT_NAME})
if (UNIX)
    target_link_libraries (${PROJECT_NAME} pthread uuid)
elseif (MSVC)
    target_link_libraries (${PROJECT_NAME} "ws2_32.lib")
    target_link_libraries (${PROJECT_NAME} "Rpcrt4.lib")
    target_link_libraries (${PROJECT_NAME} "Shlwapi.lib")
endif ()

add_executable(BoostRegexValidator BoostRegexValidator.cpp)
link_jsoncpp (BoostRegexValidator)
link_boost   (BoostRegexValidator)