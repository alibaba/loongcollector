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

#include "collection_pipeline/plugin/interface/Input.h"

namespace logtail {

class InputHostMeta : public Input {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    QueueType GetProcessQueueType() const override { return QueueType::COUNT_BOUNDED; }

private:
    uint32_t mInterval = 0;

    // 进程实体采集开关（默认启用）
    bool mEnableProcessEntity = true;

    uint32_t mFullReportInterval = 3600;
    uint32_t mIncrementalInterval = 10;

    // 过滤配置（始终应用过滤逻辑）
    bool mExcludeKernelThreads = true; // 默认排除内核线程
    uint32_t mMinRunningTimeSeconds = 20; // 默认过滤运行时间<20s的进程，0表示不过滤
    std::vector<std::string> mWhitelistPatterns; // 默认为空，不空才过滤
    std::vector<std::string> mBlacklistPatterns; // 默认为空，不空才过滤

    // 扩展属性采集开关
    bool mEnableListeningPorts = false; // 默认关闭，监听端口采集（性能开销较大，只在全量上报时采集）

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputHostMetaUnittest;
#endif
};

} // namespace logtail
