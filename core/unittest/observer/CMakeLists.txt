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
project(observer_unittest)

add_executable(observer_config_unittest ObserverConfigUnittest.cpp)
target_link_libraries(observer_config_unittest unittest_base)

# protocol unittest
# add_executable(protocol_dns_unittest ProtocolDnsUnittest.cpp)
# add_executable(protocol_http_unittest ProtocolHttpUnittest.cpp)
# add_executable(protocol_redis_unittest ProtocolRedisUnittest.cpp)
# add_executable(protocol_pgsql_unittest ProtocolPgsqlUnittest.cpp)
# add_executable(protocol_mysql_unittest ProtocolMySqlUnittest.cpp)

# target_link_libraries(protocol_dns_unittest unittest_base)
# target_link_libraries(protocol_http_unittest unittest_base)
# target_link_libraries(protocol_redis_unittest unittest_base)
# target_link_libraries(protocol_pgsql_unittest unittest_base)
# target_link_libraries(protocol_mysql_unittest unittest_base)

# cgoutp meta unittest
# add_executable(cgroup_meta_unittest CGoupMetaUnittest.cpp)
# target_link_libraries(cgroup_meta_unittest unittest_base)
add_executable(netlink_meta_unittest NetLinkUnittest.cpp)
target_link_libraries(netlink_meta_unittest unittest_base)

add_executable(hostname_meta_unittest HostnameMetaUnittest.cpp)
target_link_libraries(hostname_meta_unittest unittest_base)

# framework unittest
add_executable(network_observer_unittest NetworkObserverUnittest.cpp)
add_executable(protocol_util_unittest ProtocolUtilUnittest.cpp)
add_executable(protocol_infer_unittest ProtocolInferUnittest.cpp)

target_link_libraries(network_observer_unittest unittest_base)
target_link_libraries(protocol_util_unittest unittest_base)
target_link_libraries(protocol_infer_unittest unittest_base)

include(GoogleTest)
gtest_discover_tests(observer_config_unittest)
gtest_discover_tests(netlink_meta_unittest)
gtest_discover_tests(hostname_meta_unittest)
gtest_discover_tests(network_observer_unittest)
gtest_discover_tests(protocol_util_unittest)
gtest_discover_tests(protocol_infer_unittest)
