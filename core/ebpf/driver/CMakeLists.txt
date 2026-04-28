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
    set(ENABLE_PROFILE OFF CACHE BOOL "Enable profile" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Build test cases" FORCE)
    set(CMAKE_INSTALL_PREFIX ${DEFAULT_DEPS_ROOT})
    
    # Save the original C flags
    set(ORIGINAL_CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
    
    # Remove -Werror for coolbpf compilation (only if ENABLE_WERROR is ON)
    if (ENABLE_WERROR)
        string(REPLACE "-Werror" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    endif()

    # LoongCollector Linux: build AgentSight (cargo → libagentsight.so + agentsight.h) with coolbpf.
    # Default ON in core/CMakeLists.txt; this keeps the cache entry consistent before coolbpf's option().
    set(ENABLE_AGENTSIGHT ON CACHE BOOL "Build AgentSight library (libagentsight.so + agentsight.h)")

    # Now add the coolbpf subdirectory (adds agentsight/ when ENABLE_AGENTSIGHT is on).
    add_subdirectory(${CMAKE_SOURCE_DIR}/_thirdparty/coolbpf ${CMAKE_BINARY_DIR}/_thirdparty/coolbpf)

    # coolbpf AgentSight: vendored openssl-sys runs nested `make` and breaks when GNU make forwards a
    # jobserver (parallel top-level make). Prefer not to patch coolbpf/src/agentsight/CMakeLists.txt;
    # USES_TERMINAL reduces jobserver handoff into the cargo/openssl make chain (CMake 3.2+).
    if (TARGET libagentsight)
        set_target_properties(libagentsight PROPERTIES USES_TERMINAL TRUE)
    endif ()

    # Restore the original C flags for the rest of the build
    set(CMAKE_C_FLAGS ${ORIGINAL_CMAKE_C_FLAGS})
    
    set(_coolbpf_install_deps coolbpf coolbpf_static)
    # $<TARGET_FILE:...> only works for SHARED/MODULE (and EXECUTABLE). coolbpf may define a stub
    # INTERFACE/OBJECT target named libagentsight; skip install/copy hooks in that case.
    set(_libagentsight_file_target "")
    if (TARGET libagentsight)
        get_target_property(_as_type libagentsight TYPE)
        set(_as_file_target libagentsight)
        if (_as_type STREQUAL "ALIAS_LIBRARY")
            get_target_property(_as_aliased libagentsight ALIASED_TARGET)
            if (_as_aliased AND TARGET ${_as_aliased})
                set(_as_file_target ${_as_aliased})
                get_target_property(_as_type ${_as_file_target} TYPE)
            endif ()
        endif ()
        if (_as_type STREQUAL "SHARED_LIBRARY" OR _as_type STREQUAL "MODULE_LIBRARY")
            set(_libagentsight_file_target "${_as_file_target}")
        elseif (ENABLE_AGENTSIGHT AND NOT (_as_type STREQUAL "UTILITY"))
            message(
                STATUS
                "AgentSight: target 'libagentsight' has TYPE=${_as_type}; expected SHARED/MODULE/UTILITY(cargo). "
                "Skipping install dep and copy beside driver.")
        endif ()
    endif ()
    # coolbpf installs libagentsight.so via install(FILES ...): the producer may be UTILITY (cargo) or a real SHARED
    # target. Either way install_coolbpf must depend on it so `cmake --install` runs after the .so exists.
    if (ENABLE_AGENTSIGHT AND TARGET libagentsight)
        list(APPEND _coolbpf_install_deps libagentsight)
    elseif (ENABLE_AGENTSIGHT)
        message(STATUS "ENABLE_AGENTSIGHT=ON but no target 'libagentsight' in coolbpf; install_coolbpf skips it.")
    endif ()
    add_custom_target(
        install_coolbpf ALL
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}/_thirdparty/coolbpf
        DEPENDS ${_coolbpf_install_deps}
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
    target_compile_options(${EBPF_DIRVER_TARGET} PRIVATE -fPIC)

    # Extra copy beside libeBPFDriver.so in the build tree (core/build/ebpf/driver), in addition
    # to install_coolbpf -> CMAKE_INSTALL_PREFIX (default /opt/logtail/deps/lib).
    # Cannot use TARGET coolbpf POST_BUILD here: coolbpf is defined in a subdirectory (CMake policy).
    set(_coolbpf_soname_stamp "${CMAKE_CURRENT_BINARY_DIR}/.coolbpf_soname_copied_to_driver")
    add_custom_command(
        OUTPUT ${_coolbpf_soname_stamp}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_SONAME_FILE:coolbpf>
                ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${_coolbpf_soname_stamp}
        DEPENDS coolbpf
        COMMENT "Copy versioned libcoolbpf.so.* next to libeBPFDriver in build tree"
        VERBATIM)
    set(_copy_ebpf_dep_stamps "${_coolbpf_soname_stamp}")
    if (_libagentsight_file_target)
        set(_libagentsight_stamp "${CMAKE_CURRENT_BINARY_DIR}/.libagentsight_copied_to_driver")
        add_custom_command(
            OUTPUT ${_libagentsight_stamp}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:${_libagentsight_file_target}>
                    ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E touch ${_libagentsight_stamp}
            DEPENDS ${_libagentsight_file_target}
            COMMENT "Copy libagentsight next to libeBPFDriver in build tree"
            VERBATIM)
        list(APPEND _copy_ebpf_dep_stamps "${_libagentsight_stamp}")
    elseif (ENABLE_AGENTSIGHT AND TARGET libagentsight)
        # coolbpf: add_custom_target + cargo -> UTILITY; no $<TARGET_FILE:...>. Same path as coolbpf install(FILES ...).
        set(_agentsight_cargo_so "${CMAKE_BINARY_DIR}/_thirdparty/coolbpf/src/agentsight/release/libagentsight.so")
        set(_libagentsight_stamp "${CMAKE_CURRENT_BINARY_DIR}/.libagentsight_copied_to_driver")
        add_custom_command(
            OUTPUT ${_libagentsight_stamp}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_agentsight_cargo_so}
                    ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E touch ${_libagentsight_stamp}
            DEPENDS libagentsight
            COMMENT "Copy cargo-built libagentsight.so next to libeBPFDriver in build tree"
            VERBATIM)
        list(APPEND _copy_ebpf_dep_stamps "${_libagentsight_stamp}")
    endif ()
    # Sync cbindgen header next to the plugin (gitignored) for IDE / diff; compile still uses include path.
    if (ENABLE_AGENTSIGHT AND TARGET libagentsight)
        set(_agentsight_hdr_src "${CMAKE_SOURCE_DIR}/_thirdparty/coolbpf/src/agentsight/include/agentsight.h")
        set(_agentsight_hdr_plugin "${CMAKE_SOURCE_DIR}/ebpf/plugin/agentsight/agentsight_from_coolbpf.h")
        set(_agentsight_hdr_sync_stamp "${CMAKE_CURRENT_BINARY_DIR}/.agentsight_header_synced")
        add_custom_command(
            OUTPUT ${_agentsight_hdr_sync_stamp}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_agentsight_hdr_src} ${_agentsight_hdr_plugin}
            COMMAND ${CMAKE_COMMAND} -E touch ${_agentsight_hdr_sync_stamp}
            DEPENDS libagentsight
            COMMENT "Sync coolbpf agentsight.h next to ebpf/plugin/agentsight"
            VERBATIM)
        list(APPEND _copy_ebpf_dep_stamps "${_agentsight_hdr_sync_stamp}")
    endif ()
    add_custom_target(_loongcollector_copy_ebpf_deps_to_driver DEPENDS ${_copy_ebpf_dep_stamps})
    add_dependencies(${EBPF_DIRVER_TARGET} _loongcollector_copy_ebpf_deps_to_driver)
endif ()
