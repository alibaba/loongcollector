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
project(processor_simd_unittest)

# Enable SSE4.2 support for SIMD optimization
add_definitions(-D__INCLUDE_SSE4_2__)

# Reference source files from processor directory
set(PROCESSOR_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../processor")

add_executable(processor_simd_parse_json_native_unittest "${PROCESSOR_SRC_DIR}/ProcessorParseJsonNativeUnittest.cpp")
target_link_libraries(processor_simd_parse_json_native_unittest ${UT_BASE_TARGET})
target_compile_options(processor_simd_parse_json_native_unittest PRIVATE ${SSE4_2_FLAGS})

add_executable(json_simd_benchmark_test "${PROCESSOR_SRC_DIR}/JsonBenchmark.cpp")
target_link_libraries(json_simd_benchmark_test ${UT_BASE_TARGET})
target_compile_options(json_simd_benchmark_test PRIVATE ${SSE4_2_FLAGS})

include(GoogleTest)
gtest_discover_tests(processor_simd_parse_json_native_unittest)

