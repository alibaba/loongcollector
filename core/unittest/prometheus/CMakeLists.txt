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

cmake_minimum_required(VERSION 3.22)
project(prometheus_unittest)

add_executable(labels_unittest LabelsUnittest.cpp)
target_link_libraries(labels_unittest ${UT_BASE_TARGET})
target_link_libraries(labels_unittest ${UT_BASE_TARGET})

add_executable(relabel_unittest RelabelUnittest.cpp)
target_link_libraries(relabel_unittest ${UT_BASE_TARGET})

add_executable(target_subscriber_scheduler_unittest TargetSubscriberSchedulerUnittest.cpp)
target_link_libraries(target_subscriber_scheduler_unittest ${UT_BASE_TARGET})

add_executable(scrape_scheduler_unittest ScrapeSchedulerUnittest.cpp)
target_link_libraries(scrape_scheduler_unittest ${UT_BASE_TARGET})

add_executable(prometheus_input_runner_unittest PrometheusInputRunnerUnittest.cpp)
target_link_libraries(prometheus_input_runner_unittest ${UT_BASE_TARGET})

add_executable(textparser_unittest TextParserUnittest.cpp)
target_link_libraries(textparser_unittest ${UT_BASE_TARGET})

add_executable(textparser_benchmark TextParserBenchmark.cpp)
target_link_libraries(textparser_benchmark ${UT_BASE_TARGET})

add_executable(scrape_config_unittest ScrapeConfigUnittest.cpp)
target_link_libraries(scrape_config_unittest ${UT_BASE_TARGET})

add_executable(prom_utils_unittest UtilsUnittest.cpp)
target_link_libraries(prom_utils_unittest ${UT_BASE_TARGET})

include(GoogleTest)

gtest_discover_tests(labels_unittest)
gtest_discover_tests(relabel_unittest)
gtest_discover_tests(scrape_scheduler_unittest)
gtest_discover_tests(target_subscriber_scheduler_unittest)
gtest_discover_tests(prometheus_input_runner_unittest)
gtest_discover_tests(textparser_unittest)
gtest_discover_tests(textparser_benchmark)
gtest_discover_tests(scrape_config_unittest)
gtest_discover_tests(prom_utils_unittest)
