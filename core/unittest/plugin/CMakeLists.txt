# Copyright 2023 iLogtail Authors
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
project(plugin_unittest)

add_executable(static_processor_creator_unittest StaticProcessorCreatorUnittest.cpp)
target_link_libraries(static_processor_creator_unittest unittest_base)

add_executable(plugin_registry_unittest PluginRegistryUnittest.cpp)
target_link_libraries(plugin_registry_unittest unittest_base)
