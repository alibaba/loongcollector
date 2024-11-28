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
project(log_file_reader_unittest)

add_executable(log_file_reader_deleted_file_unittest DeletedFileUnittest.cpp)
target_link_libraries(log_file_reader_deleted_file_unittest ${UT_BASE_TARGET})

add_executable(file_reader_options_unittest FileReaderOptionsUnittest.cpp)
target_link_libraries(file_reader_options_unittest ${UT_BASE_TARGET})

add_executable(json_log_file_reader_unittest JsonLogFileReaderUnittest.cpp)
target_link_libraries(json_log_file_reader_unittest ${UT_BASE_TARGET})

add_executable(remove_last_incomplete_log_unittest RemoveLastIncompleteLogUnittest.cpp)
target_link_libraries(remove_last_incomplete_log_unittest ${UT_BASE_TARGET})

add_executable(log_file_reader_unittest LogFileReaderUnittest.cpp)
target_link_libraries(log_file_reader_unittest ${UT_BASE_TARGET})

add_executable(source_buffer_unittest SourceBufferUnittest.cpp)
target_link_libraries(source_buffer_unittest ${UT_BASE_TARGET})

add_executable(get_last_line_data_unittest GetLastLineDataUnittest.cpp)
target_link_libraries(get_last_line_data_unittest ${UT_BASE_TARGET})

# add_executable(force_read_unittest ForceReadUnittest.cpp)
# target_link_libraries(force_read_unittest ${UT_BASE_TARGET})

if (UNIX)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/testDataSet)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testDataSet/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/testDataSet/)
elseif (MSVC)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/testDataSet)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testDataSet/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/testDataSet/)
endif ()

include(GoogleTest)
gtest_discover_tests(log_file_reader_deleted_file_unittest)
gtest_discover_tests(file_reader_options_unittest)
gtest_discover_tests(json_log_file_reader_unittest)
gtest_discover_tests(remove_last_incomplete_log_unittest)
gtest_discover_tests(log_file_reader_unittest)
gtest_discover_tests(source_buffer_unittest)
gtest_discover_tests(get_last_line_data_unittest)
# gtest_discover_tests(force_read_unittest)
