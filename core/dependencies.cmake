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
link_directories("${DEPS_LIBRARY_ROOT}")

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
        spdlog                  # header-only
        rapidjson               # header-only
        gtest
        gmock
        protobuf
        re2
        cityhash
        gflags
        jsoncpp
        yamlcpp
        boost
        lz4
        zlib
        zstd
        curl
        unwind                  # google breakpad on Windows
        ssl                     # openssl
        crypto
        leveldb
        uuid
        )

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
        link_directories("${${DEP_NAME}_${LIBRARY_DIR_SUFFIX}}")
    else ()
        set(${DEP_NAME}_${LIBRARY_DIR_SUFFIX} "${DEPS_LIBRARY_ROOT}")
    endif ()
endforeach (DEP_NAME)

# spdlog, replace implementation.
if (spdlog_${INCLUDE_DIR_SUFFIX})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/deps/spdlog/sinks/rotating_file_sink-inl.h
            DESTINATION "${spdlog_${INCLUDE_DIR_SUFFIX}}/spdlog/sinks")
else ()
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/deps/spdlog/sinks/rotating_file_sink-inl.h
            DESTINATION ${DEPS_INCLUDE_ROOT}/spdlog/sinks)
endif ()

# gtest
macro(link_gtest target_name)
    if (gtest_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${gtest_${LINK_OPTION_SUFFIX}}" "${gmock_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${gtest_${LIBRARY_DIR_SUFFIX}}/libgtest.a" "${gmock_${LIBRARY_DIR_SUFFIX}}/libgmock.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "gtestd"
                optimized "gtest"
                debug "gmockd"
                optimized "gmock")
    endif ()
endmacro()

# protobuf
macro(link_protobuf target_name)
    if (protobuf_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${protobuf_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${protobuf_${LIBRARY_DIR_SUFFIX}}/libprotobuf.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "libprotobufd"
                optimized "libprotobuf")
    endif ()
    if (ANDROID)
        target_link_libraries(${target_name} "log")
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
        target_link_libraries(${target_name} "${re2_${LIBRARY_DIR_SUFFIX}}/libre2.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "re2d"
                optimized "re2")
    endif ()
endmacro()

# tcmalloc (gperftools)
macro(link_tcmalloc target_name)
    if(NOT NO_TCMALLOC)
        if (tcmalloc_${LINK_OPTION_SUFFIX})
            target_link_libraries(${target_name} "${tcmalloc_${LINK_OPTION_SUFFIX}}")
        elseif (UNIX)
            target_link_libraries(${target_name} "${tcmalloc_${LIBRARY_DIR_SUFFIX}}/libtcmalloc_minimal.a")
        elseif (MSVC)
            add_definitions(-DPERFTOOLS_DLL_DECL=)
            target_link_libraries(${target_name}
                    debug "libtcmalloc_minimald"
                    optimized "libtcmalloc_minimal")
        endif ()
    endif ()
endmacro()

# cityhash
macro(link_cityhash target_name)
    if (cityhash_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${cityhash_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${cityhash_${LIBRARY_DIR_SUFFIX}}/libcityhash.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "cityhashd"
                optimized "cityhash")
    endif ()
endmacro()

# gflags
macro(link_gflags target_name)
    if (gflags_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${gflags_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${gflags_${LIBRARY_DIR_SUFFIX}}/libgflags.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "gflags_staticd"
                optimized "gflags_static")
    endif ()
endmacro()

# jsoncpp
macro(link_jsoncpp target_name)
    if (jsoncpp_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${jsoncpp_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${jsoncpp_${LIBRARY_DIR_SUFFIX}}/libjsoncpp.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "jsoncppd"
                optimized "jsoncpp")
    endif ()
endmacro()

# yamlcpp
macro(link_yamlcpp target_name)
    if (yamlcpp_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${yamlcpp_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${yamlcpp_${LIBRARY_DIR_SUFFIX}}/libyaml-cpp.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "yaml-cppd"
                optimized "yaml-cpp")
    endif ()
endmacro()

# boost
macro(link_boost target_name)
    if (boost_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${boost_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name}
                "${boost_${LIBRARY_DIR_SUFFIX}}/libboost_regex.a"
                "${boost_${LIBRARY_DIR_SUFFIX}}/libboost_thread.a"
                "${boost_${LIBRARY_DIR_SUFFIX}}/libboost_system.a"
                "${boost_${LIBRARY_DIR_SUFFIX}}/libboost_filesystem.a"
                "${boost_${LIBRARY_DIR_SUFFIX}}/libboost_chrono.a"
                )
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
        target_link_libraries(${target_name} "${lz4_${LIBRARY_DIR_SUFFIX}}/liblz4.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "liblz4_staticd"
                optimized "liblz4_static")
    endif ()
endmacro()

# zlib
macro(link_zlib target_name)
    if (zlib_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${zlib_${LINK_OPTION_SUFFIX}}")
    elseif(ANDROID)
        target_link_libraries(${target_name} z)
    elseif (UNIX)
        target_link_libraries(${target_name} "${zlib_${LIBRARY_DIR_SUFFIX}}/libz.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "zlibstaticd"
                optimized "zlibstatic")
    endif ()
endmacro()


# zstd
macro(link_zstd target_name)
    if (zstd_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${zstd_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${zstd_${LIBRARY_DIR_SUFFIX}}/libzstd.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "zstdstaticd"
                optimized "zstdstatic")
    endif ()
endmacro()

# libcurl
macro(link_curl target_name)
    if (curl_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${curl_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${curl_${LIBRARY_DIR_SUFFIX}}/libcurl.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "libcurl-d"
                optimized "libcurl")
        add_definitions(-DCURL_STATICLIB)
        target_link_libraries(${target_name}
                debug "libeay32"
                optimized "libeay32")
        target_link_libraries(${target_name}
                debug "ssleay32"
                optimized "ssleay32")
        target_link_libraries(${target_name} "Ws2_32.lib")
        target_link_libraries(${target_name} "Wldap32.lib")
    endif ()
endmacro()

# libunwind & google breakpad
macro(link_unwind target_name)
    if (unwind_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${unwind_${LINK_OPTION_SUFFIX}}")
    elseif (ANDROID)
        # target_link_libraries(${target_name} "${unwind_${LIBRARY_DIR_SUFFIX}}/libunwindstack.a")
    elseif (UNIX)
        target_link_libraries(${target_name} "${unwind_${LIBRARY_DIR_SUFFIX}}/libunwind.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "breakpad_commond.lib"
                optimized "breakpad_common.lib")
        target_link_libraries(${target_name}
                debug "breakpad_crash_generation_clientd.lib"
                optimized "breakpad_crash_generation_client.lib")
        target_link_libraries(${target_name}
                debug "breakpad_exception_handlerd.lib"
                optimized "breakpad_exception_handler.lib")
    endif ()
endmacro()

# ssl
macro(link_ssl target_name)
    if (ssl_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${ssl_${LINK_OPTION_SUFFIX}}")
    elseif (UNIX)
        target_link_libraries(${target_name} "${ssl_${LIBRARY_DIR_SUFFIX}}/libssl.a")
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
        target_link_libraries(${target_name} "${crypto_${LIBRARY_DIR_SUFFIX}}/libcrypto.a")
    elseif (MSVC)
        #target_link_libraries (${target_name}
        #   debug "libcurl-d"
        #   optimized "libcurl")
    endif ()
endmacro()

# leveldb
macro(link_leveldb target_name)
    if (UNIX)
        target_link_libraries(${target_name} "${leveldb_${LIBRARY_DIR_SUFFIX}}/libleveldb.a")
    elseif (MSVC)
        target_link_libraries(${target_name}
                debug "leveldbd.lib"
                optimized "leveldb.lib")
    endif ()
endmacro()

# asan for debug
macro(link_asan target_name)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        target_compile_options(${target_name} PUBLIC -fsanitize=address)
        target_link_options(${target_name} PUBLIC -fsanitize=address -static-libasan)
    endif()
endmacro()

# uuid
macro(link_uuid target_name)
    if (uuid_${LINK_OPTION_SUFFIX})
        target_link_libraries(${target_name} "${uuid_${LINK_OPTION_SUFFIX}}")
    elseif (ANDROID)
        target_link_libraries(${target_name} "${uuid_${LIBRARY_DIR_SUFFIX}}/libuuid.a")
    elseif (UNIX)
        target_link_libraries(${target_name} uuid)
    endif ()
endmacro()

macro(link_spl target_name)
    logtail_define(spl_${target_name} "" "")
    
    target_link_libraries(${target_name} "presto_adapters")
    target_link_libraries(${target_name} "presto_common")
    target_link_libraries(${target_name} "presto_exception")
    target_link_libraries(${target_name} "presto_http")
    target_link_libraries(${target_name} "presto_log_rotate")
    target_link_libraries(${target_name} "presto_prometheus")
    target_link_libraries(${target_name} "presto_server_lib")
    target_link_libraries(${target_name} "presto_sls_lib")
    target_link_libraries(${target_name} "presto_sls_rpc")
    target_link_libraries(${target_name} "presto_sls_rpc_protocol")
    target_link_libraries(${target_name} "presto_thrift-cpp2")
    target_link_libraries(${target_name} "presto_thrift_extra")
    target_link_libraries(${target_name} "presto_types")
    target_link_libraries(${target_name} "velox_tpch_connector")
    target_link_libraries(${target_name} "velox_hive_connector")
    target_link_libraries(${target_name} "presto_protocol")
    target_link_libraries(${target_name} "presto_type_converter")
    target_link_libraries(${target_name} "presto_operators")
    target_link_libraries(${target_name} "velox_exec")

    target_link_libraries(${target_name} "velox_functions_prestosql")
    target_link_libraries(${target_name} "velox_functions_prestosql_impl")
    target_link_libraries(${target_name} "velox_aggregates")
    target_link_libraries(${target_name} "velox_arrow_bridge")
    target_link_libraries(${target_name} "velox_buffer")
    target_link_libraries(${target_name} "velox_codegen")
    target_link_libraries(${target_name} "velox_common_hyperloglog")
    target_link_libraries(${target_name} "velox_connector")
    target_link_libraries(${target_name} "velox_core")
    target_link_libraries(${target_name} "velox_config")
    target_link_libraries(${target_name} "velox_coverage_util")
    target_link_libraries(${target_name} "velox_dwio_catalog_fbhive")

    target_link_libraries(${target_name} "velox_dwio_dwrf_reader")
    target_link_libraries(${target_name} "velox_dwio_dwrf_writer")
    target_link_libraries(${target_name} "velox_dwio_dwrf_utils")
    target_link_libraries(${target_name} "velox_dwio_dwrf_common")
    target_link_libraries(${target_name} "velox_dwio_dwrf_proto")
    target_link_libraries(${target_name} "velox_dwio_common")
    target_link_libraries(${target_name} "velox_dwio_common_compression")
    target_link_libraries(${target_name} "velox_dwio_common_encryption")
    target_link_libraries(${target_name} "velox_dwio_common_exception")
    target_link_libraries(${target_name} "velox_dwio_parquet_reader")
    target_link_libraries(${target_name} "velox_dwio_parquet_writer")
    target_link_libraries(${target_name} "velox_encode")
    target_link_libraries(${target_name} "velox_exception")
    target_link_libraries(${target_name} "velox_presto_serializer")
    target_link_libraries(${target_name} "velox_caching")
    target_link_libraries(${target_name} "velox_expression")
    target_link_libraries(${target_name} "velox_expression_functions")
    target_link_libraries(${target_name} "velox_external_date")
    target_link_libraries(${target_name} "velox_file")
    target_link_libraries(${target_name} "velox_parse_expression")
    target_link_libraries(${target_name} "velox_function_registry")
    target_link_libraries(${target_name} "velox_functions_aggregates")
    target_link_libraries(${target_name} "velox_functions_json")
    target_link_libraries(${target_name} "velox_functions_lib")
    target_link_libraries(${target_name} "velox_functions_util")
    target_link_libraries(${target_name} "velox_functions_window")
    target_link_libraries(${target_name} "velox_hive_partition_function")
    target_link_libraries(${target_name} "velox_is_null_functions")
    target_link_libraries(${target_name} "velox_memory")

    target_link_libraries(${target_name} "velox_parse_parser")
    target_link_libraries(${target_name} "velox_parse_utils")
    target_link_libraries(${target_name} "velox_process")
    target_link_libraries(${target_name} "velox_row_fast")
    target_link_libraries(${target_name} "velox_rpc")
    target_link_libraries(${target_name} "velox_serialization")
    target_link_libraries(${target_name} "velox_test_util")
    target_link_libraries(${target_name} "velox_time")
    target_link_libraries(${target_name} "velox_tpch_gen")
    target_link_libraries(${target_name} "velox_type")
    target_link_libraries(${target_name} "velox_type_calculation")
    target_link_libraries(${target_name} "velox_type_fbhive")
    target_link_libraries(${target_name} "velox_type_tz")
    target_link_libraries(${target_name} "velox_vector")
    target_link_libraries(${target_name} "velox_vector_fuzzer")
    target_link_libraries(${target_name} "velox_window")
    target_link_libraries(${target_name} "velox_common_base")
    target_link_libraries(${target_name} "velox_duckdb_parser")
    target_link_libraries(${target_name} "velox_duckdb_allocator")
    target_link_libraries(${target_name} "velox_duckdb_conversion")
    target_link_libraries(${target_name} "velox_duckdb_functions")

    target_link_libraries(${target_name} "velox_presto_types")
    target_link_libraries(${target_name} "dbgen")
    target_link_libraries(${target_name} "duckdb")
    target_link_libraries(${target_name} "http_filters")
    target_link_libraries(${target_name} "md5")
    target_link_libraries(${target_name} "sls_connector_proto")
    target_link_libraries(${target_name} "sls_project_version")
    target_link_libraries(${target_name} "tpch_extension")

    target_link_libraries(${target_name} "proxygen")
    target_link_libraries(${target_name} "proxygenhttpserver")
    target_link_libraries(${target_name} "glog")
    target_link_libraries(${target_name} "folly")
    target_link_libraries(${target_name} "slsprotobuf")
    target_link_libraries(${target_name} "uuid")
    target_link_libraries(${target_name} "fmt")
    target_link_libraries(${target_name} "boost_context")

    target_link_libraries(${target_name} "double-conversion")
    target_link_libraries(${target_name} "sodium")
    target_link_libraries(${target_name} "fizz")
    target_link_libraries(${target_name} "wangle")
    target_link_libraries(${target_name} "antlr4-runtime")
    target_link_libraries(${target_name} "thriftcpp2")
    target_link_libraries(${target_name} "thrift-core")
    target_link_libraries(${target_name} "thriftprotocol")
    target_link_libraries(${target_name} "thriftmetadata")
    target_link_libraries(${target_name} "transport")
    target_link_libraries(${target_name} "prometheus-cpp-core")
    target_link_libraries(${target_name} "prometheus-cpp-pull")
    target_link_libraries(${target_name} "simdjson")
    target_link_libraries(${target_name} "snappy")
    target_link_libraries(${target_name} "bz2")
    target_link_libraries(${target_name} "lzo2")

    target_link_libraries(${target_name} "x")
    target_link_libraries(${target_name} "y")
    target_link_libraries(${target_name} "event")
    target_link_libraries(${target_name} "event_pthreads")

endmacro()
