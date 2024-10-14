// Copyright 2023 iLogtail Authors
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

#include "plugin/input/InputProcessSecurity.h"

#include "ebpf/include/export.h"
#include "ebpf/eBPFServer.h"

using namespace std;

namespace logtail {

const std::string InputProcessSecurity::sName = "input_process_security";

bool InputProcessSecurity::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    ebpf::eBPFServer::GetInstance()->Init();
    if (!ebpf::eBPFServer::GetInstance()->IsSupportedEnv(nami::PluginType::PROCESS_SECURITY)) {
        return false;
    }
    std::string prev_pipeline_name = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::PROCESS_SECURITY);
    std::string pipeline_name = mContext->GetConfigName();
    if (prev_pipeline_name.size() && prev_pipeline_name != pipeline_name) {
        LOG_WARNING(sLogger, ("pipeline already loaded", "PROCESS_SECURITY")("prev pipeline", prev_pipeline_name)("curr pipeline", pipeline_name));
        return false;
    }
    return mSecurityOptions.Init(ebpf::SecurityProbeType::PROCESS, config, mContext, sName);
}

bool InputProcessSecurity::Start() {
    return ebpf::eBPFServer::GetInstance()->EnablePlugin(mContext->GetConfigName(), mIndex, nami::PluginType::PROCESS_SECURITY,mContext, &mSecurityOptions);
}

bool InputProcessSecurity::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving) {
        ebpf::eBPFServer::GetInstance()->SuspendPlugin(mContext->GetConfigName(), nami::PluginType::PROCESS_SECURITY);
        return true;
    }
    return ebpf::eBPFServer::GetInstance()->DisablePlugin(mContext->GetConfigName(), nami::PluginType::PROCESS_SECURITY);
}

} // namespace logtail
