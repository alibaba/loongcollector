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

extern "C" {
#include <coolbpf/net.h>
}
#include <cstddef>

#include <map>
#include <string>
#include <vector>

namespace logtail {
namespace ebpf {

struct CaseInsensitiveLess {
    struct NoCaseCompare {
        bool operator()(const unsigned char c1, const unsigned char c2) const {
            return std::tolower(c1) < std::tolower(c2);
        }
    };

    template <typename TStringType>
    bool operator()(const TStringType& s1, const TStringType& s2) const {
        return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), NoCaseCompare());
    }
};

using HeadersMap = std::multimap<std::string, std::string, CaseInsensitiveLess>;

inline enum support_proto_e& operator++(enum support_proto_e& pt) {
    pt = static_cast<enum support_proto_e>(static_cast<int>(pt) + 1);
    return pt;
}

inline enum support_proto_e operator++(enum support_proto_e& pt, int) {
    enum support_proto_e old = pt;
    pt = static_cast<enum support_proto_e>(static_cast<int>(pt) + 1);
    return old;
}

class ConnId {
public:
    int32_t fd;
    uint32_t tgid;
    uint64_t start;

    ConnId(int32_t fd, uint32_t tgid, uint64_t start) : fd(fd), tgid(tgid), start(start) {}

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

    bool operator==(const ConnId& other) const { return fd == other.fd && tgid == other.tgid && start == other.start; }
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

} // namespace ebpf
} // namespace logtail

namespace std {
template <>
struct hash<support_proto_e> {
    std::size_t operator()(const support_proto_e& proto) const noexcept { return static_cast<std::size_t>(proto); }
};
} // namespace std


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
} // namespace std
