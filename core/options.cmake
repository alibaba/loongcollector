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

# Name/Version information.
if (NOT DEFINED LOGTAIL_VERSION)
    set(LOGTAIL_VERSION "0.0.1")
endif ()
message(STATUS "Version: ${LOGTAIL_VERSION}")

set(LOGTAIL_TARGET "loongcollector")

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

set(VERSION_CPP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/common/Version.cpp)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/Version.cpp.in ${VERSION_CPP_FILE})
cmake_policy(SET CMP0074 NEW)
