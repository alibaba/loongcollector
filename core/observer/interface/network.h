/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "interface/type.h"
#include <cstring>
#include <string>

enum class SocketCategory {
    UnknownSocket,
    InetSocket,
    UnixSocket,
};

inline std::string SocketCategoryToString(SocketCategory type) {
    switch (type) {
        case SocketCategory::InetSocket:
            return "inet_socket";
        case SocketCategory::UnixSocket:
            return "unix_socket";
        default:
            return "unknown_socket";
    }
}

// TCP状态维护
// map[sock-key] -> tcp_conn_state
// sock-key : tgid << 32 | skc_hash
// uint32_t tgid = bpf_get_current_pid_tgid() >> 32
// https://github.com/y123456yz/Reading-and-comprehense-linux-Kernel-network-protocol-stack/blob/master/linux-net-kernel/include/net/sock.h#L189

enum SockAddressType { SockAddressType_IPV4, SockAddressType_IPV6 };

// If T is a union type, the object's first non-static named data member is
// zero-initialized and padding is initialized to zero bits.
union SockAddressDetail {
    uint64_t IPV6[2]; // IPV6最长，因此需要放在最前面
    uint32_t IPV4;
};

struct SockAddress {
    SockAddressType Type;
    SockAddressDetail Addr;

    bool operator==(const SockAddress& rhs) const {
        return Type == rhs.Type && std::memcmp(Addr.IPV6, rhs.Addr.IPV6, sizeof(SockAddressDetail)) == 0;
    }

    bool operator!=(const SockAddress& rhs) const { return !(rhs == *this); }
};

enum PacketEventType {
    PacketEventType_None, // 用于一些空循环
    PacketEventType_Data,
    PacketEventType_Connected,
    PacketEventType_Accepted,
    PacketEventType_Closed
};

inline std::string PacketEventTypeToString(enum PacketEventType type) {
    switch (type) {
        case PacketEventType_None:
            return "None";
        case PacketEventType_Data:
            return "Data";
        case PacketEventType_Connected:
            return "Connected";
        case PacketEventType_Accepted:
            return "Accepted";
        case PacketEventType_Closed:
            return "Closed";
    }
    return "None";
}

// 如果是控制消息（建连、断连），只需要output
// header；如果是数据类型，传PacketEventData
struct PacketEventHeader {
    uint32_t PID;
    uint32_t SockHash; // hashed by local addr + local port + remote addrr +
                       // remote port

    PacketEventType EventType;
    PacketRoleType RoleType;

    uint64_t TimeNano;
    SockAddress SrcAddr; // 永远是本机地址，通过PacketType来计算方向
    uint16_t SrcPort;
    SockAddress DstAddr; // 永远是对端地址
    uint16_t DstPort;
};

// 当EventType为Data类型时的payload表示;  header + data
struct PacketEventData {
    ProtocolType PtlType;
    MessageType MsgType;
    PacketType PktType;

    int32_t RealLen; // 原始的数据包大小
    int32_t BufferLen; // 实际拷贝的数据包大小，即Buffer的Size

    char* Buffer; // 一般设置为 buffer + sizeof(PacketEventHeader) +
                  // sizeof(PacketEventData)
};


struct ConnectionAddrInfo {
    SockAddress LocalAddr{};
    SockAddress RemoteAddr{};
    uint16_t LocalPort{0};
    uint16_t RemotePort{0};
};