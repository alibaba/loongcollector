/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "plugin/interface/Flusher.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include <json/json.h>

#include "common/LogstoreFeedbackKey.h"
#include "log_pb/sls_logs.pb.h"

namespace logtail {

class FlusherSLS : public Flusher {
public:
    enum class CompressType { NONE, LZ4, ZSTD };

    struct Batch {
        enum class MergeType { TOPIC, LOGSTORE };

        Batch();

        MergeType mMergeType = MergeType::TOPIC;
        uint32_t mSendIntervalSecs;
        std::vector<std::string> mShardHashKeys;
    };

    static const std::string sName;

    FlusherSLS();

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    LogstoreFeedBackKey GetLogstoreKey() const { return mLogstoreKey; }

    std::string mProject;
    std::string mLogstore;
    std::string mRegion;
    std::string mEndpoint;
    std::string mAliuid;
    CompressType mCompressType = CompressType::LZ4;
    sls_logs::SlsTelemetryType mTelemetryType = sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_LOGS;
    uint32_t mFlowControlExpireTime = 0;
    int32_t mMaxSendRate = -1;
    Batch mBatch;

private:
    static const std::unordered_set<std::string> sNativeParam;

    void GenerateGoPlugin(const Json::Value& config, Json::Value& res) const;

    LogstoreFeedBackKey mLogstoreKey = 0;
};

} // namespace logtail
