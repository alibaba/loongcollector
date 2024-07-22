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
include_directories(input)

# Add source files
file(GLOB THIS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/input/*.c ${CMAKE_SOURCE_DIR}/input/*.cc ${CMAKE_SOURCE_DIR}/input/*.cpp ${CMAKE_SOURCE_DIR}/input/*.h)
list(APPEND THIS_SOURCE_FILES_LIST ${THIS_SOURCE_FILES})

if(MSVC)
    # remove observer related files in input
    list(REMOVE_ITEM THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/input/InputObserverNetwork.cpp ${CMAKE_SOURCE_DIR}/input/InputObserverNetwork.h)
    if (ENABLE_ENTERPRISE)
        list(REMOVE_ITEM THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/input/InputStream.cpp ${CMAKE_SOURCE_DIR}/input/InputStream.h)
    endif ()
elseif(UNIX)
    if (NOT LINUX)
        # remove observer related files in input
        list(REMOVE_ITEM THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/input/InputObserverNetwork.cpp ${CMAKE_SOURCE_DIR}/input/InputObserverNetwork.h)
        # remove inputStream in input
        if (ENABLE_ENTERPRISE)
            list(REMOVE_ITEM THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/input/InputStream.cpp ${CMAKE_SOURCE_DIR}/input/InputStream.h)
        endif ()
    endif()
endif()

# Set source files to parent
set(PLUGIN_SOURCE_FILES_CORE ${PLUGIN_SOURCE_FILES_CORE} ${THIS_SOURCE_FILES_LIST})
