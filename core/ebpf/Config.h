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

#include <string>
#include <variant>
#include <vector>

#include "json/json.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "ebpf/include/export.h"

namespace logtail::ebpf {

/////////////////////  /////////////////////

enum class ObserverType { PROCESS, FILE, NETWORK };
bool InitObserverNetworkOption(const Json::Value& config,
                               ObserverNetworkOption& thisObserverNetworkOption,
                               const CollectionPipelineContext* mContext,
                               const std::string& sName);

/////////////////////  /////////////////////

enum class SecurityProbeType { PROCESS, FILE, NETWORK, MAX };

class SecurityOptions {
public:
    bool Init(SecurityProbeType filterType,
              const Json::Value& config,
              const CollectionPipelineContext* mContext,
              const std::string& sName);

    std::vector<SecurityOption> mOptionList;
    SecurityProbeType mProbeType;
};

///////////////////// Process Level Config /////////////////////

struct AdminConfig {
    bool mDebugMode;
    std::string mLogLevel;
    bool mPushAllSpan;
};

struct AggregationConfig {
    int32_t mAggWindowSecond;
};

struct ConverageConfig {
    std::string mStrategy;
};

struct SampleConfig {
    std::string mStrategy;
    struct Config {
        double mRate;
    } mConfig;
};

struct SocketProbeConfig {
    int32_t mSlowRequestThresholdMs;
    int32_t mMaxConnTrackers;
    int32_t mMaxBandWidthMbPerSec;
    int32_t mMaxRawRecordPerSec;
};

struct ProfileProbeConfig {
    int32_t mProfileSampleRate;
    int32_t mProfileUploadDuration;
};

struct ProcessProbeConfig {
    bool mEnableOOMDetect;
};

class eBPFAdminConfig {
public:
    eBPFAdminConfig() = default;
    ~eBPFAdminConfig() {}

    void LoadEbpfConfig(const Json::Value& confJson);

    int32_t GetReceiveEventChanCap() const { return mReceiveEventChanCap; }

    const AdminConfig& GetAdminConfig() const { return mAdminConfig; }

    const AggregationConfig& GetAggregationConfig() const { return mAggregationConfig; }

    const ConverageConfig& GetConverageConfig() const { return mConverageConfig; }

    const SampleConfig& GetSampleConfig() const { return mSampleConfig; }

    const SocketProbeConfig& GetSocketProbeConfig() const { return mSocketProbeConfig; }

    const ProfileProbeConfig& GetProfileProbeConfig() const { return mProfileProbeConfig; }

    const ProcessProbeConfig& GetProcessProbeConfig() const { return mProcessProbeConfig; }

private:
    int32_t mReceiveEventChanCap = 0;
    AdminConfig mAdminConfig;

    AggregationConfig mAggregationConfig{};

    ConverageConfig mConverageConfig;

    SampleConfig mSampleConfig;

    SocketProbeConfig mSocketProbeConfig{};

    ProfileProbeConfig mProfileProbeConfig{};

    ProcessProbeConfig mProcessProbeConfig{};
#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
};

} // namespace logtail::ebpf
