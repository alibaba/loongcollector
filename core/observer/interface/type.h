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

#include <stdint.h>
#include <string>


enum class ObserverMetricsType : uint8_t {
    L4_METRICS = 0,
    L7_DB_METRICS = 1,
    L7_REQ_METRICS = 2,
};

inline std::string ObserverMetricsTypeToString(ObserverMetricsType type) {
    switch (type) {
        case ObserverMetricsType::L4_METRICS:
            return "l4";
        case ObserverMetricsType::L7_DB_METRICS:
            return "l7_db";
        case ObserverMetricsType::L7_REQ_METRICS:
            return "l7_req";
        default:
            break;
    }
    return "unknown";
}


// 协议的消息类型，包括Request、Response。 TODO : Produce Consume
enum MessageType { MessageType_None, MessageType_Request, MessageType_Response };

inline std::string MessageTypeToString(MessageType type) {
    switch (type) {
        case MessageType_Request:
            return "Request";
        case MessageType_Response:
            return "Response";
        default:
            break;
    }
    return "None";
}

enum class ServiceCategory {
    Unknown,
    DB,
    MQ,
    Server,
    DNS,
};

inline std::string ServiceCategoryToString(ServiceCategory type) {
    switch (type) {
        case ServiceCategory::DB:
            return "database";
        case ServiceCategory::MQ:
            return "mq";
        case ServiceCategory::Server:
            return "server";
        case ServiceCategory::DNS:
            return "dns";
        default:
            return "unknown";
    }
}

// 数据包的类型，In代表接收，例如 Recv/Read接收到的数据；Out代表发送，例如
// Send/Write发送出去的数据
enum PacketType { PacketType_None, PacketType_In, PacketType_Out };

enum class PacketRoleType : uint8_t {
    Unknown = 0,
    Client = 1,
    Server = 2,
};

inline std::string PacketRoleTypeToString(PacketRoleType type) {
    switch (type) {
        case PacketRoleType::Unknown:
            return "u";
        case PacketRoleType::Client:
            return "c";
        case PacketRoleType::Server:
            return "s";
        default:
            return "u";
    }
}

inline std::string PacketTypeToString(PacketType type) {
    switch (type) {
        case PacketType_In:
            return "In";
        case PacketType_Out:
            return "Out";
        default:
            break;
    }
    return "None";
}

// 连接的类型，被动Accept的代表是Server，主动connect的代表是Client
enum ConnectionType { ConnectionType_None = 0x01, ConnectionType_Client = 0x02, ConnectionType_Server = 0x04 };

inline std::string ConnectionTypeToString(ConnectionType type) {
    switch (type) {
        case ConnectionType_Client:
            return "Client";
        case ConnectionType_Server:
            return "Server";
        default:
            break;
    }
    return "None";
}

// 应用层协议类型，
// Don't change the protocol order, which is the same as the order in the
// ebpflib.
enum ProtocolType {
    ProtocolType_None,
    ProtocolType_HTTP,
    ProtocolType_MySQL,
    ProtocolType_DNS,
    ProtocolType_Redis,
    ProtocolType_Kafka,
    ProtocolType_PgSQL,
    ProtocolType_Mongo,
    ProtocolType_Dubbo,
    ProtocolType_HSF,
    ProtocolType_NumProto,
};

inline bool IsRemoteInvokeProtocolType(ProtocolType type) {
    switch (type) {
        case ProtocolType_HTTP:
        case ProtocolType_Dubbo:
        case ProtocolType_HSF: {
            return true;
        }
        default:
            return false;
    }
}

inline std::string ProtocolTypeToString(ProtocolType type) {
    switch (type) {
        case ProtocolType_HTTP:
            return "http";
        case ProtocolType_DNS:
            return "dns";
        case ProtocolType_MySQL:
            return "mysql";
        case ProtocolType_Redis:
            return "redis";
        case ProtocolType_Kafka:
            return "kafka";
        case ProtocolType_PgSQL:
            return "pgsql";
        case ProtocolType_Mongo:
            return "mongo";
        case ProtocolType_Dubbo:
            return "dubbo";
        case ProtocolType_HSF:
            return "hsf";
        default:
            break;
    }
    return "none";
}

inline ServiceCategory DetectRemoteServiceCategory(ProtocolType protocolType) {
    switch (protocolType) {
        case ProtocolType_MySQL:
        case ProtocolType_Redis:
        case ProtocolType_PgSQL:
        case ProtocolType_Mongo: {
            return ServiceCategory::DB;
        }
        case ProtocolType_Kafka: {
            return ServiceCategory::MQ;
        }
        case ProtocolType_HTTP:
        case ProtocolType_Dubbo:
        case ProtocolType_HSF: {
            return ServiceCategory::Server;
        }
        case ProtocolType_DNS: {
            return ServiceCategory::DNS;
        }
        default: {
            return ServiceCategory::Unknown;
        }
    }
}
