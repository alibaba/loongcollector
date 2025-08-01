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
            # target_link_libraries(${target_name} "${tcmalloc_${LIBRARY_DIR_SUFFIX}}/libtcmalloc_and_profiler.a")
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
        target_link_libraries(${target_name} "${uuid_${LIBRARY_DIR_SUFFIX}}/libuuid.a")
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
        target_link_libraries(${target_name}
                debug "libprotobufd"
                optimized "libprotobuf")
    endif()
endmacro()

macro(link_spl target_name)
    logtail_define(spl_${target_name} "" "")

    find_library(LIB_FOUND "libloongcollector_spl.a")
    if (NOT LIB_FOUND)
        message(FATAL_ERROR "Please upgrade your development image to compile!")
    endif()
    
    target_link_libraries(${target_name} "libloongcollector_spl.a")
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
    target_link_libraries(${target_name} "libboost_context.a")

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
    target_link_libraries(${target_name} "libsimdjson.a")
    target_link_libraries(${target_name} "libsnappy.a")
    target_link_libraries(${target_name} "libbz2.a")
    target_link_libraries(${target_name} "liblzo2.a")

    target_link_libraries(${target_name} "liby.a")
    target_link_libraries(${target_name} "libevent.a")
    target_link_libraries(${target_name} "libevent_pthreads.a")
    target_link_libraries(${target_name} "libapsara_common.a")
    target_link_libraries(${target_name} "libbuild_info.a")
    
endmacro()
