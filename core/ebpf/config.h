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

#include <string>
#include <json/json.h>
#include <variant>
#include <vector>

#include "pipeline/PipelineContext.h"
#include "ebpf/include/export.h"


namespace logtail {
namespace ebpf {

/////////////////////  /////////////////////

enum class ObserverType { PROCESS, FILE, NETWORK };
bool InitObserverNetworkOption(const Json::Value& config, 
                               nami::ObserverNetworkOption& thisObserverNetworkOption,
                               const PipelineContext* mContext,
                               const std::string& sName);

/////////////////////  /////////////////////

enum class SecurityFilterType { PROCESS, FILE, NETWORK };

class SecurityOptions {
public:
    bool Init(SecurityFilterType filterType,
              const Json::Value& config,
              const PipelineContext* mContext,
              const std::string& sName);

    std::vector<nami::SecurityOption> mOptionList;
    SecurityFilterType mFilterType;
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
    eBPFAdminConfig() {}
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
    int32_t mReceiveEventChanCap;
    AdminConfig mAdminConfig;

    AggregationConfig mAggregationConfig;

    ConverageConfig mConverageConfig;

    SampleConfig mSampleConfig;

    SocketProbeConfig mSocketProbeConfig;

    ProfileProbeConfig mProfileProbeConfig;

    ProcessProbeConfig mProcessProbeConfig;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif

};

} // ebpf
} // logtail
