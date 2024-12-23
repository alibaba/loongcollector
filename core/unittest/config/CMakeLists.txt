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
project(config_unittest)

# add_executable(config_match_unittest ConfigMatchUnittest.cpp)
# target_link_libraries(config_match_unittest ${UT_BASE_TARGET})

# add_executable(config_updator_unittest ConfigUpdatorUnittest.cpp)
# target_link_libraries(config_updator_unittest ${UT_BASE_TARGET})

add_executable(pipeline_config_unittest PipelineConfigUnittest.cpp)
target_link_libraries(pipeline_config_unittest ${UT_BASE_TARGET})

add_executable(task_config_unittest TaskConfigUnittest.cpp)
target_link_libraries(task_config_unittest ${UT_BASE_TARGET})

add_executable(config_watcher_unittest ConfigWatcherUnittest.cpp)
target_link_libraries(config_watcher_unittest ${UT_BASE_TARGET})

add_executable(config_update_unittest ConfigUpdateUnittest.cpp)
target_link_libraries(config_update_unittest ${UT_BASE_TARGET})

if (ENABLE_ENTERPRISE)
    # add_executable(legacy_config_provider_unittest LegacyConfigProviderUnittest.cpp)
    # target_link_libraries(legacy_config_provider_unittest ${UT_BASE_TARGET})
endif ()

add_executable(config_feedbackable_unittest ConfigFeedbackableUnittest.cpp)
target_link_libraries(config_feedbackable_unittest ${UT_BASE_TARGET})

add_executable(common_config_provider_unittest CommonConfigProviderUnittest.cpp)
target_link_libraries(common_config_provider_unittest ${UT_BASE_TARGET})

add_executable(pipeline_config_watcher_unittest PipelineConfigWatcherUnittest.cpp)
target_link_libraries(pipeline_config_watcher_unittest ${UT_BASE_TARGET})

include(GoogleTest)
gtest_discover_tests(pipeline_config_unittest)
gtest_discover_tests(task_config_unittest)
gtest_discover_tests(config_watcher_unittest)
gtest_discover_tests(config_update_unittest)
if (ENABLE_ENTERPRISE)
    # gtest_discover_tests(legacy_config_provider_unittest)
endif ()
gtest_discover_tests(config_feedbackable_unittest)
gtest_discover_tests(common_config_provider_unittest)
gtest_discover_tests(pipeline_config_watcher_unittest)
