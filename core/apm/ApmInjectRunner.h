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
#include "task_pipeline/TaskPipelineContext.h"
#include "common/ProcParser.h"
#include "monitor/metric_models/MetricTypes.h"
#include "runner/InputRunner.h"
#include "common/ThreadPool.h"
#include "task_pipeline/TaskPipelineContext.h"
#include "common/MachineInfoUtil.h"

namespace logtail::apm {

class ApmInjectRunner : public InputRunner {
public:
    static ApmInjectRunner* GetInstance() {
        static ApmInjectRunner sInstance;
        return &sInstance;
    }

    ApmInjectRunner(const ApmInjectRunner&) = delete;
    ApmInjectRunner(ApmInjectRunner&&) = delete;
    ApmInjectRunner& operator=(const ApmInjectRunner&) = delete;
    ApmInjectRunner& operator=(ApmInjectRunner&&) = delete;

    explicit ApmInjectRunner() = default;

    virtual ~ApmInjectRunner() = default;

    void Init() override;
    void Stop() override;
    [[nodiscard]] bool HasRegisteredPlugins() const override;
    void EventGC() override {}

    bool DoAttach(const TaskPipelineContext* ctx, AttachConfig& config);

private:

    // pipeline name ==> AttachConfig

    AttachManager mAttachMgr;
    PackageManager mPackageMgr;
    std::unique_ptr<ThreadPool> mThreadPool;
    std::atomic_bool mStarted = false;
    ECSMeta mEcsMeta;
};

} // namespace logtail::apm
