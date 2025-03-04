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

constexpr DataElement kHostIp = {"host_ip",
                                 "host_ip", // metric
                                 "host.ip", // span
                                 "host.ip", // log
                                 "host ip",
                                 AggregationType::Level0};

constexpr DataElement kHostName = {"host_name",
                                   "host", // metric, DO NOT USE host_name for compatibility
                                   "host.name", // span
                                   "host.name", // log
                                   "host name",
                                   AggregationType::Level0};

constexpr DataElement kAppType = {"app_type",
                                  "source", // metric
                                  "arms.app.type", // span
                                  "app_type", // log
                                  "app type",
                                  AggregationType::Level0};

constexpr DataElement kDataType = {"data_type",
                                   "data_type", // metric
                                   "data_type", // span
                                   "data_type", // log
                                   "data_type",
                                   AggregationType::Level0};

constexpr DataElement kPodName = {
    "pod_name",
    "pod_name", // metric
    "k8s.pod.name", // span
    "k8s.pod.name", // log, inside pod
    "",
    AggregationType::Level1,
};

constexpr DataElement kPodUid = {
    "pod_uid",
    "pod_uid", // metric
    "k8s.pod.uid", // span
    "k8s.pod.uid", // log, inside pod
    "pod uid",
    AggregationType::Level1,
};

constexpr DataElement kPodIp = {
    "pod_ip",
    "podIp", // metric
    "k8s.pod.ip", // span
    "k8s.pod.ip", // log, inside pod
    "",
    AggregationType::Level1,
};

constexpr DataElement kWorkloadKind = {
    "workload_kind",
    "workloadKind", // metric
    "k8s.workload.kind", // span
    "k8s.workload.kind", // log, inside pod.workload
    "",
    AggregationType::Level1,
};

constexpr DataElement kWorkloadName = {
    "workload_name",
    "workloadName", // metric
    "k8s.workload.name", // span
    "k8s.workload.name", // log, inside pod.workload
    "",
    AggregationType::Level1,
};

constexpr DataElement kNamespace = {
    "namespace",
    "namespace", // metric
    "k8s.namespace", // span
    "k8s.namespace", // log, inside pod
    "",
    AggregationType::Level1,
};

