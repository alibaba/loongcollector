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

cmake_minimum_required(VERSION 2.9)
project(log_file_reader_unittest)

add_executable(log_file_reader_deleted_file_unittest DeletedFileUnittest.cpp)
target_link_libraries(log_file_reader_deleted_file_unittest unittest_base)

add_executable(source_buffer_unittest SourceBufferUnittest.cpp)
target_link_libraries(source_buffer_unittest unittest_base)

add_executable(log_file_reader_unittest LogFileReaderUnittest.cpp)
target_link_libraries(log_file_reader_unittest unittest_base)

add_executable(last_matched_line_unittest LastMatchedLineUnittest.cpp)
target_link_libraries(last_matched_line_unittest unittest_base)

add_executable(log_split_unittest LogSplitUnittest.cpp)
target_link_libraries(log_split_unittest unittest_base)

add_executable(json_log_file_reader_unittest JsonLogFileReaderUnittest.cpp)
target_link_libraries(json_log_file_reader_unittest unittest_base)

add_executable(delimiter_log_file_reader_unittest DelimiterLogFileReaderUnittest.cpp)
target_link_libraries(delimiter_log_file_reader_unittest unittest_base)

add_executable(common_reg_log_file_reader_unittest CommonRegLogFileReaderUnittest.cpp)
target_link_libraries(common_reg_log_file_reader_unittest unittest_base)

add_executable(apsara_log_file_reader_unittest ApsaraLogFileReaderUnittest.cpp)
target_link_libraries(apsara_log_file_reader_unittest unittest_base)

if (UNIX)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/testDataSet)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testDataSet/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/testDataSet/)
elseif (MSVC)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/testDataSet)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testDataSet/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/testDataSet/)
endif ()
