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
project(logtail)

include(CMakeDependentOption)

# variable LINUX is supported in cmake version 3.25
# for now, explicitly set LINUX to true if the platform is linux
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX TRUE)
endif ()

# Options.
option(BUILD_LOGTAIL "Build Logtail executable and tools" ON)
option(BUILD_LOGTAIL_SHARED_LIBRARY "Build Logtail shared library")
option(ENABLE_ENTERPRISE "enable enterprise feature")
cmake_dependent_option(ENABLE_COMPATIBLE_MODE "Build Logtail in compatible mode (for low version Linux)" OFF "LINUX" OFF)
cmake_dependent_option(ENABLE_STATIC_LINK_CRT "Build Logtail by linking CRT statically" OFF "LINUX" OFF)
option(WITHOUTGDB "Build Logtail without gdb")
option(BUILD_LOGTAIL_UT "Build unit test for Logtail")

if (ENABLE_ENTERPRISE)
    message(STATUS "Enable Enterprise Feature.")
    add_definitions(-D__ENTERPRISE__)
    include(${CMAKE_CURRENT_SOURCE_DIR}/enterprise_options.cmake)
else ()
    include(${CMAKE_CURRENT_SOURCE_DIR}/options.cmake)
endif ()

# Default C/CXX flags.
if (UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fpic -fPIC -D_LARGEFILE64_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -fpic -fPIC -D_LARGEFILE64_SOURCE")
    if (NOT WITHOUTGDB)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -ggdb")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -ggdb")
    endif ()
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O1 -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O1 -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
    string(REPLACE "-O3" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "-O3" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
elseif (MSVC)
    add_definitions(-DNOMINMAX)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /MP /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /MP")
endif ()

# To be compatible with low version Linux.
if (ENABLE_COMPATIBLE_MODE)
    message(STATUS "Enable compatible mode.")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c90")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--wrap=memcpy")
    add_definitions(-DENABLE_COMPATIBLE_MODE)
endif ()

if (ANDROID OR CMAKE_BUILD_TYPE MATCHES Debug)
    set(NO_TCMALLOC TRUE)
    add_definitions(-DLOGTAIL_NO_TC_MALLOC)
endif ()

# Dependencies.
include(${CMAKE_CURRENT_SOURCE_DIR}/utils.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/dependencies.cmake)

# Subdirectories (modules).
set(SUB_DIRECTORIES_LIST
        aggregator application app_config checkpoint common config config/provider config/watcher config_manager config_server_pb 
        container_manager controller event event_handler event_listener file_server flusher go_pipeline helper input log_pb logger 
        models monitor parser pipeline plugin plugin/creator plugin/instance plugin/interface polling processor processor/daemon processor/inner
        profile_sender queue reader sdk sender sls_control fuse
        )
if (LINUX)
    set(SUB_DIRECTORIES_LIST ${SUB_DIRECTORIES_LIST} observer spl)
    if (ENABLE_ENTERPRISE)
        set(SUB_DIRECTORIES_LIST ${SUB_DIRECTORIES_LIST} shennong streamlog)
    endif()
elseif(MSVC)
    if (ENABLE_ENTERPRISE)
        set(SUB_DIRECTORIES_LIST ${SUB_DIRECTORIES_LIST} daemon)
    endif()
endif ()
if (ENABLE_ENTERPRISE)
    set(SUB_DIRECTORIES_LIST ${SUB_DIRECTORIES_LIST} config_sdk)
endif()

# Collect source files for UT.
set(ALL_SOURCE_FILES "")
macro(append_source_files source_files)
    set(ALL_SOURCE_FILES ${ALL_SOURCE_FILES} ${${source_files}} PARENT_SCOPE)
endmacro()
# Module includes & add_subdirectory.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories("/opt/logtail_spl/include")
foreach (DIR_NAME ${SUB_DIRECTORIES_LIST})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME})
endforeach (DIR_NAME)
foreach (DIR_NAME ${SUB_DIRECTORIES_LIST})
    add_subdirectory(${DIR_NAME})
endforeach (DIR_NAME)

# Logtail executable.
if (BUILD_LOGTAIL)
    if (ENABLE_ENTERPRISE)
        if (UNIX)
            add_executable(${LOGTAIL_TARGET} enterprise_logtail.cpp)
        elseif (MSVC)
            add_executable(${LOGTAIL_TARGET} enterprise_logtail_windows.cpp)
        endif ()
    else ()
        if (UNIX)
            add_executable(${LOGTAIL_TARGET} logtail.cpp)
        elseif (MSVC)
            add_executable(${LOGTAIL_TARGET} logtail_windows.cpp)
        endif ()
    endif()
    target_link_libraries(${LOGTAIL_TARGET} application)
    if (ENABLE_COMPATIBLE_MODE)
        target_link_libraries(${LOGTAIL_TARGET} rt -static-libstdc++ -static-libgcc)
    endif ()
    if (ENABLE_STATIC_LINK_CRT)
        target_link_libraries(${LOGTAIL_TARGET} -static-libstdc++ -static-libgcc)
    endif ()
endif()

if (BUILD_LOGTAIL_SHARED_LIBRARY)
    if (ENABLE_ENTERPRISE)
        if (UNIX)
            add_library(${LOGTAIL_TARGET} SHARED enterprise_logtail.cpp)
        elseif (MSVC)
            add_library(${LOGTAIL_TARGET} SHARED enterprise_logtail_windows.cpp)
        endif ()
    else ()
        if (UNIX)
            add_library(${LOGTAIL_TARGET} SHARED logtail.cpp)
        elseif (MSVC)
            add_library(${LOGTAIL_TARGET} SHARED logtail_windows.cpp)
        endif ()
    endif()
    target_link_libraries(${LOGTAIL_TARGET} application)
    if (ENABLE_COMPATIBLE_MODE)
        target_link_libraries(${LOGTAIL_TARGET} rt -static-libstdc++ -static-libgcc)
    endif ()
    if (ENABLE_STATIC_LINK_CRT)
        target_link_libraries(${LOGTAIL_TARGET} -static-libstdc++ -static-libgcc)
    endif ()
endif ()

# Logtail UT.
if (BUILD_LOGTAIL_UT)
    message(STATUS "Build unittest.")
    include(CTest)
    enable_testing()
    add_subdirectory(unittest)
endif ()
