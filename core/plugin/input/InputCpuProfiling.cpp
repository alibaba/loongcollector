
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

#include "plugin/input/InputCpuProfiling.h"
#include "container_manager/ContainerManager.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "ebpf/EBPFServer.h"
#include "ebpf/plugin/cpu_profiling/ProcessDiscoveryManager.h"
#include "ebpf/include/export.h"
#include "logger/Logger.h"
#include "app_config/AppConfig.h"
#include <vector>

namespace logtail {

const std::string InputCpuProfiling::sName = "input_cpu_profiling";

static bool noop(ContainerInfo& containerInfo,
                 const CollectionPipelineContext*,
                 const FileDiscoveryOptions*) {
    return true;
}

bool InputCpuProfiling::Init(const Json::Value &config,
                             Json::Value &optionalGoPipeline) {
    // TODO: add metrics
    auto ok = mCpuProfilingOption.Init(config, mContext, sName);
    if (!ok) {
        return false;
    }
    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
        mTempFileDiscoveryOptions.SetEnableContainerDiscoveryFlag(true);
        mTempFileDiscoveryOptions.SetDeduceAndSetContainerBaseDirFunc(noop);
        mCpuProfilingOption.mContainerDiscovery.GenerateContainerMetaFetchingGoPipeline(
            optionalGoPipeline, nullptr, mContext->GetPipeline().GenNextPluginMeta(false));
        mTempFileDiscoveryOptions.SetContainerDiscoveryOptions(std::move(mCpuProfilingOption.mContainerDiscovery));
    }
    return true;
}

bool InputCpuProfiling::Start() {
    ebpf::EBPFServer::GetInstance()->Init();
    if (!ebpf::EBPFServer::GetInstance()->IsSupportedEnv(
            logtail::ebpf::PluginType::CPU_PROFILING)) {
        return false;
    }
    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
        ContainerManager::GetInstance()->Init();
        mTempFileDiscoveryOptions.SetContainerInfo(std::make_shared<std::vector<ContainerInfo>>());
        ContainerManager::GetInstance()->AddContainerHandler(
            mContext->GetConfigName(),
            std::make_pair(&mTempFileDiscoveryOptions, mContext),
            [configName = mContext->GetConfigName()](std::shared_ptr<ContainerDiff> diff) {
                LOG_DEBUG(sLogger, ("config", configName)("container handler", "called"));
                ebpf::ProcessDiscoveryManager::GetInstance()->UpdateDiscovery(
                        configName, [&](ebpf::ProcessDiscoveryConfig& config) {
                            for (const auto& containerId : diff->mRemoved) {
                                config.mContainerIds.erase(containerId);
                            }
                            for (const auto& container : diff->mAdded) {
                                config.mContainerIds.insert(container->mID);
                            }
                        });
            });
    }
    return ebpf::EBPFServer::GetInstance()->EnablePlugin(
        mContext->GetConfigName(), mIndex,
        logtail::ebpf::PluginType::CPU_PROFILING, mContext,
        &mCpuProfilingOption, mPluginMetricPtr);
}

bool InputCpuProfiling::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving) {
        return ebpf::EBPFServer::GetInstance()->SuspendPlugin(
            mContext->GetConfigName(),
            logtail::ebpf::PluginType::CPU_PROFILING);
    }
    ContainerManager::GetInstance()->RemoveContainerHandler(
        mContext->GetConfigName());
    return ebpf::EBPFServer::GetInstance()->DisablePlugin(
        mContext->GetConfigName(), logtail::ebpf::PluginType::CPU_PROFILING);
}

} // namespace logtail
