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

#include "ebpf/eBPFServer.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/Lock.h"
#include "common/LogtailCommonFlags.h"
#include "common/MachineInfoUtil.h"
#include "ebpf/Config.h"
#include "ebpf/include/export.h"
#include "logger/Logger.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"
#include "plugin/network_observer/NetworkObserverManager.h"
#include "metadata/K8sMetadata.h"

DEFINE_FLAG_INT64(kernel_min_version_for_ebpf,
                  "the minimum kernel version that supported eBPF normal running, 4.19.0.0 -> 4019000000",
                  4019000000);

namespace logtail {
namespace ebpf {

static const uint16_t KERNEL_VERSION_310 = 3010; // for centos7
static const std::string KERNEL_NAME_CENTOS = "CentOS";
static const uint16_t KERNEL_CENTOS_MIN_VERSION = 7006;

bool EnvManager::IsSupportedEnv(logtail::ebpf::PluginType type) {
    if (!mInited) {
        LOG_ERROR(sLogger, ("env manager not inited ...", ""));
        return false;
    }
    bool status = false;
    switch (type) {
        case logtail::ebpf::PluginType::NETWORK_OBSERVE:
            status = mArchSupport && (mBTFSupport || m310Support);
            break;
        case logtail::ebpf::PluginType::FILE_SECURITY:
        case logtail::ebpf::PluginType::NETWORK_SECURITY:
        case logtail::ebpf::PluginType::PROCESS_SECURITY: {
            status = mArchSupport && mBTFSupport;
            break;
        }
        default:
            status = false;
    }
    if (!status) {
        LOG_WARNING(sLogger,
                    ("runtime env not supported, plugin type: ", int(type))("arch support is ", mArchSupport)(
                        "btf support is ", mBTFSupport)("310 support is ", m310Support));
    }
    return status;
}

bool EnvManager::AbleToLoadDyLib() {
    return mArchSupport;
}

void EnvManager::InitEnvInfo() {
    if (mInited)
        return;
    mInited = true;

#ifdef _MSC_VER
    LOG_WARNING(sLogger, ("MS", "not supported"));
    mArchSupport = false;
    return;
#elif defined(__aarch64__)
    LOG_WARNING(sLogger, ("aarch64", "not supported"));
    mArchSupport = false;
    return;
#elif defined(__arm__)
    LOG_WARNING(sLogger, ("arm", "not supported"));
    mArchSupport = false;
    return;
#elif defined(__i386__)
    LOG_WARNING(sLogger, ("i386", "not supported"));
    mArchSupport = false;
    return;
#endif
    mArchSupport = true;
    std::string release;
    int64_t version;
    GetKernelInfo(release, version);
    LOG_INFO(sLogger, ("ebpf kernel release", release)("kernel version", version));
    if (release.empty()) {
        LOG_WARNING(sLogger, ("cannot find kernel release", ""));
        mBTFSupport = false;
        return;
    }
    if (version >= INT64_FLAG(kernel_min_version_for_ebpf)) {
        mBTFSupport = true;
        return;
    }
    if (version / 1000000 != KERNEL_VERSION_310) {
        LOG_WARNING(sLogger, ("unsupported kernel version, will not start eBPF plugin ... version", version));
        m310Support = false;
        return;
    }

    std::string os;
    int64_t osVersion;
    if (GetRedHatReleaseInfo(os, osVersion, STRING_FLAG(default_container_host_path))
        || GetRedHatReleaseInfo(os, osVersion)) {
        if (os == KERNEL_NAME_CENTOS && osVersion >= KERNEL_CENTOS_MIN_VERSION) {
            m310Support = true;
            return;
        } else {
            LOG_WARNING(
                sLogger,
                ("unsupported os for 310 kernel, will not start eBPF plugin ...", "")("os", os)("version", osVersion));
            m310Support = false;
            return;
        }
    }
    LOG_WARNING(sLogger, ("not redhat release, will not start eBPF plugin ...", ""));
    m310Support = false;
}

bool eBPFServer::IsSupportedEnv(logtail::ebpf::PluginType type) {
    return mEnvMgr.IsSupportedEnv(type);
}

void eBPFServer::Init() {
    if (mInited) {
        return;
    }
    mEnvMgr.InitEnvInfo();
    if (!mEnvMgr.AbleToLoadDyLib()) {
        return;
    }
    mInited = true;
    // check env

    // mMonitorMgr = std::make_unique<eBPFSelfMonitorMgr>();
    DynamicMetricLabels dynamicLabels;
    dynamicLabels.emplace_back(METRIC_LABEL_KEY_PROJECT, [this]() -> std::string { return this->GetAllProjects(); });
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER}},
        std::move(dynamicLabels));

    mStartPluginTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_START_PLUGIN_TOTAL);
    mStopPluginTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_STOP_PLUGIN_TOTAL);
    mSuspendPluginTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_SUSPEND_PLUGIN_TOTAL);

    mSourceManager = std::make_shared<SourceManager>();
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
}

