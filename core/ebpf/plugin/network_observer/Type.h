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

#include <atomic>
#include <map>
#include <string>
#include <vector>

#include "collection_pipeline/queue/QueueKey.h"
#include "common/HashUtil.h"
#include "common/Lock.h"
#include "ebpf/include/export.h"
#include "ebpf/util/sampler/Sampler.h"

namespace logtail::ebpf {

class AppDetail {
public:
    explicit AppDetail(ObserverNetworkOption* opt)
        : mAppName(opt->mApmConfig.mAppName),
          mAppId(opt->mApmConfig.mAppId),
          mWorkspace(opt->mApmConfig.mWorkspace),
          mServiceId(opt->mApmConfig.mServiceId),
          mEnableL7(opt->mL7Config.mEnable),
          mEnableLog(opt->mL7Config.mEnableLog),
          mEnableSpan(opt->mL7Config.mEnableSpan),
          mEnableMetric(opt->mL7Config.mEnableMetric),
          mEnableL4(opt->mL4Config.mEnable),
          mSampleRate(opt->mL7Config.mSampleRate) {
        // init mSampler
        if (mSampleRate < 0) {
            // LOG_WARNING(sLogger,
            //             ("invalid sample rate, must between [0, 1], use default 0.01, given", mSampleRate));
            mSampleRate = 0;
        } else if (mSampleRate >= 1) {
            mSampleRate = 1.0;
        }
        // LOG_INFO(sLogger, ("sample rate", mSampleRate));
        mSampler = std::make_shared<HashRatioSampler>(mSampleRate);
        std::hash<std::string> hasher;
        AttrHashCombine(mAppHash, hasher(mAppName));
        AttrHashCombine(mAppHash, hasher(mAppId));
        AttrHashCombine(mAppHash, hasher(mWorkspace));
        AttrHashCombine(mAppHash, hasher(mServiceId));
    }

    std::string mAppName;
    std::string mAppId;
    std::string mWorkspace;
    std::string mServiceId;

    bool mEnableL7;
    bool mEnableLog;
    bool mEnableSpan;
    bool mEnableMetric;
    bool mEnableL4;

    // sampler ...
    double mSampleRate;
    std::shared_ptr<Sampler> mSampler;
    // plugin queue key ...
    std::string mConfigName;
    QueueKey mQueueKey = 0;
    uint32_t mPluginIndex = -1;

    size_t mAppHash;
};

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

    ~ConnId() {}

    ConnId(int32_t fd, uint32_t tgid, uint64_t start) : fd(fd), tgid(tgid), start(start) {}
    ConnId(const ConnId& other) = default;
    ConnId& operator=(const ConnId& other) {
        if (this != &other) {
            fd = other.fd;
            tgid = other.tgid;
            start = other.start;
        }
        return *this;
    }

    ConnId(ConnId&& other) noexcept : fd(other.fd), tgid(other.tgid), start(other.start) {}
    ConnId& operator=(ConnId&& other) noexcept {
        if (this != &other) {
            fd = other.fd;
            tgid = other.tgid;
            start = other.start;
        }
        return *this;
    }

    explicit ConnId(const struct connect_id_t& connId) : fd(connId.fd), tgid(connId.tgid), start(connId.start) {}

    bool operator==(const ConnId& other) const { return fd == other.fd && tgid == other.tgid && start == other.start; }
};

struct ConnIdHash {
    std::size_t operator()(const ConnId& obj) const {
        std::size_t hashResult = 0UL;
        AttrHashCombine(hashResult, std::hash<int32_t>{}(obj.fd));
        AttrHashCombine(hashResult, std::hash<uint32_t>{}(obj.tgid));
        AttrHashCombine(hashResult, std::hash<uint64_t>{}(obj.start));
        return hashResult;
    }
};

} // namespace logtail::ebpf


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
