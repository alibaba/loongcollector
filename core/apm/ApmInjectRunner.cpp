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

#include "MachineInfoUtil.h"
#include "apm/Types.h"
#include "logger/Logger.h"

namespace logtail::apm {

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

bool ApmInjectRunner::DoAttach(const TaskPipelineContext* ctx, AttachConfig& config) {
    // re-try queue
    // store in map ...

    bool res = false;
    // async prepare ...
    std::string agentPath;
    res = mPackageMgr.PrepareAPMAgent(config.mLanguage, config.mAgentVersion, agentPath);
    if (!res) {
        // TODO @qianlu.kk send alarm ...
        return false;
    }

    // setup exec-hook
    res = mPackageMgr.InstallExecHook();
    if (!res) {
        // TODO @qianlu.kk send alarm ...
        return false;
    }

    for (auto& rule : config.mMatchRules) {
        std::vector<int> pids;
        int ret = findPidsByRule(rule, pids);
        if (ret) {
            // failed to find pids ...
            LOG_WARNING(sLogger, ("failed to find pids for rule", ""));
            // TODO @qianlu.kk send alarm ...
            continue;
        }
        if (pids.empty()) {
            // do we need report ??
            continue;
        }

        for (int pid : pids) {
            bool res = mAttachMgr.DoAttach(rule, agentPath, config, pid);
            if (!res) {
                // TODO @qianlu.kk send alarm ...
            }
        }
    }
    return true;
}

int ApmInjectRunner::findPidsByRule(MatchRule& rule, std::vector<int>& pids) {
    switch (rule.mRuleType) {
        case RuleType::kCwd: {
            /* code */
            break;
        }
        default:
            break;
    }

    return 0;
}

// std::vector<Proc> ApmInjectRunner::listAllProcess() {}


} // namespace logtail::apm
