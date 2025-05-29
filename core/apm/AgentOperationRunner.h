/*
 * Copyright 2025 iLogtail Authors
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

#include <array>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <variant>

#include "apm/AttachManager.h"
#include "apm/PackageManager.h"
#include "apm/Types.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/ProcParser.h"
#include "monitor/metric_models/MetricTypes.h"
#include "runner/InputRunner.h"
#include "common/ThreadPool.h"

namespace logtail::apm {

class AgentOperationRunner : public InputRunner {
public:
    static AgentOperationRunner* GetInstance() {
        static AgentOperationRunner sInstance;
        return &sInstance;
    }

    AgentOperationRunner(const AgentOperationRunner&) = delete;
    AgentOperationRunner(AgentOperationRunner&&) = delete;
    AgentOperationRunner& operator=(const AgentOperationRunner&) = delete;
    AgentOperationRunner& operator=(AgentOperationRunner&&) = delete;

    explicit AgentOperationRunner() = default;

    virtual ~AgentOperationRunner() = default;

    void Init() override;
    void Stop() override;
    [[nodiscard]] bool HasRegisteredPlugins() const override;
    void EventGC() override {}

    bool DoAttach(AttachConfig& config);

private:
    int findPidsByRule(MatchRule& rule, std::vector<int>& pids);

    AttachManager mAttachMgr;
    PackageManager mPackageMgr;
    std::unique_ptr<ThreadPool> mThreadPool;
};

} // namespace logtail::apm
