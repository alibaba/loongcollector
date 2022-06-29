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

cmake_minimum_required (VERSION 2.8)
project (logtail)

# Options.
option (BUILD_LOGTAIL_UT "Build unit test for Logtail" OFF)
option (ENABLE_COMPATIBLE_MODE "Build Logtail in compatible mode (for low version Linux)" ON)
option (ENABLE_STATIC_LINK_CRT "Build Logtail by linking CRT statically" ON)

# Name/Version information.
if (NOT DEFINED LOGTAIL_VERSION)
    set (LOGTAIL_VERSION "1.1.0")
endif ()
message(STATUS "Version: ${LOGTAIL_VERSION}")

# Extract Git commit information for tracing.
# For a better solution see https://jonathanhamberg.com/post/cmake-embedding-git-hash/ but this is simple and easy.
find_package(Git)
# Error is just ignored and output will be empty at runtime.
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%H
  WORKING_DIRECTORY "${FLB_ROOT}"
  OUTPUT_VARIABLE LOGTAIL_GIT_HASH
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Git hash: ${LOGTAIL_GIT_HASH}")

string(TIMESTAMP LOGTAIL_BUILD_DATE "%Y%m%d")
message(STATUS "Build date: ${LOGTAIL_BUILD_DATE}")

set (LOGTAIL_TARGET      "ilogtail")
set (VERSION_CPP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/common/Version.cpp)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/common/Version.cpp.in ${VERSION_CPP_FILE})

# Default C/CXX flags.
if (UNIX)
	set (CMAKE_CXX_FLAGS "-Wno-error=deprecated-declarations -Wno-deprecated-declarations ")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -ggdb -Wall -fpic -fPIC -D_LARGEFILE64_SOURCE")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -ggdb -Wall -fpic -fPIC -D_LARGEFILE64_SOURCE")
    set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -ggdb -O0 -Wall -fpic -fPIC -D_LARGEFILE64_SOURCE")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -ggdb -O0 -Wall -fpic -fPIC -D_LARGEFILE64_SOURCE")
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -Wall -fpic -fPIC -D_LARGEFILE64_SOURCE")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall -fpic -fPIC -D_LARGEFILE64_SOURCE")
    string (REPLACE "-O3" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string (REPLACE "-O3" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
elseif (MSVC)
    add_definitions (-DNOMINMAX)
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /MP /Zi")
    set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set (CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /MP")
endif()

# To be compatible with low version Linux (-DENABLE_COMPATIBLE_MODE=ON).
if (ENABLE_COMPATIBLE_MODE)
    if (UNIX)
        message (STATUS "Enable compatible mode.")
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c90")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--wrap=memcpy")
        add_definitions (-DENABLE_COMPATIBLE_MODE)
    endif()
endif()

# Dependencies.
include (${CMAKE_CURRENT_SOURCE_DIR}/utils.cmake)
include (${CMAKE_CURRENT_SOURCE_DIR}/dependencies.cmake)

# Subdirectories (modules).
set (SUB_DIRECTORIES_LIST
    aggregator app_config checkpoint common config config_manager
    controller event event_handler event_listener helper log_pb logger monitor
    parser plugin polling processor profiler reader profile_sender sender shennong sdk
    fuse sls_control
)
if (UNIX)
    set (SUB_DIRECTORIES_LIST ${SUB_DIRECTORIES_LIST} streamlog)
endif ()

# Collect source files for UT.
set (ALL_SOURCE_FILES "")
macro (append_source_files source_files)
    set (ALL_SOURCE_FILES ${ALL_SOURCE_FILES} ${${source_files}} PARENT_SCOPE)
endmacro ()
# Module includes & add_subdirectory.
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
foreach (DIR_NAME ${SUB_DIRECTORIES_LIST})
    include_directories (${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME})
endforeach (DIR_NAME)
foreach (DIR_NAME ${SUB_DIRECTORIES_LIST})
    add_subdirectory (${DIR_NAME})
endforeach (DIR_NAME)

# Logtail executable.
if (UNIX)
	add_executable (${LOGTAIL_TARGET} logtail.cpp)
elseif (MSVC)
	add_executable (${LOGTAIL_TARGET} logtail_windows.cpp)
endif ()
target_link_libraries (${LOGTAIL_TARGET}
    common
    logger
    config_manager
    checkpoint
    processor
    controller
    plugin
    monitor
    sender
    profiler
    profile_sender
    app_config
    sls_control
)
if (UNIX)
    target_link_libraries (${LOGTAIL_TARGET} streamlog)
    target_link_libraries (${LOGTAIL_TARGET} pthread dl uuid)
    if (ENABLE_STATIC_LINK_CRT)
        target_link_libraries (${LOGTAIL_TARGET} -static-libstdc++ -static-libgcc)
    endif ()
    if (ENABLE_COMPATIBLE_MODE)
        target_link_libraries (${LOGTAIL_TARGET} rt)
        target_link_libraries (${LOGTAIL_TARGET} pthread dl uuid -static-libstdc++ -static-libgcc)
    else ()
        target_link_libraries (${LOGTAIL_TARGET} pthread dl uuid)
    endif ()
    link_ssl    (${LOGTAIL_TARGET})
    link_crypto (${LOGTAIL_TARGET})
endif ()

# Logtail UT.
if (BUILD_LOGTAIL_UT)
    message (STATUS "Build unittest.")
    add_subdirectory (unittest)
endif ()
