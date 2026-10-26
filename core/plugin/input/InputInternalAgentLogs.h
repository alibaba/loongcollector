/*
 * Copyright 2024 iLogtail Authors
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

#include <cstdint>

#include <string>
#include <vector>

#include "collection_pipeline/plugin/interface/Input.h"

namespace logtail {

class InputInternalAgentLogs : public Input {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    QueueType GetProcessQueueType() const override { return QueueType::COUNT_BOUNDED; }
    bool ExpandAdditionalInputs(size_t startIdx, std::vector<std::unique_ptr<InputInstance>>& extras) override;

    static std::string InferArtifact(const std::string& path);

private:
    enum class RuntimeLogKind { None, Cpp, Go };

    bool createStaticFileInput(size_t inputIdx,
                               const Json::Value& groupConfig,
                               RuntimeLogKind runtimeKind,
                               std::unique_ptr<InputInstance>& extra);
    bool appendAgentLogTagProcessor(std::vector<std::unique_ptr<ProcessorInstance>>& processors);
    bool appendAgentLogMicrotimeProcessor(std::vector<std::unique_ptr<ProcessorInstance>>& processors);
    bool appendRuntimeLogProcessors(std::vector<std::unique_ptr<ProcessorInstance>>& processors,
                                    RuntimeLogKind runtimeKind);
    bool appendProcessor(std::vector<std::unique_ptr<ProcessorInstance>>& processors,
                         const std::string& type,
                         const Json::Value& detail);
    Json::Value buildRuntimeLogsConfig(RuntimeLogKind runtimeKind) const;
    Json::Value buildWholeSmallConfig() const;
    Json::Value buildWholeDirsConfig() const;
    Json::Value buildFileCheckpointConfig() const;
    void alarmCompressedRuntimeLogs() const;

    std::string mAliuid;
    std::vector<std::string> mIPList;
    int64_t mStartTime = 0;
    int64_t mEndTime = 0;
    bool mHasTimeWindow = false;
    bool mSkipCollect = false;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputInternalAgentLogsUnittest;
#endif
};

} // namespace logtail
