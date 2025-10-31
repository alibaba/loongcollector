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

macro(all_link target_name)
    if (BUILD_LOGTAIL_UT)
        link_gtest(${target_name})
    endif ()
    link_cityhash(${target_name})
    link_leveldb(${target_name})
    link_curl(${target_name})
    link_tcmalloc(${target_name})
    link_grpc(${target_name})
    link_ssl(${target_name}) # must after link_spl
    link_crypto(${target_name}) # must after link_spl
    if (NOT ENABLE_ENTERPRISE AND UNIX)
        link_rdkafka(${target_name})
    endif()
    if (UNIX)
        target_link_libraries(${target_name} dl)
        if (ENABLE_COMPATIBLE_MODE)
            target_link_libraries(${target_name} rt -static-libstdc++ -static-libgcc)
        endif ()
        if (ENABLE_STATIC_LINK_CRT)
            target_link_libraries(${target_name} -static-libstdc++ -static-libgcc)
        endif ()
    elseif (MSVC)
        target_link_libraries(${target_name} "Psapi.lib")
    endif ()
endmacro()

macro(link_systemd_journal target_name)
    if (LINUX)
        # 使用 pkg-config 查找 libsystemd
        find_package(PkgConfig REQUIRED)
        pkg_search_module(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

        if (SYSTEMD_FOUND)
            # 链接 libsystemd（现代方式）
            target_link_libraries(${target_name} PkgConfig::SYSTEMD)
            target_include_directories(${target_name} PRIVATE ${SYSTEMD_INCLUDE_DIRS})
            message(STATUS "Linked with libsystemd via pkg-config: ${SYSTEMD_LIBRARIES}")
        else()
            message(FATAL_ERROR "libsystemd not found. Please install libsystemd-dev or systemd-devel.")
        endif()
    endif()
endmacro()