void eBPFServer::Stop() {
    if (!mInited)
        return;
    mInited = false;
    LOG_INFO(sLogger, ("begin to stop all plugins", ""));
    // destroy source manager
    // do not destroy source manager ...
    // mSourceManager.reset();
    for (int i = 0; i < int(logtail::ebpf::PluginType::MAX); i++) {
        UpdatePipelineName(static_cast<logtail::ebpf::PluginType>(i), "", "");
    }

    // UpdateContext must after than StopPlugin
    if (mEventCB)
        mEventCB->UpdateContext(nullptr, -1, -1);
    if (mMeterCB)
        mMeterCB->UpdateContext(nullptr, -1, -1);
    if (mSpanCB)
        mSpanCB->UpdateContext(nullptr, -1, -1);
    if (mNetworkSecureCB)
        mNetworkSecureCB->UpdateContext(nullptr, -1, -1);
    if (mProcessSecureCB)
        mProcessSecureCB->UpdateContext(nullptr, -1, -1);
    if (mFileSecureCB)
        mFileSecureCB->UpdateContext(nullptr, -1, -1);
}

// maybe update or create 
bool eBPFServer::StartPluginInternal(const std::string& pipeline_name,
                                     uint32_t plugin_index,
                                     PluginType type,
                                     const logtail::CollectionPipelineContext* ctx,
                                     const std::variant<SecurityOptions*, ObserverNetworkOption*> options,
                                     PluginMetricManagerPtr mgr) {
    std::string prev_pipeline_name = CheckLoadedPipelineName(type);
    if (prev_pipeline_name.size() && prev_pipeline_name != pipeline_name) {
        LOG_WARNING(sLogger,
                    ("pipeline already loaded, plugin type",
                     int(type))("prev pipeline", prev_pipeline_name)("curr pipeline", pipeline_name));
        return false;
    }

    UpdatePipelineName(type, pipeline_name, ctx->GetProjectName());

    // init self monitor
    // mMonitorMgr->Init(type, mgr, pipeline_name, ctx->GetProjectName());

    // step1: convert options to export type
    bool ret = false;
    auto eBPFConfig = std::make_unique<logtail::ebpf::PluginConfig>();
    eBPFConfig->mPluginType = type;
    // call update function
    // step2: call init function
    switch (type) {
        case logtail::ebpf::PluginType::PROCESS_SECURITY: {
            logtail::ebpf::ProcessConfig pconfig;
            // TODO @qianlu.kk set new handler ...

            // pconfig.process_security_cb_ = [this](std::vector<std::unique_ptr<AbstractSecurityEvent>>& events) {
            //     return mProcessSecureCB->handle(events);
            // };
            SecurityOptions* opts = std::get<SecurityOptions*>(options);
            pconfig.options_ = opts->mOptionList;
            // UpdateContext must ahead of StartPlugin
            mProcessSecureCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
            eBPFConfig->mConfig = std::move(pconfig);
            ret = mSourceManager->StartPlugin(type, std::move(eBPFConfig));
            break;
        }

        case logtail::ebpf::PluginType::NETWORK_OBSERVE: {
            logtail::ebpf::NetworkObserveConfig nconfig;

            // TODO @qianlu.kk register k8s metadata callback for metric ??
            
            mEventCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
            auto idx = static_cast<int>(PluginType::NETWORK_OBSERVE);
            mPlugins[idx] = NetworkObserverManager::Create(
                mBaseManager, 
                mSourceManager, 
                [&](const std::vector<std::unique_ptr<ApplicationBatchEvent>>& events) {
                    mEventCB->handle(events);
                }
            );

            ret = (mPlugins[idx]->Init(options) == 0);
            break;
        }

        case logtail::ebpf::PluginType::NETWORK_SECURITY: {
            logtail::ebpf::NetworkSecurityConfig nconfig;
            // TODO @qianlu.kk set new handler ...

            // nconfig.network_security_cb_ = [this](std::vector<std::unique_ptr<AbstractSecurityEvent>>& events) {
            //     return mNetworkSecureCB->handle(events);
            // };
            // SecurityOptions* opts = std::get<SecurityOptions*>(options);
            // nconfig.options_ = opts->mOptionList;
            // eBPFConfig->mConfig = std::move(nconfig);
            // // UpdateContext must ahead of StartPlugin
            // mNetworkSecureCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
            // ret = mSourceManager->StartPlugin(type, std::move(eBPFConfig));
            break;
        }

        case logtail::ebpf::PluginType::FILE_SECURITY: {
            logtail::ebpf::FileSecurityConfig fconfig;
            // TODO @qianlu.kk set new handler ...

            // fconfig.file_security_cb_ = [this](std::vector<std::unique_ptr<AbstractSecurityEvent>>& events) {
            //     return mFileSecureCB->handle(events);
            // };
            // SecurityOptions* opts = std::get<SecurityOptions*>(options);
            // fconfig.options_ = opts->mOptionList;
            // eBPFConfig->mConfig = std::move(fconfig);
            // // UpdateContext must ahead of StartPlugin
            // mFileSecureCB->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
            // ret = mSourceManager->StartPlugin(type, std::move(eBPFConfig));
            break;
        }
        default:
            LOG_ERROR(sLogger, ("unknown plugin type", int(type)));
            return false;
    }

    if (ret) {
        mStartPluginTotal->Add(1);
    }

    return ret;
}

