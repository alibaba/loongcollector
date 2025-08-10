
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

#include "ebpf/EBPFServer.h"
#include "ebpf/include/export.h"
#include "logger/Logger.h"

namespace logtail {

const std::string InputCpuProfiling::sName = "input_cpu_profiling";

bool InputCpuProfiling::Init(const Json::Value &config,
                             Json::Value &optionalGoPipeline) {
    // TODO: add metrics
    return mCpuProfilingOption.Init(config, mContext, sName);
}

bool InputCpuProfiling::Start() {
    ebpf::EBPFServer::GetInstance()->Init();
    if (!ebpf::EBPFServer::GetInstance()->IsSupportedEnv(
            logtail::ebpf::PluginType::CPU_PROFILING)) {
        return false;
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
    return ebpf::EBPFServer::GetInstance()->DisablePlugin(
        mContext->GetConfigName(), logtail::ebpf::PluginType::CPU_PROFILING);
}

} // namespace logtail
