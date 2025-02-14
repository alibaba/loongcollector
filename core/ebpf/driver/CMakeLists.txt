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
project(driver)

if (LINUX)
    # include(ExternalProject)

    # ExternalProject_Add(coolbpf_proj
    #         PREFIX "${EXTERNAL_INSTALL_DIR}"
    #         SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/coolbpf"
    #         CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${DEFAULT_DEPS_ROOT} -DENABLE_PROFILE=OFF -DBUILD_TESTING=OFF
    # )
    set(ENABLE_PROFILE OFF)
    set(BUILD_TESTING OFF)
    set(CMAKE_INSTALL_PREFIX ${DEFAULT_DEPS_ROOT})
    add_subdirectory(coolbpf)
    add_custom_target(
        install_coolbpf ALL
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
        DEPENDS coolbpf coolbpf_static
    )

    include_directories("${DEPS_INCLUDE_ROOT}/coolbpf")

    file(GLOB LIB_SOURCE_FILES *.cpp *.h)

    add_library(EbpfDriver SHARED ${LIB_SOURCE_FILES})
    target_link_libraries(EbpfDriver coolbpf)
    add_dependencies(EbpfDriver install_coolbpf)
    target_link_options(EbpfDriver PRIVATE "-Wl,-rpath=.:${DEFAULT_DEPS_ROOT}/lib")
endif ()