bool eBPFServer::HasRegisteredPlugins() const {
    std::lock_guard<std::mutex> lk(mMtx);
    for (auto& pipeline : mLoadedPipeline) {
        if (!pipeline.empty())
            return true;
    }
    return false;
}

bool eBPFServer::EnablePlugin(const std::string& pipeline_name,
                              uint32_t plugin_index,
                              PluginType type,
                              const CollectionPipelineContext* ctx,
                              const std::variant<SecurityOptions*, ObserverNetworkOption*> options,
                              PluginMetricManagerPtr mgr) {
    if (!IsSupportedEnv(type)) {
        return false;
    }
    return StartPluginInternal(pipeline_name, plugin_index, type, ctx, options, mgr);
}

bool eBPFServer::DisablePlugin(const std::string& pipeline_name, logtail::ebpf::PluginType type) {
    if (!IsSupportedEnv(type)) {
        return true;
    }
    std::string prev_pipeline = CheckLoadedPipelineName(type);
    if (prev_pipeline == pipeline_name) {
        UpdatePipelineName(type, "", "");
    } else {
        LOG_WARNING(sLogger, ("prev pipeline", prev_pipeline)("curr pipeline", pipeline_name));
        return true;
    }
    bool ret = mSourceManager->StopPlugin(type);
    // UpdateContext must after than StopPlugin
    if (ret) {
        UpdateCBContext(type, nullptr, -1, -1);
        mStopPluginTotal->Add(1);
    }
    return ret;
}

std::string eBPFServer::CheckLoadedPipelineName(logtail::ebpf::PluginType type) {
    std::lock_guard<std::mutex> lk(mMtx);
    return mLoadedPipeline[int(type)];
}

std::string eBPFServer::GetAllProjects() {
    std::lock_guard<std::mutex> lk(mMtx);
    std::string res;
    for (int i = 0; i < int(logtail::ebpf::PluginType::MAX); i++) {
        if (mPluginProject[i] != "") {
            res += mPluginProject[i];
            res += " ";
        }
    }
    return res;
}

void eBPFServer::UpdatePipelineName(logtail::ebpf::PluginType type, const std::string& name, const std::string& project) {
    std::lock_guard<std::mutex> lk(mMtx);
    mLoadedPipeline[int(type)] = name;
    mPluginProject[int(type)] = project;
    return;
}

bool eBPFServer::SuspendPlugin(const std::string& pipeline_name, logtail::ebpf::PluginType type) {
    if (!IsSupportedEnv(type)) {
        return false;
    }
    // mark plugin status is update
    bool ret = mSourceManager->SuspendPlugin(type);
    if (ret) {
        UpdateCBContext(type, nullptr, -1, -1);
        mSuspendPluginTotal->Add(1);
    }
    return ret;
}

void eBPFServer::UpdateCBContext(PluginType type,
                                 const logtail::CollectionPipelineContext* ctx,
                                 logtail::QueueKey key,
                                 int idx) {
    switch (type) {
        case logtail::ebpf::PluginType::PROCESS_SECURITY: {
            if (mProcessSecureCB)
                mProcessSecureCB->UpdateContext(ctx, key, idx);
            return;
        }
        case logtail::ebpf::PluginType::NETWORK_OBSERVE: {
            if (mMeterCB)
                mMeterCB->UpdateContext(ctx, key, idx);
            if (mSpanCB)
                mSpanCB->UpdateContext(ctx, key, idx);
            if (mEventCB)
                mEventCB->UpdateContext(ctx, key, idx);
            return;
        }
        case logtail::ebpf::PluginType::NETWORK_SECURITY: {
            if (mNetworkSecureCB)
                mNetworkSecureCB->UpdateContext(ctx, key, idx);
            return;
        }
        case logtail::ebpf::PluginType::FILE_SECURITY: {
            if (mFileSecureCB)
                mFileSecureCB->UpdateContext(ctx, key, idx);
            return;
        }
        default:
            return;
    }
}

} // namespace ebpf
} // namespace logtail
