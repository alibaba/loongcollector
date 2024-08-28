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

#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <gflags/gflags.h>

#include "app_config/AppConfig.h"
#include "ebpf/config.h"
#include "ebpf/eBPFServer.h"
#include "logger/Logger.h"
#include "ebpf/include/export.h"
#include "common/LogtailCommonFlags.h"

namespace logtail {
namespace ebpf {

void eBPFServer::Init() {
    if (mInited) {
        return;
    }
    mSourceManager = std::make_unique<SourceManager>();
    mSourceManager->Init();
    // ebpf config
    auto configJson = AppConfig::GetInstance()->GetConfig();
    mAdminConfig.LoadEbpfConfig(configJson);
    mEventCB = std::make_unique<EventHandler>(nullptr, -1, 0);
#ifdef __ENTERPRISE__
    mMeterCB = std::make_unique<ArmsMeterHandler>(nullptr, -1, 0);
    mSpanCB = std::make_unique<ArmsSpanHandler>(nullptr, -1, 0);
#else
    mMeterCB = std::make_unique<OtelMeterHandler>(nullptr, -1, 0);
    mSpanCB = std::make_unique<OtelSpanHandler>(nullptr, -1, 0);
#endif

    mNetworkSecureCB = std::make_unique<SecurityHandler>(nullptr, -1, 0);
    mProcessSecureCB = std::make_unique<SecurityHandler>(nullptr, -1, 0);
    mFileSecureCB = std::make_unique<SecurityHandler>(nullptr, -1, 0);
    mInited = true;
}

void eBPFServer::Stop() {
    LOG_INFO(sLogger, ("begin to stop all plugins", ""));
    mSourceManager->StopAll();
    // UpdateContext must after than StopPlugin
    if (mEventCB) mEventCB->UpdateContext(nullptr, -1, -1);
    if (mMeterCB) mMeterCB->UpdateContext(nullptr, -1, -1);
    if (mSpanCB) mSpanCB->UpdateContext(nullptr,-1, -1);
    if (mNetworkSecureCB) mNetworkSecureCB->UpdateContext(nullptr,-1, -1);
    if (mProcessSecureCB) mProcessSecureCB->UpdateContext(nullptr,-1, -1);
    if (mFileSecureCB) mFileSecureCB->UpdateContext(nullptr, -1, -1);
}

bool eBPFServer::StartPluginInternal(const std::string& pipeline_name, uint32_t plugin_index,
                        nami::PluginType type, 
                        const logtail::PipelineContext* ctx, 
                        const std::variant<SecurityOptions*, nami::ObserverNetworkOption*> options) {

    std::string prev_pipeline_name = CheckLoadedPipelineName(type);
    if (prev_pipeline_name.size() && prev_pipeline_name != pipeline_name) {
        LOG_WARNING(sLogger, ("pipeline already loaded, plugin type", int(type))("prev pipeline", prev_pipeline_name)("curr pipeline", pipeline_name));
        return false;
    }

    UpdatePipelineName(type, pipeline_name);

    // step1: convert options to export type
    std::variant<nami::NetworkObserveConfig, nami::ProcessConfig, nami::NetworkSecurityConfig, nami::FileSecurityConfig> config;
    bool ret = false;
    // call update function
    // step2: call init function
    switch(type) {
    case nami::PluginType::PROCESS_SECURITY: {
        nami::ProcessConfig pconfig;
        pconfig.process_security_cb_ = [this](auto events) { return mProcessSecureCB->handle(std::move(events)); };
        SecurityOptions* opts = std::get<SecurityOptions*>(options);
        pconfig.options_ = opts->mOptionList;
        config = std::move(pconfig);
        // UpdateContext must ahead of StartPlugin
        mProcessSecureCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
        ret = mSourceManager->StartPlugin(type, config);
        break;
    }

    case nami::PluginType::NETWORK_OBSERVE:{
        nami::NetworkObserveConfig nconfig;
        nami::ObserverNetworkOption* opts = std::get<nami::ObserverNetworkOption*>(options);
        if (opts->mEnableMetric) {
            nconfig.measure_cb_ = [this](auto events, auto ts) { return mMeterCB->handle(std::move(events), ts); };
            mMeterCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
        }
        if (opts->mEnableSpan) {
            nconfig.span_cb_ = [this](auto events) { return mSpanCB->handle(std::move(events)); };
            mSpanCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
        }
        if (opts->mEnableLog) {
            nconfig.event_cb_ = [this](auto events) { return mEventCB->handle(std::move(events)); };
            mEventCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
        }

        config = std::move(nconfig);
        ret = mSourceManager->StartPlugin(type, config);
        break;
    }

    case nami::PluginType::NETWORK_SECURITY:{
        nami::NetworkSecurityConfig nconfig;
        nconfig.network_security_cb_ = [this](auto events) { return mNetworkSecureCB->handle(std::move(events)); };
        SecurityOptions* opts = std::get<SecurityOptions*>(options);
        nconfig.options_ = opts->mOptionList;
        config = std::move(nconfig);
        // UpdateContext must ahead of StartPlugin
        mNetworkSecureCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
        ret = mSourceManager->StartPlugin(type, config);
        break;
    }

    case nami::PluginType::FILE_SECURITY:{
        nami::FileSecurityConfig fconfig;
        fconfig.file_security_cb_ = [this](auto events) { return mFileSecureCB->handle(std::move(events)); };
        SecurityOptions* opts = std::get<SecurityOptions*>(options);
        fconfig.options_ = opts->mOptionList;
        config = std::move(fconfig);
        // UpdateContext must ahead of StartPlugin
        mFileSecureCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
        ret = mSourceManager->StartPlugin(type, config);
        break;
    }
    default:
        LOG_ERROR(sLogger, ("unknown plugin type", int(type)));
        return false;
    }

    return ret;
}

bool eBPFServer::EnablePlugin(const std::string& pipeline_name, uint32_t plugin_index,
                        nami::PluginType type, 
                        const PipelineContext* ctx, 
                        const std::variant<SecurityOptions*, nami::ObserverNetworkOption*> options) {
    return StartPluginInternal(pipeline_name, plugin_index, type, ctx, options);
}

bool eBPFServer::DisablePlugin(const std::string& pipeline_name, nami::PluginType type) {
    std::string prev_pipeline = CheckLoadedPipelineName(type);
    if (prev_pipeline == pipeline_name) {
        UpdatePipelineName(type, "");
    }
    else {
        LOG_WARNING(sLogger, ("prev pipeline", prev_pipeline)("curr pipeline", pipeline_name));
        return true;
    }
    bool ret = mSourceManager->StopPlugin(type);
    // UpdateContext must after than StopPlugin
    if (ret) UpdateCBContext(type, nullptr, -1, -1);
    return ret;
}

std::string eBPFServer::CheckLoadedPipelineName(nami::PluginType type) {
    std::lock_guard<std::mutex> lk(mMtx);
    return mLoadedPipeline[int(type)];
}

void eBPFServer::UpdatePipelineName(nami::PluginType type, const std::string& name) {
    std::lock_guard<std::mutex> lk(mMtx);
    mLoadedPipeline[int(type)] = name;
    return;
}

bool eBPFServer::SuspendPlugin(const std::string& pipeline_name, nami::PluginType type) {
    // mark plugin status is update
    bool ret = mSourceManager->SuspendPlugin(type);
    if (ret) UpdateCBContext(type, nullptr, -1, -1);
    return ret;
}

void eBPFServer::UpdateCBContext(nami::PluginType type, const logtail::PipelineContext* ctx, logtail::QueueKey key, int idx) {
    switch (type) {
    case nami::PluginType::PROCESS_SECURITY:{
        if (mProcessSecureCB) mProcessSecureCB->UpdateContext(ctx, key, idx);
        return;
    }
    case nami::PluginType::NETWORK_OBSERVE:{
        if (mMeterCB) mMeterCB->UpdateContext(ctx, key, idx);
        if (mSpanCB) mSpanCB->UpdateContext(ctx, key, idx);
        if (mEventCB) mEventCB->UpdateContext(ctx, key, idx);
        return;
    }
    case nami::PluginType::NETWORK_SECURITY:{
        if (mNetworkSecureCB) mNetworkSecureCB->UpdateContext(ctx, key, idx);
        return;
    }
    case nami::PluginType::FILE_SECURITY:{
        if (mFileSecureCB) mFileSecureCB->UpdateContext(ctx, key, idx);
        return;
    }
    default:
        return;
    }
}

} // namespace ebpf
}
