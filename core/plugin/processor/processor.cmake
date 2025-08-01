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
include_directories(processor)

# Add source files
file(GLOB THIS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/plugin/processor/*.c ${CMAKE_SOURCE_DIR}/plugin/processor/*.cc ${CMAKE_SOURCE_DIR}/plugin/processor/*.cpp ${CMAKE_SOURCE_DIR}/plugin/processor/*.h)
list(APPEND THIS_SOURCE_FILES_LIST ${THIS_SOURCE_FILES})
# add processor/inner
file(GLOB THIS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/plugin/processor/inner/*.c ${CMAKE_SOURCE_DIR}/plugin/processor/inner/*.cc ${CMAKE_SOURCE_DIR}/plugin/processor/inner/*.cpp ${CMAKE_SOURCE_DIR}/plugin/processor/inner/*.h)
if (MSVC)
    list(REMOVE_ITEM THIS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/plugin/processor/inner/ProcessorPromRelabelMetricNative.cpp)
    list(REMOVE_ITEM THIS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/plugin/processor/inner/ProcessorPromParseMetricNative.cpp)
    list(REMOVE_ITEM THIS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/plugin/processor/inner/ProcessorPromRelabelMetricNative.h)
    list(REMOVE_ITEM THIS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/plugin/processor/inner/ProcessorPromParseMetricNative.h)
endif()
list(APPEND THIS_SOURCE_FILES_LIST ${THIS_SOURCE_FILES})

# Set source files to parent
list(REMOVE_ITEM THIS_SOURCE_FILES_LIST ${CMAKE_SOURCE_DIR}/plugin/processor/ProcessorSPL.cpp ${CMAKE_SOURCE_DIR}/plugin/processor/ProcessorSPL.h)
set(PLUGIN_SOURCE_FILES_CORE ${PLUGIN_SOURCE_FILES_CORE} ${THIS_SOURCE_FILES_LIST})
set(PLUGIN_SOURCE_FILES_SPL ${PLUGIN_SOURCE_FILES_SPL} ${CMAKE_SOURCE_DIR}/plugin/processor/ProcessorSPL.cpp ${CMAKE_SOURCE_DIR}/plugin/processor/ProcessorSPL.h)
