// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "ebpf/type/table/DataTable.h"

namespace logtail {
namespace ebpf {

constexpr DataElement kHost = {"host",
                               "host", // metric
                               "host", // span
                               "host", // log
                               "",
                               AggregationType::Level0};

constexpr DataElement kPodName = {
    "pod_name",
    "pod_name", // metric
    "k8s.pod.name", // span
    "k8s.pod.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPodIp = {
    "pod_name",
    "pod_name", // metric
    "k8s.pod.name", // span
    "k8s.pod.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kWorkloadKind = {
    "workload_kind",
    "workload_kind", // metric
    "k8s.workload.kind", // span
    "k8s.workload.kind", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kWorkloadName = {
    "workload_name",
    "workload_name", // metric
    "k8s.workload.name", // span
    "k8s.workload.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kNamespace = {
    "namespace",
    "namespace", // metric
    "k8s.namespace", // span
    "k8s.namespace", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerPodName = {
    "peer_pod_name",
    "peer_pod_name", // metric
    "k8s.peer.pod.name", // span
    "k8s.peer.pod.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerPodIp = {
    "peer_pod_ip",
    "peer_pod_ip", // metric
    "k8s.peer.pod.ip", // span
    "k8s.peer.pod.ip", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerWorkloadKind = {
    "peer_workload_kind",
    "peer_workload_kind", // metric
    "k8s.peer.workload.kind", // span
    "k8s.peer.workload.kind", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerWorkloadName = {
    "peer_workload_name",
    "peer_workload_name", // metric
    "k8s.peer.workload.name", // span
    "k8s.peer.workload.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerServiceName = {
    "peer_service_name",
    "peer_service_name", // metric
    "k8s.peer.service.name", // span
    "k8s.peer.service.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerNamespace = {
    "peer_namespace",
    "peer_namespace", // metric
    "k8s.peer.namespace", // span
    "k8s.peer.namespace", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kRemoteAddr = {
    "remote_addr",
    "remote_addr", // metric
    "remote.addr", // span
    "remote.addr", // log
    "IP address of the remote endpoint.",
    AggregationType::Level1,
};

constexpr DataElement kRemotePort = {
    "remote_port",
    "remote_port", // metric
    "remote.port", // span
    "remote.port", // log
    "Port of the remote endpoint.",
    AggregationType::Level1,
};

constexpr DataElement kLocalAddr = {
    "local_addr",
    "local_addr", // metric
    "local.addr", // span
    "local.addr", // log
    "IP address of the local endpoint.",
    AggregationType::Level1,
};

constexpr DataElement kLocalPort = {"local_port",
                                    "local_addr", // metric
                                    "local.port", // span
                                    "local.port", // log
                                    "Port of the local endpoint."};

constexpr DataElement kTraceRole = {"trace_role",
                                    "trace_role", // metric
                                    "trace.role", // span
                                    "trace.role", // log
                                    "The role (client-or-server) of the process that owns the connections."};

constexpr DataElement kLatencyNS = {"latency",
                                    "latency", // metric
                                    "latency", // span
                                    "latency", // log
                                    "Request-response latency."};

constexpr DataElement kStartTsNs = {"startTsNs",
                                    "startTsNs", // metric
                                    "startTsNs", // span
                                    "start.ts.ns", // log
                                    "Request-response latency."};

constexpr DataElement kEndTsNs = {"endTsNs",
                                  "", // metric
                                  "endTsNs", // span
                                  "end.ts.ns", // log
                                  "Request-response latency."};

constexpr DataElement kRegionId = {"region_id",
                                   "regionId", // metric
                                   "regionId", // span
                                   "region.id", // log
                                   "region id",
                                   AggregationType::Level0};

constexpr DataElement kIp = {"ip",
                             "serverIp", // metric
                             "ip", // span
                             "ip", // log
                             "local ip.",
                             AggregationType::Level0};

constexpr DataElement kAppId = {"app_id",
                                "pid", // metric
                                "app.id", // span
                                "arms.app.id", // log
                                "arms app id",
                                AggregationType::Level0};

constexpr DataElement kNetNs = {
    "net_ns",
    "", // metric
    "net.namespace", // span
    "net.namespace", // log
    "",
};
constexpr DataElement kFamily = {
    "family",
    "", // metric
    "family", // span
    "family", // log
    "",
};

constexpr DataElement kAppName = {"app_name",
                                  "service", // metric
                                  "arms.app.name", // span
                                  "arms.app.name", // log
                                  "arms app name",
                                  AggregationType::Level0};

constexpr DataElement kPeerAppName = {"peer_app_name",
                                      "arms_peer_app_name", // metric
                                      "arms.peer.app.name", // span
                                      "arms.app.name", // log
                                      "arms app name",
                                      AggregationType::Level0};

constexpr DataElement kDestId = {
    "dest_id",
    "destId", // metric
    "destId", // span
    "destId", // log
    "peer addr (ip:port)",
    AggregationType::Level1,
};

constexpr DataElement kFd = {
    "fd",
    "fd", // metric
    "fd", // span
    "fd", // log
    "fd",
};

constexpr DataElement kPid = {
    "pid",
    "", // metric
    "process.id", // span
    "process.id", // log
    "pid",
};

constexpr DataElement kEndpoint = {"endpoint",
                                   "endpoint", // metric
                                   "endpoint", // span
                                   "endpoint", // log
                                   "reqeust path"};

constexpr DataElement kProtocol = {
    "protocol",
    "protocol", // metric
    "protocol", // span
    "protocol", // log
    "request protocol",
    AggregationType::NoAggregate,
};

constexpr DataElement kRpcType = {
    "rpcType",
    "rpcType", // metric
    "rpcType", // span
    "rpcType", // log
    "arms rpc type",
    AggregationType::Level1,
};

constexpr DataElement kCallType = {
    "callType",
    "callType", // metric
    "callType", // span
    "call.type", // log
    "arms call type",
    AggregationType::Level1,
};

constexpr DataElement kCallKind = {
    "callKind",
    "callKind", // metric
    "callKind", // span
    "call.kind", // log
    "arms call kind",
    AggregationType::Level1,
};

constexpr DataElement kRpc = {
    "rpc",
    "rpc", // metric
    "rpc", // span
    "rpc", // log
    "span name",
    AggregationType::Level1,
};

constexpr DataElement kContainerId = {
    "container_id",
    "", // metric
    "container.id", // span
    "container.id", // log
    "local container id",
    AggregationType::Level1,
};

// for processes
constexpr DataElement kProcessId = {"", "", "", "", "", AggregationType::Level0};

constexpr DataElement kKtime = {"", "", "", "", "", AggregationType::Level0};

constexpr DataElement kExecId = {"", "", "", "exec_id", "", AggregationType::NoAggregate};

constexpr DataElement kParentExecId = {"", "", "", "parent_exec_id", "", AggregationType::NoAggregate};

constexpr DataElement kUser = {"", "", "", "user", "", AggregationType::NoAggregate};

constexpr DataElement kUid = {"", "", "", "uid", "", AggregationType::NoAggregate};

constexpr DataElement kBinary = {"", "", "", "binary", "", AggregationType::NoAggregate};

constexpr DataElement kCWD = {"", "", "", "cwd", "", AggregationType::NoAggregate};

constexpr DataElement kArguments = {"", "", "", "arguments", "", AggregationType::NoAggregate};

constexpr DataElement kCap = {"", "", "", "cap", "", AggregationType::NoAggregate};

constexpr DataElement kCallName = {"", "", "", "call_name", "", AggregationType::NoAggregate};

constexpr DataElement kEventType
    = {"event_type", "event_type", "event_type", "event_type", "", AggregationType::NoAggregate};

constexpr DataElement kEventTime
    = {"event_time", "event_time", "event_time", "event_time", "", AggregationType::NoAggregate};

constexpr DataElement kParentProcess
    = {"parent_process", "parent_process", "parent_process", "parent_process", "", AggregationType::NoAggregate};

// for network
constexpr DataElement kSaddr
    = {"source.addr", "saddr", "saddr", "saddr", "source address", AggregationType::NoAggregate};
constexpr DataElement kDaddr = {"", "", "", "daddr", "dest address", AggregationType::NoAggregate};
constexpr DataElement kSport = {"", "", "", "sport", "source port", AggregationType::NoAggregate};
constexpr DataElement kState = {"", "", "", "state", "connection state", AggregationType::NoAggregate};
constexpr DataElement kDport = {"", "", "", "dport", "dest port", AggregationType::NoAggregate};

constexpr DataElement kL4Protocol = {"", "", "", "protocol", "L4 protocol", AggregationType::NoAggregate};

constexpr DataElement kNetwork
    = {"", "", "", "network", "value is json for saddr and daddr ... ", AggregationType::NoAggregate};

// for file
constexpr DataElement kFilePath = {"", "", "", "path", "file path", AggregationType::NoAggregate};

constexpr DataElement kFile = {"", "", "", "file", "value is json for path ... ", AggregationType::NoAggregate};

} // namespace ebpf
} // namespace logtail
