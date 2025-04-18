// Copyright 2025 iLogtail Authors
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

namespace logtail::ebpf {

inline constexpr DataElement kHostIp = {
    "host_ip",
    "host_ip", // metric
    "host.ip", // span
    "host.ip", // log
    "host ip",
};

inline constexpr DataElement kHostName = {
    "host_name",
    "host", // metric, DO NOT USE host_name for compatibility
    "host.name", // span
    "host.name", // log
    "host name",
};

inline constexpr DataElement kAppType = {
    "app_type",
    "source", // metric
    "arms.app.type", // span
    "app_type", // log
    "app type",
};

inline constexpr DataElement kDataType = {
    "data_type",
    "data_type", // metric
    "data_type", // span
    "data_type", // log
    "data_type",
};

inline constexpr DataElement kPodName = {
    "pod_name",
    "pod_name", // metric
    "k8s.pod.name", // span
    "k8s.pod.name", // log, inside pod
    "",
};

inline constexpr DataElement kPodIp = {
    "pod_ip",
    "podIp", // metric
    "k8s.pod.ip", // span
    "k8s.pod.ip", // log, inside pod
    "",
};

inline constexpr DataElement kWorkloadKind = {
    "workload_kind",
    "workloadKind", // metric
    "k8s.workload.kind", // span
    "k8s.workload.kind", // log, inside pod.workload
    "",
};

inline constexpr DataElement kWorkloadName = {
    "workload_name",
    "workloadName", // metric
    "k8s.workload.name", // span
    "k8s.workload.name", // log, inside pod.workload
    "",
};

inline constexpr DataElement kNamespace = {
    "namespace",
    "namespace", // metric
    "k8s.namespace", // span
    "k8s.namespace", // log, inside pod
    "",
};

inline constexpr DataElement kServiceName = {
    "service_name",
    "peerServiceName", // metric
    "k8s.peer.service.name", // span
    "k8s.peer.service.name", // log
    "",
};

inline constexpr DataElement kPeerPodName = {
    "peer_pod_name",
    "peerPodName", // metric
    "k8s.peer.pod.name", // span
    "k8s.peer.pod.name", // log
    "",
};

inline constexpr DataElement kPeerPodIp = {
    "peer_pod_ip",
    "peerPodIp", // metric
    "k8s.peer.pod.ip", // span
    "k8s.peer.pod.ip", // log
    "",
};

inline constexpr DataElement kPeerWorkloadKind = {
    "peer_workload_kind",
    "peerWorkloadKind", // metric
    "k8s.peer.workload.kind", // span
    "k8s.peer.workload.kind", // log
    "",
};

inline constexpr DataElement kPeerWorkloadName = {
    "peer_workload_name",
    "peerWorkloadName", // metric
    "peerWorkloadName", // span
    "k8s.peer.workload.name", // log
    "",
};

inline constexpr DataElement kPeerServiceName = {
    "peer_service_name",
    "peerServiceName", // metric
    "k8s.peer.service.name", // span
    "k8s.peer.service.name", // log
    "",
};

inline constexpr DataElement kPeerNamespace = {
    "peer_namespace",
    "peerNamespace", // metric
    "k8s.peer.namespace", // span
    "k8s.peer.namespace", // log
    "",
};

inline constexpr DataElement kRemoteAddr = {
    "remote_addr",
    "remote_addr", // metric
    "remote.addr", // span
    "remote.addr", // log
    "IP address of the remote endpoint.",
};

inline constexpr DataElement kRemotePort = {
    "remote_port",
    "remote_port", // metric
    "remote.port", // span
    "remote.port", // log
    "Port of the remote endpoint.",
};

inline constexpr DataElement kLocalAddr = {
    "local_addr",
    "local_addr", // metric
    "local.addr", // span
    "local.addr", // log
    "IP address of the local endpoint.",
};

inline constexpr DataElement kTraceRole = {
    "trace_role",
    "trace_role", // metric
    "trace.role", // span
    "trace.role", // log
    "The role (client-or-server) of the process that owns the connections.",
};

inline constexpr DataElement kLatencyNS = {
    "latency",
    "latency", // metric
    "latency", // span
    "latency", // log
    "Request-response latency.",
};

inline constexpr DataElement kStartTsNs = {
    "startTsNs",
    "startTsNs", // metric
    "startTsNs", // span
    "start_time_nsec", // log
    "Request-response latency.",
};

inline constexpr DataElement kIp = {
    "ip",
    "serverIp", // metric
    "ip", // span
    "ip", // log
    "local ip.",
};

inline constexpr DataElement kRemoteIp = {
    "remote_ip",
    "", // metric
    "remote.ip", // span
    "remote.ip", // log
    "remote ip.",
};

inline constexpr DataElement kAppId = {
    "app_id",
    "pid", // metric
    "arms.appId", // span
    "arms.app.id", // log
    "arms app id",
};

inline constexpr DataElement kNetNs = {
    "net_ns",
    "", // metric
    "net.namespace", // span
    "netns", // log
    "",
};
inline constexpr DataElement kFamily = {
    "family",
    "", // metric
    "family", // span
    "family", // log
    "",
};

inline constexpr DataElement kAppName = {
    "app",
    "service", // metric
    "service.name", // span
    "arms.app.name", // log
    "arms app name",
};

inline constexpr DataElement kPeerAppName = {
    "peer_app",
    "arms_peer_app_name", // metric
    "arms.peer.app.name", // span
    "arms.app.name", // log
    "arms app name",
};

inline constexpr DataElement kDestId = {
    "dest_id",
    "destId", // metric
    "destId", // span
    "dest.id", // log
    "peer addr (ip:port)",
};

inline constexpr DataElement kFd = {
    "fd",
    "fd", // metric
    "fd", // span
    "fd", // log
    "fd",
};

inline constexpr DataElement kEndpoint = {
    "endpoint",
    "endpoint", // metric
    "endpoint", // span
    "endpoint", // log
    "reqeust path",
};

inline constexpr DataElement kProtocol = {
    "protocol",
    "protocol", // metric
    "protocol", // span
    "protocol", // log
    "request protocol",
};

inline constexpr DataElement kRpcType = {
    "rpcType",
    "rpcType", // metric
    "rpcType", // span
    "rpc_type", // log
    "arms rpc type",
};

inline constexpr DataElement kCallType = {
    "callType",
    "callType", // metric
    "callType", // span
    "arms.call.type", // log
    "arms call type",
};

inline constexpr DataElement kCallKind = {
    "callKind",
    "callKind", // metric
    "callKind", // span
    "arms.call.kind", // log
    "arms call kind",
};

inline constexpr DataElement kRpc = {
    "rpc",
    "rpc", // metric
    "rpc", // span
    "rpc", // log
    "span name",
};

inline constexpr DataElement kContainerId = {
    "container_id",
    "", // metric
    "container.id", // span
    "container.id", // log
    "local container id",
};

// for processes
inline constexpr DataElement kProcessId = {
    "process_pid",
    "process_pid",
    "process.pid",
    "pid",
    "process pid",
};

inline constexpr DataElement kKtime = {
    "ktime",
    "",
    "",
    "ktime",
    "",
};

inline constexpr DataElement kExecId = {
    "exec_id",
    "",
    "",
    "exec_id",
    "",
};

inline constexpr DataElement kUser = {
    "user",
    "",
    "",
    "user",
    "",
};

inline constexpr DataElement kUid = {
    "uid",
    "",
    "",
    "uid",
    "",
};

inline constexpr DataElement kBinary = {
    "binary",
    "",
    "",
    "binary",
    "",
};

inline constexpr DataElement kCWD = {
    "cwd",
    "",
    "",
    "cwd",
    "",
};

inline constexpr DataElement kArguments = {
    "arguments",
    "",
    "",
    "arguments",
    "",
};

inline constexpr DataElement kCapPermitted = {
    "cap_permitted",
    "",
    "",
    "cap.permitted",
    "",
};

inline constexpr DataElement kCapInheritable = {
    "cap_inheritable",
    "",
    "",
    "cap.inheritable",
    "",
};

inline constexpr DataElement kCapEffective = {
    "cap_effective",
    "",
    "",
    "cap.effective",
    "",
};

inline constexpr DataElement kCallName = {
    "call_name",
    "",
    "",
    "call_name",
    "",
};

inline constexpr DataElement kEventType = {
    "event_type",
    "event_type",
    "event_type",
    "event_type",
    "",
};

inline constexpr DataElement kParentProcessId = {
    "parent_process_pid",
    "",
    "",
    "parent.pid",
    "parent process pid",
};

inline constexpr DataElement kParentKtime = {
    "parent_ktime",
    "",
    "",
    "parent.ktime",
    "",
};

inline constexpr DataElement kParentExecId = {
    "parent_exec_id",
    "",
    "",
    "parent.exec_id",
    "",
};

inline constexpr DataElement kParentUser = {
    "parent_user",
    "",
    "",
    "parent.user",
    "",
};

inline constexpr DataElement kParentUid = {
    "parent_uid",
    "",
    "",
    "parent.uid",
    "",
};

inline constexpr DataElement kParentBinary = {
    "parent_binary",
    "",
    "",
    "parent.binary",
    "",
};

inline constexpr DataElement kParentCWD = {
    "parent_cwd",
    "",
    "",
    "parent.cwd",
    "",
};

inline constexpr DataElement kParentArguments = {
    "parent_arguments",
    "",
    "",
    "parent.arguments",
    "",
};

inline constexpr DataElement kParentCapPermitted = {
    "parent_cap_permitted",
    "",
    "",
    "parent.cap.permitted",
    "",
};

inline constexpr DataElement kParentCapInheritable = {
    "parent_cap_inheritable",
    "",
    "",
    "parent.cap.inheritable",
    "",
};

inline constexpr DataElement kParentCapEffective = {
    "parent_cap_effective",
    "",
    "",
    "parent.cap.effective",
    "",
};

// for network
inline constexpr DataElement kSaddr = {
    "source.addr",
    "saddr",
    "saddr",
    "network.saddr",
    "source address",
};

inline constexpr DataElement kDaddr = {
    "dest.addr",
    "",
    "",
    "network.daddr",
    "dest address",
};

inline constexpr DataElement kSport = {
    "source.port",
    "",
    "",
    "network.sport",
    "source port",
};

inline constexpr DataElement kState = {
    "state",
    "state",
    "",
    "network.state",
    "connection state",
};

inline constexpr DataElement kDport = {
    "dest.port",
    "",
    "",
    "network.dport",
    "dest port",
};

inline constexpr DataElement kL4Protocol = {
    "protocol",
    "",
    "",
    "network.protocol",
    "L4 protocol",
};

// for file
inline constexpr DataElement kFilePath = {
    "path",
    "",
    "",
    "file.path",
    "file path",
};

} // namespace logtail::ebpf
