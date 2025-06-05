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

#include "apm/ApmInjectRunner.h"

#include <filesystem>

#include "MachineInfoUtil.h"
#include "apm/Types.h"
#include "logger/Logger.h"

namespace logtail::apm {

namespace fs = std::filesystem;

void ApmInjectRunner::Init() {
    if (mStarted) {
        return;
    }
    mThreadPool = std::make_unique<ThreadPool>(1);
    mThreadPool->Start();
#ifndef APSARA_UNIT_TEST_MAIN
    if (!FetchECSMeta(mEcsMeta)) {
        LOG_WARNING(sLogger, ("failed to fetch ecs meta", ""));
    }
#endif
    mStarted = true;
}

void ApmInjectRunner::Stop() {
    if (!mStarted) {
        return;
    }
    std::future<void> result = std::async(std::launch::async, [this]() { mThreadPool->Stop(); });
    if (result.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
        LOG_ERROR(sLogger, ("apm inject runner stop timeout 3 seconds", "forced to stopped, may cause thread leak"));
    } else {
        LOG_INFO(sLogger, ("apm inject runner", "stop successfully"));
    }
    mStarted = false;
}

bool ApmInjectRunner::HasRegisteredPlugins() const {
    return true;
}

void ApmInjectRunner::injectApmAgentInner() {
    // if task is failed and is retryable, re-submit ...

    // else just return and send alarm ...

}
void ApmInjectRunner::removeApmAgentInner() {
    // if task is failed and is retryable, re-submit ...

    // else just return and send alarm ...
}

bool ApmInjectRunner::RemoveApmAgent(const TaskPipelineContext* ctx) {
    const auto& it = mAttachConfigs.find(ctx->GetConfigName());
    if (it == mAttachConfigs.end()) {
        LOG_DEBUG(sLogger, ("inject config does not exist", ctx->GetConfigName()));
        return true;
    }

    mDeletedConfigs.emplace_back(ctx->GetConfigName(), std::move(it->second));
    mAttachConfigs.erase(it);
    return true;
}

bool ApmInjectRunner::InjectApmAgent(const TaskPipelineContext* ctx, std::unique_ptr<AttachConfig>&& conf) {
    std::unique_ptr<AttachContext> attachContext = std::make_unique<AttachContext>(std::move(conf));
    mAttachConfigs.emplace(ctx->GetConfigName(), attachContext);

    // submit to thread pool

    bool res = false;
    // async prepare ...
    fs::path agentPath;
    res = mPackageMgr.PrepareAPMAgent(
        attachContext->mAttachConfig->mLanguage, attachContext->mAttachConfig->mAppId, std::string(mEcsMeta.GetRegionID()), attachContext->mAttachConfig->mAgentVersion, agentPath);
    if (!res) {
        // TODO @qianlu.kk send alarm ...
        return false;
    }

    // setup exec-hook
    res = mPackageMgr.InstallExecHook(std::string(mEcsMeta.GetRegionID()));
    if (!res) {
        // TODO @qianlu.kk send alarm ...
        return false;
    }

    for (auto& rule : attachContext->mAttachConfig->mMatchRules) {
        bool res = mAttachMgr.DoAttach(rule, agentPath, attachContext);
        if (!res) {
            // TODO @qianlu.kk send alarm ...
        }
    }
    return true;
}

// std::vector<Proc> ApmInjectRunner::listAllProcess() {}


} // namespace logtail::apm
