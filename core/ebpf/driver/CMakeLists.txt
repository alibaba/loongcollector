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
    set(ENABLE_PROFILE OFF)
    set(BUILD_TESTING OFF)
    set(CMAKE_INSTALL_PREFIX ${DEFAULT_DEPS_ROOT})
    add_subdirectory(${CMAKE_SOURCE_DIR}/_thirdparty/coolbpf ${CMAKE_BINARY_DIR}/_thirdparty/coolbpf)
    add_custom_target(
        install_coolbpf ALL
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}/_thirdparty/coolbpf
        DEPENDS coolbpf coolbpf_static
    )

    include_directories("${DEPS_INCLUDE_ROOT}/coolbpf")

    file(GLOB LIB_SOURCE_FILES *.cpp *.h)
    if (BUILD_LOGTAIL_UT)
        add_definitions(-DAPSARA_UNIT_TEST_MAIN)
    endif ()

    add_library(${EBPF_DIRVER_TARGET} SHARED ${LIB_SOURCE_FILES})
    target_link_options(${EBPF_DIRVER_TARGET} PRIVATE "-Wl,-rpath=.:${DEFAULT_DEPS_ROOT}/lib")
    target_link_libraries(${EBPF_DIRVER_TARGET} coolbpf)
    add_dependencies(${EBPF_DIRVER_TARGET} install_coolbpf)
    target_compile_options(${EBPF_DIRVER_TARGET} PRIVATE -fPIC -Werror)
endif ()
