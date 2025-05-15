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

#include "apm/AgentOperationRunner.h"
#include "apm/Types.h"
#include "logger/Logger.h"

namespace logtail::apm {

void AgentOperationRunner::Init() {
    // do load ... 
    mHookMgr.LoadExecHook();
}

void AgentOperationRunner::Stop() {}

bool AgentOperationRunner::HasRegisteredPlugins() const {}

bool AgentOperationRunner::DoAttach(const CollectionPipelineContext* ctx, uint32_t pluginIndex, AttachConfig& config) {
    // async prepare ... 
    mPackageMgr.CheckAndDownloadAPMAgent(config.mLanguage, config.mAgentVersion);
    for (auto& rule : config.mMatchRules) {
        std::vector<int> pids;
        int ret = findPidsByRule(rule, pids);
        if (ret) {
            // failed to find pids ... 
            LOG_WARNING(sLogger, ("failed to find pids for rule", ""));
            continue;
        }
        if (pids.empty()) {
            continue;
        }

        for (int pid : pids) {
            mAttachMgr.DoAttach(ctx, pluginIndex, config, pid);
        }
    }
}

int AgentOperationRunner::findPidsByRule(MatchRule& rule, std::vector<int>& pids) {
    switch (rule.mRuleType)
    {
    case RuleType::kCwd: {
        /* code */
        break;
    }
    default:
        break;
    }

    return 0;
}

// std::vector<Proc> AgentOperationRunner::listAllProcess() {}



}
