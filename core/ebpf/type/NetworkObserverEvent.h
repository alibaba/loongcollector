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

#include <cstddef>
#include <coolbpf/net.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <list>

namespace logtail {
namespace ebpf {


enum class CallType {
  UNKNOWN,
  HTTP,
  HTTP_CLIENT,
  MYSQL,
  MYSQL_SERVER,
  DNS,
  DNS_SERVER,
  KAFKA_PRODUCER,
  KAFKA_CONSUMER,
  // others ...
};

enum class ProtocolType {
  UNKNOWN,
  HTTP,
  MYSQL,
  DNS,
  REDIS,
  KAFKA,
  PGSQL,
  MONGO,
  DUBBO,
  HSF,
  MAX,
};


enum class AggregateType {
//  NET_L7,
//  NET_L5,
  NETWORK,
  PROCESS,
  MAX,
};

enum class NetworkDataType {
  APP,
  L5,
  MAX,
};

enum class ProcessDataType {
  MAX,
};


inline ProtocolType& operator++(ProtocolType &pt) {
  pt = static_cast<ProtocolType>(static_cast<int>(pt) + 1);
  return pt;
}

// 后置递增
inline ProtocolType operator++(ProtocolType &pt, int) {
  ProtocolType old = pt;
  pt = static_cast<ProtocolType>(static_cast<int>(pt) + 1);
  return old;
}

enum class EventType {
  UNKNOWN_EVENT = 0,
  CONN_STATS_EVENT = 1,
  HTTP_EVENT = 2,
  MYSQL_EVENT = 3,
  REDIS_EVENT = 4,
  DNS_EVENT = 5,
  PROCESS_EVENT = 6,
  MAX=7,
};

enum class ConvergeType {
  IP,
  URL,
  PORT,
};


class ConnId {
public:
  int32_t fd;
  uint32_t tgid;
  uint64_t start;

  ConnId(int32_t fd, uint32_t tgid, uint64_t start):fd(fd),tgid(tgid), start(start) {}

  ConnId(const ConnId& other) : fd(other.fd), tgid(other.tgid), start(other.start) {}
  ConnId& operator=(const ConnId& other) {
    if (this != &other) {
      fd = other.fd;
      tgid = other.tgid;
      start = other.start;
    }
    return *this;
  }

  ConnId(ConnId&& other) : fd(other.fd), tgid(other.tgid), start(other.start) {}
  ConnId& operator=(ConnId&& other) noexcept {
    if (this != &other) {
      fd = other.fd;
      tgid = other.tgid;
      start = other.start;
    }
    return *this;
  }

  explicit ConnId(const struct connect_id_t& conn_id) : fd(conn_id.fd), tgid(conn_id.tgid), start(conn_id.start) {}

  bool operator==(const ConnId& other) const {
    return fd == other.fd && tgid == other.tgid && start == other.start;
  }
};

struct ConnIdHash {
    inline static void combine(std::size_t& hash_result, std::size_t hash) {
        hash_result ^= hash + 0x9e3779b9 + (hash_result << 6) + (hash_result >> 2);
    }

    std::size_t operator()(const ConnId& obj) const {
        std::size_t hash_result = 0UL;
        combine(hash_result, std::hash<int32_t>{}(obj.fd));
        combine(hash_result, std::hash<uint32_t>{}(obj.tgid));
        combine(hash_result, std::hash<uint64_t>{}(obj.start));
        return hash_result;
    }
};


inline std::string ConnIdToStringHash(const ConnId& conn_id) {
  size_t hash_value = std::hash<ConnId>()(conn_id);
  return std::to_string(hash_value);
}

class NetDataEvent
{
public:
  ConnId conn_id;
  uint64_t start_ts;
  uint64_t end_ts;
  ProtocolType protocol;
  enum support_role_e role;
//  uint16_t request_len;
//  uint16_t response_len;
  std::string req_msg;
  std::string resp_msg;

  explicit NetDataEvent(struct conn_data_event_t* conn_data)
    : conn_id(conn_data->conn_id),
    start_ts(conn_data->start_ts),
    end_ts(conn_data->end_ts),
    protocol(static_cast<ProtocolType>(conn_data->protocol)), role(conn_data->role) {

//    conn_id = ConnId(conn_data->conn_id);
//    size_t offset = offsetof(struct conn_data_event_t, msg);
//    memcpy(&data_event->conn_id, event_data, offset);
    req_msg = std::string(conn_data->msg, conn_data->request_len);
    resp_msg = std::string(conn_data->msg+conn_data->request_len, conn_data->response_len);
//    size_t msg_length = event_data->request_len + event_data->response_len;
//    data_event->msg = std::string(event_data->msg, msg_length);
  }
};

}
}


namespace std {
template <>
struct hash<logtail::ebpf::ProtocolType> {
  std::size_t operator()(const logtail::ebpf::ProtocolType& proto) const noexcept {
    return static_cast<std::size_t>(proto);
  }
};
}


namespace std {
template <>
struct hash<logtail::ebpf::ConnId> {
  std::size_t operator()(const logtail::ebpf::ConnId& k) const {
    std::size_t h1 = std::hash<int32_t>{}(k.fd);
    std::size_t h2 = std::hash<uint32_t>{}(k.tgid);
    std::size_t h3 = std::hash<uint64_t>{}(k.start);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};
}

