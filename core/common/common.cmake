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
# This file is used to collect all source files in common directory

# Add include directory
include_directories(common)

# Add source files
file(GLOB THIS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/common/*.c ${CMAKE_SOURCE_DIR}/common/*.cc ${CMAKE_SOURCE_DIR}/common/*.cpp ${CMAKE_SOURCE_DIR}/common/*.h)
list(APPEND THIS_SOURCE_FILES_LIST ${THIS_SOURCE_FILES})
# add xxhash in common
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    file(GLOB XX_HASH_SOURCE_FILES ${CMAKE_SOURCE_DIR}/common/xxhash/*.c ${CMAKE_SOURCE_DIR}/common/xxhash/*.h)
else ()
    file(GLOB XX_HASH_SOURCE_FILES ${CMAKE_SOURCE_DIR}/common/xxhash/xxhash.c ${CMAKE_SOURCE_DIR}/common/xxhash/xxhash.h)
endif ()
list(APPEND THIS_SOURCE_FILES_LIST ${XX_HASH_SOURCE_FILES})
# add memory in common
list(APPEND THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/common/memory/SourceBuffer.h)
list(APPEND THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/common/http/AsynCurlRunner.cpp ${CMAKE_SOURCE_DIR}/common/http/Curl.cpp ${CMAKE_SOURCE_DIR}/common/http/HttpResponse.cpp)
list(APPEND THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/common/timer/Timer.cpp ${CMAKE_SOURCE_DIR}/common/timer/HttpRequestTimerEvent.cpp)
# remove several files in common
list(REMOVE_ITEM THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/common/BoostRegexValidator.cpp ${CMAKE_SOURCE_DIR}/common/GetUUID.cpp)

if(MSVC)
    # remove LinuxDaemonUtil in common
    if (ENABLE_ENTERPRISE)
        list(REMOVE_ITEM THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/common/LinuxDaemonUtil.h ${CMAKE_SOURCE_DIR}/common/LinuxDaemonUtil.cpp)
    endif()
elseif(UNIX)
    if (LINUX)
        # needed by observer in common
        file(GLOB PICOHTTPPARSER_SOURCE_FILES ${CMAKE_SOURCE_DIR}/common/protocol/picohttpparser/*.c ${CMAKE_SOURCE_DIR}/common/protocol/picohttpparser/*.h)
        list(APPEND THIS_SOURCE_FILES_LIST ${PICOHTTPPARSER_SOURCE_FILES})
    endif()
endif()

# Set source files to parent
set(PLUGIN_SOURCE_FILES_CORE ${PLUGIN_SOURCE_FILES_CORE} ${THIS_SOURCE_FILES_LIST})
