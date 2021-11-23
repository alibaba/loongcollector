#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

proto_library(
  name = "tracing_protocol_proto_lib",
  srcs = ["Tracing.proto"],
  deps = ["//common:common_protocol_proto_lib"],
)

cc_proto_library(
  name = "tracing_protocol_cc_proto",
  deps = [":tracing_protocol_proto_lib"],
)

cc_grpc_library(
  name = "tracing_protocol_cc_grpc",
  srcs = [":tracing_protocol_proto_lib"],
  deps = [":tracing_protocol_cc_proto"],
  grpc_only = True,
  generate_mocks = True,
)

proto_library(
  name = "configuration_discovery_service_proto_lib",
  srcs = ["ConfigurationDiscoveryService.proto"],
  deps = ["//common:common_protocol_proto_lib"],
)

cc_proto_library(
  name = "configuration_discovery_service_cc_proto",
  deps = [":configuration_discovery_service_proto_lib"],
)

cc_grpc_library(
  name = "configuration_discovery_service_cc_grpc",
  srcs = [":configuration_discovery_service_proto_lib"],
  deps = [":configuration_discovery_service_cc_proto"],
  grpc_only = True,
  generate_mocks = True,
)