constexpr DataElement kServiceName = {
    "service_name",
    "peerServiceName", // metric
    "k8s.peer.service.name", // span
    "k8s.peer.service.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerPodName = {
    "peer_pod_name",
    "peerPodName", // metric
    "k8s.peer.pod.name", // span
    "k8s.peer.pod.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerPodIp = {
    "peer_pod_ip",
    "peerPodIp", // metric
    "k8s.peer.pod.ip", // span
    "k8s.peer.pod.ip", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerWorkloadKind = {
    "peer_workload_kind",
    "peerWorkloadKind", // metric
    "k8s.peer.workload.kind", // span
    "k8s.peer.workload.kind", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerWorkloadName = {
    "peer_workload_name",
    "peerWorkloadName", // metric
    "peerWorkloadName", // span
    "k8s.peer.workload.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerServiceName = {
    "peer_service_name",
    "peerServiceName", // metric
    "k8s.peer.service.name", // span
    "k8s.peer.service.name", // log
    "",
    AggregationType::Level1,
};

constexpr DataElement kPeerNamespace = {
    "peer_namespace",
    "peerNamespace", // metric
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
                                    "start_time_nsec", // log
                                    "Request-response latency."};

constexpr DataElement kEndTsNs = {"endTsNs",
                                  "", // metric
                                  "endTsNs", // span
                                  "end_time_nsec", // log
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

constexpr DataElement kRemoteIp = {"remote_ip",
                                   "", // metric
                                   "remote.ip", // span
                                   "remote.ip", // log
                                   "remote ip.",
                                   AggregationType::NoAggregate};

constexpr DataElement kAppId = {"app_id",
                                "pid", // metric
                                "arms.appId", // span
                                "arms.app.id", // log
                                "arms app id",
                                AggregationType::Level0};

constexpr DataElement kNetNs = {
    "net_ns",
    "", // metric
    "net.namespace", // span
    "netns", // log
    "",
};
constexpr DataElement kFamily = {
    "family",
    "", // metric
    "family", // span
    "family", // log
    "",
};

constexpr DataElement kAppName = {"app",
                                  "service", // metric
                                  "service.name", // span
                                  "arms.app.name", // log
                                  "arms app name",
                                  AggregationType::Level0};

constexpr DataElement kPeerAppName = {"peer_app",
                                      "arms_peer_app_name", // metric
                                      "arms.peer.app.name", // span
                                      "arms.app.name", // log
                                      "arms app name",
                                      AggregationType::Level0};

constexpr DataElement kDestId = {
    "dest_id",
    "destId", // metric
    "destId", // span
    "dest.id", // log
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

constexpr DataElement kEndpoint = {"endpoint",
                                   "endpoint", // metric
                                   "endpoint", // span
                                   "endpoint", // log
                                   "reqeust path",
                                   AggregationType::Level1};

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
    "rpc_type", // log
    "arms rpc type",
    AggregationType::Level1,
};

constexpr DataElement kCallType = {
    "callType",
    "callType", // metric
    "callType", // span
    "arms.call.type", // log
    "arms call type",
    AggregationType::Level1,
};

constexpr DataElement kCallKind = {
    "callKind",
    "callKind", // metric
    "callKind", // span
    "arms.call.kind", // log
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

constexpr DataElement kContainerName = {
    "container_name",
    "container_name", // metric
    "container.name", // span
    "container.name", // log
    "local container name",
    AggregationType::Level1,
};

constexpr DataElement kContainerIp = {
    "container_ip",
    "container_ip", // metric
    "container.ip", // span
    "container.ip", // log
    "container ip",
    AggregationType::Level1,
};

constexpr DataElement kImageName = {
    "image_name",
    "conatainer_image_name", // metric
    "conatainer.image.name", // span
    "conatainer.image.name", // log
    "container image name",
    AggregationType::Level1,
};

// for processes
constexpr DataElement kProcessId
    = {"process_pid", "process_pid", "process.pid", "pid", "process pid", AggregationType::Level0};

constexpr DataElement kKtime = {"ktime", "", "", "ktime", "", AggregationType::Level0};

constexpr DataElement kExecId = {"exec_id", "", "", "exec_id", "", AggregationType::NoAggregate};

constexpr DataElement kUser = {"user", "", "", "user", "", AggregationType::NoAggregate};

constexpr DataElement kUid = {"uid", "", "", "uid", "", AggregationType::NoAggregate};

constexpr DataElement kBinary = {"binary", "", "", "binary", "", AggregationType::NoAggregate};

constexpr DataElement kCWD = {"cwd", "", "", "cwd", "", AggregationType::NoAggregate};

constexpr DataElement kArguments = {"arguments", "", "", "arguments", "", AggregationType::NoAggregate};

constexpr DataElement kCapPermitted = {"cap_permitted", "", "", "cap.permitted", "", AggregationType::NoAggregate};

constexpr DataElement kCapInheritable
    = {"cap_inheritable", "", "", "cap.inheritable", "", AggregationType::NoAggregate};

constexpr DataElement kCapEffective = {"cap_effective", "", "", "cap.effective", "", AggregationType::NoAggregate};

constexpr DataElement kCallName = {"call_name", "", "", "call_name", "", AggregationType::NoAggregate};

constexpr DataElement kEventType
    = {"event_type", "event_type", "event_type", "event_type", "", AggregationType::NoAggregate};

constexpr DataElement kEventTime
    = {"event_time", "event_time", "event_time", "event_time", "", AggregationType::NoAggregate};

constexpr DataElement kParentProcessId
    = {"parent_process_pid", "", "", "parent.pid", "parent process pid", AggregationType::Level0};

constexpr DataElement kParentKtime = {"parent_ktime", "", "", "parent.ktime", "", AggregationType::Level0};

constexpr DataElement kParentExecId = {"parent_exec_id", "", "", "parent.exec_id", "", AggregationType::NoAggregate};

constexpr DataElement kParentUser = {"parent_user", "", "", "parent.user", "", AggregationType::NoAggregate};

constexpr DataElement kParentUid = {"parent_uid", "", "", "parent.uid", "", AggregationType::NoAggregate};

constexpr DataElement kParentBinary = {"parent_binary", "", "", "parent.binary", "", AggregationType::NoAggregate};

constexpr DataElement kParentCWD = {"parent_cwd", "", "", "parent.cwd", "", AggregationType::NoAggregate};

constexpr DataElement kParentArguments
    = {"parent_arguments", "", "", "parent.arguments", "", AggregationType::NoAggregate};

constexpr DataElement kParentCapPermitted
    = {"parent_cap_permitted", "", "", "parent.cap.permitted", "", AggregationType::NoAggregate};

constexpr DataElement kParentCapInheritable
    = {"parent_cap_inheritable", "", "", "parent.cap.inheritable", "", AggregationType::NoAggregate};

constexpr DataElement kParentCapEffective
    = {"parent_cap_effective", "", "", "parent.cap.effective", "", AggregationType::NoAggregate};

// for network
constexpr DataElement kSaddr
    = {"source.addr", "saddr", "saddr", "network.saddr", "source address", AggregationType::NoAggregate};
constexpr DataElement kDaddr = {"dest.addr", "", "", "network.daddr", "dest address", AggregationType::NoAggregate};
constexpr DataElement kSport = {"source.port", "", "", "network.sport", "source port", AggregationType::NoAggregate};
constexpr DataElement kState
    = {"state", "state", "", "network.state", "connection state", AggregationType::NoAggregate};
constexpr DataElement kDport = {"dest.port", "", "", "network.dport", "dest port", AggregationType::NoAggregate};

constexpr DataElement kL4Protocol
    = {"protocol", "", "", "network.protocol", "L4 protocol", AggregationType::NoAggregate};

// for file
constexpr DataElement kFilePath = {"path", "", "", "file.path", "file path", AggregationType::NoAggregate};
} // namespace ebpf
} // namespace logtail
