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

# Set dependencies root, we assume that all dependencies are installed with following structure:
# - ${DEPS_ROOT}
#	- include: DEPS_INCLUDE_ROOT
#	- lib: DEPS_LIBRARY_ROOT
#	- bin: DEPS_BINARY_ROOT
# You can set your own ${DEPS_ROOT} when calling cmake, sub-directories can also be set with
# corresponding variables.
if (UNIX)
    set(DEFAULT_DEPS_ROOT "/opt/logtail/deps")
elseif (MSVC)
    set(DEFAULT_DEPS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../logtail_deps")
endif ()
logtail_define(DEPS_ROOT "Root directory for dependencies to install" ${DEFAULT_DEPS_ROOT})
logtail_define(DEPS_INCLUDE_ROOT "" "${DEPS_ROOT}/include")
logtail_define(DEPS_LIBRARY_ROOT "" "${DEPS_ROOT}/lib")
logtail_define(DEPS_BINARY_ROOT "" "${DEPS_ROOT}/bin")
include_directories("${DEPS_INCLUDE_ROOT}")
# Set library search path for find_library() - preferred over link_directories()
list(PREPEND CMAKE_LIBRARY_PATH "${DEPS_LIBRARY_ROOT}")

# Each dependency has three related variables can be set:
# - {dep_name}_INCLUDE_DIR
# - {dep_name}_LIBRARY_DIR
# - {dep_name}_LINK_OPTION: static link is used by default, you can set this variable to
#	library name only to change this behaviour.
set(INCLUDE_DIR_SUFFIX "INCLUDE_DIR")
set(LIBRARY_DIR_SUFFIX "LIBRARY_DIR")
set(LINK_OPTION_SUFFIX "LINK_OPTION")
# Dependencies list.
set(DEP_NAME_LIST
        boost
        cityhash
        crypto
        curl
        gflags
        gmock
        gtest
        jsoncpp
        leveldb
        lz4
        protobuf
        rapidjson               # header-only
        re2
        simdjson
        spdlog                  # header-only
        ssl                     # openssl
        unwind                  # google breakpad on Windows
        uuid
        yamlcpp
        zlib
        zstd
        )

if (NOT ENABLE_ENTERPRISE AND UNIX)
    list(APPEND DEP_NAME_LIST "rdkafka")
endif()

if (NOT NO_TCMALLOC)
    list(APPEND DEP_NAME_LIST "tcmalloc") # (gperftools)
endif()

if (MSVC)
    if (NOT DEFINED unwind_${INCLUDE_DIR_SUFFIX})
        set(unwind_${INCLUDE_DIR_SUFFIX} ${DEPS_INCLUDE_ROOT}/breakpad)
    endif ()
endif ()

# Set link options, add user-defined INCLUDE_DIR and LIBRARY_DIR.
foreach (DEP_NAME ${DEP_NAME_LIST})
    logtail_define(${DEP_NAME}_${LINK_OPTION_SUFFIX} "Link option for ${DEP_NAME}" "")

    if (${DEP_NAME}_${INCLUDE_DIR_SUFFIX})
        include_directories("${${DEP_NAME}_${INCLUDE_DIR_SUFFIX}}")
    endif ()

    if (${DEP_NAME}_${LIBRARY_DIR_SUFFIX})
        # Prefer CMAKE_LIBRARY_PATH for find_library over link_directories
        list(PREPEND CMAKE_LIBRARY_PATH "${${DEP_NAME}_${LIBRARY_DIR_SUFFIX}}")
    else ()
        set(${DEP_NAME}_${LIBRARY_DIR_SUFFIX} "${DEPS_LIBRARY_ROOT}")
    endif ()
endforeach (DEP_NAME)

# gtest
macro(link_gtest target_name)
    if (gtest_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${gtest_${LINK_OPTION_SUFFIX}}" "${gmock_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(GTEST_LIB libgtest.a PATHS "${gtest_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(GMOCK_LIB libgmock.a PATHS "${gmock_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(GTEST_LIB AND GMOCK_LIB)
            target_link_libraries(${target_name} "${GTEST_LIB}" "${GMOCK_LIB}")
        else()
            message(FATAL_ERROR "Could not find gtest or gmock libraries")
        endif()
    elseif (MSVC)
        find_library(GTEST_DEBUG_LIB gtestd.lib PATHS "${gtest_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(GTEST_RELEASE_LIB gtest.lib PATHS "${gtest_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(GMOCK_DEBUG_LIB gmockd.lib PATHS "${gmock_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(GMOCK_RELEASE_LIB gmock.lib PATHS "${gmock_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        
        if(GTEST_RELEASE_LIB AND GMOCK_RELEASE_LIB)
            if(GTEST_DEBUG_LIB AND GMOCK_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${GTEST_DEBUG_LIB}" "${GMOCK_DEBUG_LIB}"
                        optimized "${GTEST_RELEASE_LIB}" "${GMOCK_RELEASE_LIB}")
            else()
                # Use release libraries for both debug and release builds when debug libs are missing
                target_link_libraries(${target_name} "${GTEST_RELEASE_LIB}" "${GMOCK_RELEASE_LIB}")
            endif()
        elseif(GTEST_DEBUG_LIB AND GMOCK_DEBUG_LIB)
            # Fallback to debug libs when only debug is available
            target_link_libraries(${target_name} "${GTEST_DEBUG_LIB}" "${GMOCK_DEBUG_LIB}")
        else()
            message(FATAL_ERROR "Could not find gtest/gmock libraries (GTEST: d=${GTEST_DEBUG_LIB}; r=${GTEST_RELEASE_LIB}, GMOCK: d=${GMOCK_DEBUG_LIB}; r=${GMOCK_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

logtail_define(protobuf_BIN "Absolute path to protoc" "${DEPS_BINARY_ROOT}/protoc")

function(compile_proto PROTO_PATH OUTPUT_PATH PROTO_FILES)
    file(MAKE_DIRECTORY ${OUTPUT_PATH})
    execute_process(COMMAND ${protobuf_BIN} 
        --proto_path=${PROTO_PATH}
        --cpp_out=${OUTPUT_PATH}
        ${PROTO_FILES})
endfunction()

function(compile_proto_grpc PROTO_PATH OUTPUT_PATH PROTO_FILES)
    file(MAKE_DIRECTORY ${OUTPUT_PATH})
    execute_process(COMMAND ${protobuf_BIN}  
        --plugin=protoc-gen-grpc=${DEPS_BINARY_ROOT}/grpc_cpp_plugin
        -I=${PROTO_PATH}
        --cpp_out=${OUTPUT_PATH}
        --grpc_out=${OUTPUT_PATH}
        ${PROTO_FILES})
endfunction()

compile_proto(
    "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/sls"
    "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/sls"
    "sls_logs.proto;logtail_buffer_meta.proto;metric.proto;checkpoint.proto"
)

compile_proto(
    "${CMAKE_CURRENT_SOURCE_DIR}/../protobuf_public/models"
    "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/models"
    "log_event.proto;metric_event.proto;span_event.proto;pipeline_event_group.proto"
)

if (UNIX)
compile_proto_grpc(
    "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/forward"
    "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/forward"
    "loongsuite.proto"
)
endif()

compile_proto(
    "${CMAKE_CURRENT_SOURCE_DIR}/../config_server/protocol/v1"
    "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/config_server/v1"
    "agent.proto"
)

compile_proto(
    "${CMAKE_CURRENT_SOURCE_DIR}/../config_server/protocol/v2"
    "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/config_server/v2"
    "agentV2.proto"
)
# re2
macro(link_re2 target_name)
    if (re2_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${re2_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(RE2_LIB libre2.a PATHS "${re2_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(RE2_LIB)
            target_link_libraries(${target_name} "${RE2_LIB}")
        else()
            message(FATAL_ERROR "Could not find re2 library")
        endif()
    elseif (MSVC)
        find_library(RE2_DEBUG_LIB re2d.lib PATHS "${re2_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(RE2_RELEASE_LIB re2.lib PATHS "${re2_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(RE2_RELEASE_LIB)
            if(RE2_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${RE2_DEBUG_LIB}"
                        optimized "${RE2_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${RE2_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find re2 library (Debug: ${RE2_DEBUG_LIB}, Release: ${RE2_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# tcmalloc (gperftools)
macro(link_tcmalloc target_name)
    if(NOT NO_TCMALLOC)
        if (tcmalloc_${LINK_OPTION_SUFFIX})
            target_link_libraries(${target_name} "${tcmalloc_${LINK_OPTION_SUFFIX}}")
        elseif (UNIX)
            find_library(TCMALLOC_LIB libtcmalloc_minimal.a PATHS "${tcmalloc_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
            if(TCMALLOC_LIB)
                target_link_libraries(${target_name} "${TCMALLOC_LIB}")
            else()
                message(FATAL_ERROR "Could not find tcmalloc library")
            endif()
            # target_link_libraries(${target_name} "${tcmalloc_${LIBRARY_DIR_SUFFIX}}/libtcmalloc_and_profiler.a")
            elseif (MSVC)
        add_definitions(-DPERFTOOLS_DLL_DECL=)
        find_library(TCMALLOC_DEBUG_LIB libtcmalloc_minimald.lib PATHS "${tcmalloc_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(TCMALLOC_RELEASE_LIB libtcmalloc_minimal.lib PATHS "${tcmalloc_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(TCMALLOC_RELEASE_LIB)
            if(TCMALLOC_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${TCMALLOC_DEBUG_LIB}"
                        optimized "${TCMALLOC_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${TCMALLOC_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find tcmalloc library (Debug: ${TCMALLOC_DEBUG_LIB}, Release: ${TCMALLOC_RELEASE_LIB})")
        endif()
        endif ()
    endif ()
endmacro()

# cityhash
macro(link_cityhash target_name)
    if (cityhash_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${cityhash_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(CITYHASH_LIB libcityhash.a PATHS "${cityhash_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(CITYHASH_LIB)
            target_link_libraries(${target_name} "${CITYHASH_LIB}")
        else()
            message(FATAL_ERROR "Could not find cityhash library")
        endif()
    elseif (MSVC)
        find_library(CITYHASH_DEBUG_LIB cityhashd.lib PATHS "${cityhash_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(CITYHASH_RELEASE_LIB cityhash.lib PATHS "${cityhash_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(CITYHASH_RELEASE_LIB)
            if(CITYHASH_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${CITYHASH_DEBUG_LIB}"
                        optimized "${CITYHASH_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${CITYHASH_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find cityhash library (Debug: ${CITYHASH_DEBUG_LIB}, Release: ${CITYHASH_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# gflags
macro(link_gflags target_name)
    if (gflags_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${gflags_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(GFLAGS_LIB libgflags.a PATHS "${gflags_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(GFLAGS_LIB)
            target_link_libraries(${target_name} "${GFLAGS_LIB}")
        else()
            message(FATAL_ERROR "Could not find gflags library")
        endif()
    elseif (MSVC)
        find_library(GFLAGS_DEBUG_LIB gflags_staticd.lib PATHS "${gflags_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(GFLAGS_RELEASE_LIB gflags_static.lib PATHS "${gflags_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(GFLAGS_RELEASE_LIB)
            if(GFLAGS_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${GFLAGS_DEBUG_LIB}"
                        optimized "${GFLAGS_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${GFLAGS_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find gflags library (Debug: ${GFLAGS_DEBUG_LIB}, Release: ${GFLAGS_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# jsoncpp
macro(link_jsoncpp target_name)
    if (jsoncpp_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${jsoncpp_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(JSONCPP_LIB libjsoncpp.a PATHS "${jsoncpp_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(JSONCPP_LIB)
            target_link_libraries(${target_name} "${JSONCPP_LIB}")
        else()
            message(FATAL_ERROR "Could not find jsoncpp library")
        endif()
    elseif (MSVC)
        find_library(JSONCPP_DEBUG_LIB jsoncppd.lib PATHS "${jsoncpp_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(JSONCPP_RELEASE_LIB jsoncpp.lib PATHS "${jsoncpp_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(JSONCPP_RELEASE_LIB)
            if(JSONCPP_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${JSONCPP_DEBUG_LIB}"
                        optimized "${JSONCPP_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${JSONCPP_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find jsoncpp library (Debug: ${JSONCPP_DEBUG_LIB}, Release: ${JSONCPP_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# yamlcpp
macro(link_yamlcpp target_name)
    if (yamlcpp_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${yamlcpp_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(YAMLCPP_LIB libyaml-cpp.a PATHS "${yamlcpp_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(YAMLCPP_LIB)
            target_link_libraries(${target_name} "${YAMLCPP_LIB}")
        else()
            message(FATAL_ERROR "Could not find yaml-cpp library")
        endif()
    elseif (MSVC)
        find_library(YAMLCPP_DEBUG_LIB yaml-cppd.lib PATHS "${yamlcpp_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(YAMLCPP_RELEASE_LIB yaml-cpp.lib PATHS "${yamlcpp_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(YAMLCPP_RELEASE_LIB)
            if(YAMLCPP_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${YAMLCPP_DEBUG_LIB}"
                        optimized "${YAMLCPP_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${YAMLCPP_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find yaml-cpp library (Debug: ${YAMLCPP_DEBUG_LIB}, Release: ${YAMLCPP_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# boost
macro(link_boost target_name)
    if (boost_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${boost_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(BOOST_REGEX_LIB libboost_regex.a PATHS "${boost_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BOOST_THREAD_LIB libboost_thread.a PATHS "${boost_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BOOST_SYSTEM_LIB libboost_system.a PATHS "${boost_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BOOST_FILESYSTEM_LIB libboost_filesystem.a PATHS "${boost_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BOOST_CHRONO_LIB libboost_chrono.a PATHS "${boost_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        
        if(BOOST_REGEX_LIB AND BOOST_THREAD_LIB AND BOOST_SYSTEM_LIB AND BOOST_FILESYSTEM_LIB AND BOOST_CHRONO_LIB)
            target_link_libraries(${target_name}
                    "${BOOST_REGEX_LIB}"
                    "${BOOST_THREAD_LIB}"
                    "${BOOST_SYSTEM_LIB}"
                    "${BOOST_FILESYSTEM_LIB}"
                    "${BOOST_CHRONO_LIB}")
        else()
            message(FATAL_ERROR "Could not find one or more boost libraries")
        endif()
    elseif (MSVC)
        if (NOT DEFINED Boost_FOUND)
            set(Boost_USE_STATIC_LIBS ON)
            set(Boost_USE_MULTITHREADED ON)
            set(Boost_USE_STATIC_RUNTIME ON)
            find_package(Boost 1.59.0 REQUIRED COMPONENTS regex thread system filesystem)
        endif ()
        if (Boost_FOUND)
            include_directories(${Boost_INCLUDE_DIRS})
            link_directories(${Boost_LIBRARY_DIRS})
        endif ()
        target_link_libraries(${target_name} ${Boost_LIBRARIES})
    endif ()
endmacro()

# lz4
macro(link_lz4 target_name)
    if (lz4_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${lz4_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(LZ4_LIB liblz4.a PATHS "${lz4_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(LZ4_LIB)
            target_link_libraries(${target_name} "${LZ4_LIB}")
        else()
            message(FATAL_ERROR "Could not find lz4 library")
        endif()
    elseif (MSVC)
        find_library(LZ4_DEBUG_LIB liblz4_staticd.lib PATHS "${lz4_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(LZ4_RELEASE_LIB liblz4_static.lib PATHS "${lz4_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(LZ4_RELEASE_LIB)
            if(LZ4_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${LZ4_DEBUG_LIB}"
                        optimized "${LZ4_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${LZ4_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find lz4 library (Debug: ${LZ4_DEBUG_LIB}, Release: ${LZ4_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# zlib
macro(link_zlib target_name)
    if (zlib_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${zlib_${LINK_OPTION_SUFFIX}}")
    elseif(ANDROID)
        target_link_libraries(${target_name} z)
    elseif (UNIX)
        find_library(ZLIB_LIB libz.a PATHS "${zlib_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(ZLIB_LIB)
            target_link_libraries(${target_name} "${ZLIB_LIB}")
        else()
            message(FATAL_ERROR "Could not find zlib library")
        endif()
    elseif (MSVC)
        find_library(ZLIB_DEBUG_LIB zlibstaticd.lib PATHS "${zlib_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(ZLIB_RELEASE_LIB zlibstatic.lib PATHS "${zlib_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(ZLIB_RELEASE_LIB)
            if(ZLIB_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${ZLIB_DEBUG_LIB}"
                        optimized "${ZLIB_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${ZLIB_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find zlib library (Debug: ${ZLIB_DEBUG_LIB}, Release: ${ZLIB_RELEASE_LIB})")
        endif()
    endif ()
endmacro()


# zstd
macro(link_zstd target_name)
    if (zstd_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${zstd_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(ZSTD_LIB libzstd.a PATHS "${zstd_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(ZSTD_LIB)
            target_link_libraries(${target_name} "${ZSTD_LIB}")
        else()
            message(FATAL_ERROR "Could not find zstd library")
        endif()
    elseif (MSVC)
        find_library(ZSTD_DEBUG_LIB zstdstaticd.lib PATHS "${zstd_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(ZSTD_RELEASE_LIB zstdstatic.lib PATHS "${zstd_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(ZSTD_RELEASE_LIB)
            if(ZSTD_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${ZSTD_DEBUG_LIB}"
                        optimized "${ZSTD_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${ZSTD_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find zstd library (Debug: ${ZSTD_DEBUG_LIB}, Release: ${ZSTD_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# libcurl
macro(link_curl target_name)
    if (curl_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${curl_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(CURL_LIB libcurl.a PATHS "${curl_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(CURL_LIB)
            target_link_libraries(${target_name} "${CURL_LIB}")
        else()
            message(FATAL_ERROR "Could not find curl library")
        endif()
    elseif (MSVC)
        find_library(CURL_DEBUG_LIB libcurl-d.lib PATHS "${curl_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(CURL_RELEASE_LIB libcurl.lib PATHS "${curl_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(LIBEAY32_LIB libeay32.lib PATHS "${curl_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(SSLEAY32_LIB ssleay32.lib PATHS "${curl_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        
        if(CURL_RELEASE_LIB)
            if(CURL_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${CURL_DEBUG_LIB}"
                        optimized "${CURL_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${CURL_RELEASE_LIB}")
            endif()
            
            add_definitions(-DCURL_STATICLIB)
            
            if(LIBEAY32_LIB)
                target_link_libraries(${target_name} "${LIBEAY32_LIB}")
            else()
                target_link_libraries(${target_name} "libeay32")
            endif()
            
            if(SSLEAY32_LIB)
                target_link_libraries(${target_name} "${SSLEAY32_LIB}")
            else()
                target_link_libraries(${target_name} "ssleay32")
            endif()
            
            target_link_libraries(${target_name} "Ws2_32.lib")
            target_link_libraries(${target_name} "Wldap32.lib")
        else()
            message(FATAL_ERROR "Could not find curl library (Debug: ${CURL_DEBUG_LIB}, Release: ${CURL_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# libunwind & google breakpad
macro(link_unwind target_name)
    if (unwind_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${unwind_${LINK_OPTION_SUFFIX}}")
    elseif (ANDROID)
        # target_link_libraries(${target_name} "${unwind_${LIBRARY_DIR_SUFFIX}}/libunwindstack.a")
    elseif (UNIX)
        find_library(UNWIND_LIB libunwind.a PATHS "${unwind_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(UNWIND_LIB)
            target_link_libraries(${target_name} "${UNWIND_LIB}")
        else()
            message(FATAL_ERROR "Could not find unwind library")
        endif()
    elseif (MSVC)
        find_library(BREAKPAD_COMMON_DEBUG_LIB breakpad_commond.lib PATHS "${unwind_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BREAKPAD_COMMON_RELEASE_LIB breakpad_common.lib PATHS "${unwind_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BREAKPAD_CLIENT_DEBUG_LIB breakpad_crash_generation_clientd.lib PATHS "${unwind_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BREAKPAD_CLIENT_RELEASE_LIB breakpad_crash_generation_client.lib PATHS "${unwind_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BREAKPAD_HANDLER_DEBUG_LIB breakpad_exception_handlerd.lib PATHS "${unwind_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(BREAKPAD_HANDLER_RELEASE_LIB breakpad_exception_handler.lib PATHS "${unwind_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        
        if(BREAKPAD_COMMON_RELEASE_LIB AND BREAKPAD_CLIENT_RELEASE_LIB AND BREAKPAD_HANDLER_RELEASE_LIB)
            # Link common library
            if(BREAKPAD_COMMON_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${BREAKPAD_COMMON_DEBUG_LIB}"
                        optimized "${BREAKPAD_COMMON_RELEASE_LIB}")
            else()
                target_link_libraries(${target_name} "${BREAKPAD_COMMON_RELEASE_LIB}")
            endif()
            
            # Link client library
            if(BREAKPAD_CLIENT_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${BREAKPAD_CLIENT_DEBUG_LIB}"
                        optimized "${BREAKPAD_CLIENT_RELEASE_LIB}")
            else()
                target_link_libraries(${target_name} "${BREAKPAD_CLIENT_RELEASE_LIB}")
            endif()
            
            # Link handler library
            if(BREAKPAD_HANDLER_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${BREAKPAD_HANDLER_DEBUG_LIB}"
                        optimized "${BREAKPAD_HANDLER_RELEASE_LIB}")
            else()
                target_link_libraries(${target_name} "${BREAKPAD_HANDLER_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find one or more breakpad libraries (missing release versions)")
        endif()
    endif ()
endmacro()

# ssl
macro(link_ssl target_name)
    if (ssl_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${ssl_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(SSL_LIB libssl.a PATHS "${ssl_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(SSL_LIB)
            target_link_libraries(${target_name} "${SSL_LIB}")
        else()
            message(FATAL_ERROR "Could not find ssl library")
        endif()
    elseif (MSVC)
        #target_link_libraries (${target_name}
        #   debug "libcurl-d"
        #   optimized "libcurl")
    endif ()
endmacro()

# crypto
macro(link_crypto target_name)
    if (crypto_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${crypto_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(CRYPTO_LIB libcrypto.a PATHS "${crypto_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(CRYPTO_LIB)
            target_link_libraries(${target_name} "${CRYPTO_LIB}")
        else()
            message(FATAL_ERROR "Could not find crypto library")
        endif()
    elseif (MSVC)
        #target_link_libraries (${target_name}
        #   debug "libcurl-d"
        #   optimized "libcurl")
    endif ()
endmacro()

# leveldb
macro(link_leveldb target_name)
    if (UNIX)
        find_library(LEVELDB_LIB libleveldb.a PATHS "${leveldb_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(LEVELDB_LIB)
            target_link_libraries(${target_name} "${LEVELDB_LIB}")
        else()
            message(FATAL_ERROR "Could not find leveldb library")
        endif()
    elseif (MSVC)
        find_library(LEVELDB_DEBUG_LIB leveldbd.lib PATHS "${leveldb_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(LEVELDB_RELEASE_LIB leveldb.lib PATHS "${leveldb_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(LEVELDB_RELEASE_LIB)
            if(LEVELDB_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${LEVELDB_DEBUG_LIB}"
                        optimized "${LEVELDB_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${LEVELDB_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find leveldb library (Debug: ${LEVELDB_DEBUG_LIB}, Release: ${LEVELDB_RELEASE_LIB})")
        endif()
    endif ()
endmacro()

# asan for debug
macro(link_asan target_name)
    if (UNIX)
        target_compile_options(${target_name} PUBLIC -fsanitize=address)
        target_link_options(${target_name} PUBLIC -fsanitize=address -static-libasan)
    elseif(MSVC)
        target_compile_options(${target_name} PUBLIC /fsanitize=address)
        target_link_options(${target_name} PUBLIC /fsanitize=address)
    endif()
endmacro()

# uuid
macro(link_uuid target_name)
    if (uuid_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${uuid_${LINK_OPTION_SUFFIX}}")
    elseif (ANDROID)
        find_library(UUID_LIB libuuid.a PATHS "${uuid_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(UUID_LIB)
            target_link_libraries(${target_name} "${UUID_LIB}")
        else()
            message(FATAL_ERROR "Could not find uuid library")
        endif()
    elseif (UNIX)
        target_link_libraries(${target_name} uuid)
    endif ()
endmacro()

# grpc
macro(link_grpc target_name)
    if (UNIX)
        set(OPENSSL_USE_STATIC_LIBS ON)
        set(OPENSSL_ROOT_DIR ${DEFAULT_DEPS_ROOT})
        find_package(re2 QUIET PATHS ${DEPS_ROOT}/lib64/cmake/re2 NO_DEFAULT_PATH)
        if(NOT re2_FOUND)
            message(FATAL_ERROR "re2 not found, please upgrade your development image to compile!")
        endif()
        find_package(absl QUIET PATHS ${DEPS_ROOT}/lib64/cmake/absl NO_DEFAULT_PATH)
        if(NOT absl_FOUND)
            message(FATAL_ERROR "absl not found, please upgrade your development image to compile!")
        endif()
        find_package(utf8_range QUIET PATHS ${DEPS_ROOT}/lib64/cmake/utf8_range NO_DEFAULT_PATH)
        if(NOT utf8_range_FOUND)
            message(FATAL_ERROR "utf8_range not found, please upgrade your development image to compile!")
        endif()
        find_package(protobuf QUIET PATHS ${DEPS_ROOT}/lib64/cmake/protobuf NO_DEFAULT_PATH)
        if(NOT protobuf_FOUND)
            message(FATAL_ERROR "protobuf not found, please upgrade your development image to compile!")
        endif()
        find_package(gRPC QUIET PATHS ${DEPS_ROOT} NO_DEFAULT_PATH)
        if(NOT gRPC_FOUND)
            message(FATAL_ERROR "gRPC not found, please upgrade your development image to compile!")
        endif()
        target_link_libraries(${target_name} gRPC::grpc++ protobuf::libprotobuf utf8_range::utf8_range)
    elseif (MSVC)
        find_library(PROTOBUF_DEBUG_LIB libprotobufd.lib PATHS "${protobuf_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        find_library(PROTOBUF_RELEASE_LIB libprotobuf.lib PATHS "${protobuf_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(PROTOBUF_RELEASE_LIB)
            if(PROTOBUF_DEBUG_LIB)
                target_link_libraries(${target_name}
                        debug "${PROTOBUF_DEBUG_LIB}"
                        optimized "${PROTOBUF_RELEASE_LIB}")
            else()
                # Use release library for both debug and release builds
                target_link_libraries(${target_name} "${PROTOBUF_RELEASE_LIB}")
            endif()
        else()
            message(FATAL_ERROR "Could not find protobuf library (Debug: ${PROTOBUF_DEBUG_LIB}, Release: ${PROTOBUF_RELEASE_LIB})")
        endif()
    endif()
endmacro()


macro(link_simdjson target_name)
    if (simdjson_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${simdjson_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        find_library(SIMDJSON_LIB libsimdjson.a PATHS "${simdjson_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
        if(SIMDJSON_LIB)
            message(STATUS "Found simdjson library: ${SIMDJSON_LIB}")
            target_link_libraries(${target_name} "${SIMDJSON_LIB}")
        else()
            message(FATAL_ERROR "Could not find simdjson library")
        endif()
    endif ()
endmacro()


# rdkafka
macro(link_rdkafka target_name)
    if (rdkafka_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${rdkafka_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${rdkafka_${LIBRARY_DIR_SUFFIX}}/librdkafka.a")
        target_link_libraries(${target_name} "${rdkafka_${LIBRARY_DIR_SUFFIX}}/librdkafka++.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "rdkafkad"
                optimized "rdkafka")
        target_link_libraries(${target_name}
                debug "rdkafka++d"
                optimized "rdkafka++")
    endif ()
endmacro()

macro(link_spl target_name)
    logtail_define(spl_${target_name} "" "")

    find_library(LIB_FOUND "libloongcollector_spl_2.1.9.a")
    if (NOT LIB_FOUND)
        message(FATAL_ERROR "Please upgrade your development image to compile!")
    endif()
    
    target_link_libraries(${target_name} "libloongcollector_spl_2.1.9.a")
    target_link_libraries(${target_name} "libx.a")
    target_link_libraries(${target_name} "libxx.a")
    target_link_libraries(${target_name} "libpresto_common.a")
    target_link_libraries(${target_name} "libpresto_exception.a")
    target_link_libraries(${target_name} "libpresto_http.a")
    target_link_libraries(${target_name} "libpresto_log_rotate.a")
    target_link_libraries(${target_name} "libpresto_prometheus.a")
    target_link_libraries(${target_name} "libpresto_server_lib.a")
    target_link_libraries(${target_name} "libpresto_sls_lib.a")
    target_link_libraries(${target_name} "libpresto_sls_rpc.a")
    target_link_libraries(${target_name} "libpresto_sls_rpc_protocol.a")
    target_link_libraries(${target_name} "libpresto_thrift-cpp2.a")
    target_link_libraries(${target_name} "libpresto_thrift_extra.a")
    target_link_libraries(${target_name} "libpresto_types.a")
    target_link_libraries(${target_name} "libvelox_tpch_connector.a")
    target_link_libraries(${target_name} "libvelox_hive_connector.a")
    target_link_libraries(${target_name} "libpresto_protocol.a")
    target_link_libraries(${target_name} "libpresto_type_converter.a")
    target_link_libraries(${target_name} "libpresto_operators.a")
    target_link_libraries(${target_name} "libvelox_exec.a")

    target_link_libraries(${target_name} "libvelox_functions_prestosql.a")
    target_link_libraries(${target_name} "libvelox_functions_prestosql_impl.a")
    target_link_libraries(${target_name} "libvelox_aggregates.a")
    target_link_libraries(${target_name} "libvelox_arrow_bridge.a")
    target_link_libraries(${target_name} "libvelox_buffer.a")
    target_link_libraries(${target_name} "libvelox_common_hyperloglog.a")
    target_link_libraries(${target_name} "libvelox_connector.a")
    target_link_libraries(${target_name} "libvelox_core.a")
    target_link_libraries(${target_name} "libvelox_config.a")
    target_link_libraries(${target_name} "libvelox_coverage_util.a")
    target_link_libraries(${target_name} "libvelox_dwio_catalog_fbhive.a")

    target_link_libraries(${target_name} "libvelox_dwio_dwrf_reader.a")
    target_link_libraries(${target_name} "libvelox_dwio_dwrf_writer.a")
    target_link_libraries(${target_name} "libvelox_dwio_dwrf_utils.a")
    target_link_libraries(${target_name} "libvelox_dwio_dwrf_common.a")
    target_link_libraries(${target_name} "libvelox_dwio_dwrf_proto.a")
    target_link_libraries(${target_name} "libvelox_dwio_common.a")
    target_link_libraries(${target_name} "libvelox_dwio_common_compression.a")
    target_link_libraries(${target_name} "libvelox_dwio_common_encryption.a")
    target_link_libraries(${target_name} "libvelox_dwio_common_exception.a")
    target_link_libraries(${target_name} "libvelox_dwio_parquet_reader.a")
    target_link_libraries(${target_name} "libvelox_dwio_parquet_writer.a")
    target_link_libraries(${target_name} "libvelox_encode.a")
    target_link_libraries(${target_name} "libvelox_exception.a")
    target_link_libraries(${target_name} "libvelox_presto_serializer.a")
    target_link_libraries(${target_name} "libvelox_caching.a")
    target_link_libraries(${target_name} "libvelox_expression.a")
    target_link_libraries(${target_name} "libvelox_expression_functions.a")
    target_link_libraries(${target_name} "libvelox_external_date.a")
    target_link_libraries(${target_name} "libvelox_file.a")
    target_link_libraries(${target_name} "libvelox_parse_expression.a")
    target_link_libraries(${target_name} "libvelox_function_registry.a")
    target_link_libraries(${target_name} "libvelox_functions_aggregates.a")
    target_link_libraries(${target_name} "libvelox_functions_json.a")
    target_link_libraries(${target_name} "libvelox_functions_lib.a")
    target_link_libraries(${target_name} "libvelox_functions_util.a")
    target_link_libraries(${target_name} "libvelox_functions_window.a")
    target_link_libraries(${target_name} "libvelox_functions_lib_date_time_formatter.a")
    target_link_libraries(${target_name} "libvelox_hive_partition_function.a")
    target_link_libraries(${target_name} "libvelox_is_null_functions.a")
    target_link_libraries(${target_name} "libvelox_memory.a")
    target_link_libraries(${target_name} "libvelox_status.a")

    target_link_libraries(${target_name} "libvelox_parse_parser.a")
    target_link_libraries(${target_name} "libvelox_parse_utils.a")
    target_link_libraries(${target_name} "libvelox_process.a")
    target_link_libraries(${target_name} "libvelox_row_fast.a")
    target_link_libraries(${target_name} "libvelox_rpc.a")
    target_link_libraries(${target_name} "libvelox_serialization.a")
    target_link_libraries(${target_name} "libvelox_time.a")
    target_link_libraries(${target_name} "libvelox_tpch_gen.a")
    target_link_libraries(${target_name} "libvelox_type.a")
    target_link_libraries(${target_name} "libvelox_type_calculation.a")
    target_link_libraries(${target_name} "libvelox_type_fbhive.a")
    target_link_libraries(${target_name} "libvelox_type_tz.a")
    target_link_libraries(${target_name} "libvelox_type_signature.a")
    target_link_libraries(${target_name} "libvelox_vector.a")
    target_link_libraries(${target_name} "libvelox_vector.a")
    target_link_libraries(${target_name} "libvelox_vector_fuzzer.a")
    target_link_libraries(${target_name} "libvelox_window.a")
    target_link_libraries(${target_name} "libvelox_common_base.a")
    target_link_libraries(${target_name} "libvelox_duckdb_parser.a")
    target_link_libraries(${target_name} "libvelox_duckdb_conversion.a")
    target_link_libraries(${target_name} "libvelox_signature_parser.a")
    target_link_libraries(${target_name} "libvelox_common_compression.a")

    target_link_libraries(${target_name} "libvelox_presto_types.a")
    target_link_libraries(${target_name} "libduckdb.a")
    target_link_libraries(${target_name} "libduckdb_utf8proc.a")
    target_link_libraries(${target_name} "libduckdb_fmt.a")
    target_link_libraries(${target_name} "libduckdb_fastpforlib.a")
    target_link_libraries(${target_name} "libduckdb_re2.a")
    target_link_libraries(${target_name} "libduckdb_miniz.a")
    target_link_libraries(${target_name} "libduckdb_pg_query.a")
    target_link_libraries(${target_name} "libduckdb_fsst.a")
    target_link_libraries(${target_name} "libduckdb_hyperloglog.a")
    target_link_libraries(${target_name} "libduckdb_mbedtls.a")
    target_link_libraries(${target_name} "libhttp_filters.a")
    target_link_libraries(${target_name} "libmd5.a")
    target_link_libraries(${target_name} "libsls_connector_proto.a")
    target_link_libraries(${target_name} "libsls_project_version.a")
    target_link_libraries(${target_name} "libjemalloc_extension.a")

    target_link_libraries(${target_name} "libproxygen.a")
    target_link_libraries(${target_name} "libproxygenhttpserver.a")
    target_link_libraries(${target_name} "libglog.a")
    target_link_libraries(${target_name} "libfolly.a")
    target_link_libraries(${target_name} "libuuid.a")
    target_link_libraries(${target_name} "libfmt.a")
    find_library(BOOST_CONTEXT_LIB libboost_context.a PATHS "${boost_${LIBRARY_DIR_SUFFIX}}" NO_DEFAULT_PATH)
    if(BOOST_CONTEXT_LIB)
        target_link_libraries(${target_name} "${BOOST_CONTEXT_LIB}")
    else()
        message(FATAL_ERROR "Could not find boost_context library")
    endif()

    target_link_libraries(${target_name} "libdouble-conversion.a")
    target_link_libraries(${target_name} "libsodium.a")
    target_link_libraries(${target_name} "libfizz.a")
    target_link_libraries(${target_name} "libwangle.a")
    target_link_libraries(${target_name} "libantlr4-runtime.a")
    target_link_libraries(${target_name} "libthriftcpp2.a")
    target_link_libraries(${target_name} "libthrift-core.a")
    target_link_libraries(${target_name} "libthriftprotocol.a")
    target_link_libraries(${target_name} "libthriftmetadata.a")
    target_link_libraries(${target_name} "libtransport.a")
    target_link_libraries(${target_name} "libprometheus-cpp-core.a")
    target_link_libraries(${target_name} "libprometheus-cpp-pull.a")
    target_link_libraries(${target_name} "libsnappy.a")
    target_link_libraries(${target_name} "libbz2.a")
    target_link_libraries(${target_name} "liblzo2.a")

    target_link_libraries(${target_name} "liby.a")
    target_link_libraries(${target_name} "libevent.a")
    target_link_libraries(${target_name} "libevent_pthreads.a")
    target_link_libraries(${target_name} "libapsara_common.a")
    target_link_libraries(${target_name} "libbuild_info.a")
    
endmacro()
