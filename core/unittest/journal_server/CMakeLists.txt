# Copyright 2025 iLogtail Authors
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
project(journal_server_unittest)

# JournalServer测试只在Linux平台下编译，因为需要systemd journal支持
if (LINUX)
    # JournalServer核心测试
    add_executable(journal_server_unittest JournalServerUnittest.cpp)
    target_link_libraries(journal_server_unittest ${UT_BASE_TARGET})

    # JournalReader测试
    add_executable(journal_reader_unittest JournalReaderUnittest.cpp)
    target_link_libraries(journal_reader_unittest ${UT_BASE_TARGET})

    # JournalConnection测试
    add_executable(journal_connection_manager_unittest JournalConnectionManagerUnittest.cpp)
    target_link_libraries(journal_connection_manager_unittest ${UT_BASE_TARGET})

    # JournalFilter测试
    add_executable(journal_filter_unittest JournalFilterUnittest.cpp)
    target_link_libraries(journal_filter_unittest ${UT_BASE_TARGET})

    # JournalConfig测试
    add_executable(journal_config_unittest JournalConfigUnittest.cpp)
    target_link_libraries(journal_config_unittest ${UT_BASE_TARGET})

    # JournalUtils测试
    add_executable(journal_utils_unittest JournalUtilsUnittest.cpp)
    target_link_libraries(journal_utils_unittest ${UT_BASE_TARGET})

    # JournalEntryProcessor测试
    add_executable(journal_entry_processor_unittest JournalEntryProcessorUnittest.cpp)
    target_link_libraries(journal_entry_processor_unittest ${UT_BASE_TARGET})

    # InputJournal测试
    add_executable(input_journal_unittest InputJournalUnittest.cpp)
    target_link_libraries(input_journal_unittest ${UT_BASE_TARGET})

    include(GoogleTest)
    gtest_discover_tests(journal_server_unittest)
    gtest_discover_tests(journal_reader_unittest)
    gtest_discover_tests(journal_connection_manager_unittest)
    gtest_discover_tests(journal_filter_unittest)
    gtest_discover_tests(journal_config_unittest)
    gtest_discover_tests(journal_utils_unittest)
    gtest_discover_tests(journal_entry_processor_unittest)
    gtest_discover_tests(input_journal_unittest)
else()
    message(STATUS "JournalServer unittest skipped - not supported on this platform")
endif()