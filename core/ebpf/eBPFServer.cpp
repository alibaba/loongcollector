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
#include <future>

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/Lock.h"
#include "common/LogtailCommonFlags.h"
#include "common/MachineInfoUtil.h"
#include "ebpf/Config.h"
#include "ebpf/include/export.h"
#include "logger/Logger.h"
#include "metadata/K8sMetadata.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"
#include "plugin/network_observer/NetworkObserverManager.h"
#include "plugin/process_security/ProcessSecurityManager.h"
#include "plugin/file_security/FileSecurityManager.h"
#include "plugin/network_security/NetworkSecurityManager.h"
#include "common/magic_enum.hpp"

DEFINE_FLAG_INT64(kernel_min_version_for_ebpf,
                  "the minimum kernel version that supported eBPF normal running, 4.19.0.0 -> 4019000000",
                  4019000000);

namespace logtail {
namespace ebpf {

static const uint16_t KERNEL_VERSION_310 = 3010; // for centos7
static const std::string KERNEL_NAME_CENTOS = "CentOS";
static const uint16_t KERNEL_CENTOS_MIN_VERSION = 7006;

bool EnvManager::IsSupportedEnv(PluginType type) {
    if (!mInited) {
        LOG_ERROR(sLogger, ("env manager not inited ...", ""));
        return false;
    }
    bool status = false;
    switch (type) {
        case PluginType::NETWORK_OBSERVE:
            status = mArchSupport && (mBTFSupport || m310Support);
            break;
        case PluginType::FILE_SECURITY:
        case PluginType::NETWORK_SECURITY:
        case PluginType::PROCESS_SECURITY: {
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

bool eBPFServer::IsSupportedEnv(PluginType type) {
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
    mRunning = true;

    mHostIp = GetHostIp();
    mHostName = GetHostName();

#ifdef APSARA_UNIT_TEST_MAIN
    mHostPathPrefix = "/logtail_host";
    LOG_DEBUG(sLogger, ("running in container mode, would set host path prefix to ", mHostPathPrefix));
#else
    // read host path prefix
    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
        mHostPathPrefix = STRING_FLAG(default_container_host_path);
        LOG_INFO(sLogger, ("running in container mode, would set host path prefix to ", mHostPathPrefix));
    } else {
        LOG_INFO(sLogger, ("running in host mode", "would not set host path prefix ..."));
    }
#endif

    mScheduler = std::make_unique<Timer>();
    mScheduler->Init();
    mPoller = async(std::launch::async, &eBPFServer::PollPerfBuffers, this);
    mHandler = async(std::launch::async, &eBPFServer::HandlerEvents, this);
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
}

void eBPFServer::Stop() {
    if (!mInited)
        return;
    mInited = false;

    LOG_INFO(sLogger, ("begin to stop all plugins", ""));
    for (int i = 0; i < int(PluginType::MAX); i++) {
        auto pipelineName = mLoadedPipeline[i];
        if (pipelineName.size()) {
            bool ret = DisablePlugin(pipelineName, static_cast<PluginType>(i));
            LOG_INFO(sLogger, ("force stop plugin", magic_enum::enum_name(static_cast<PluginType>(i))) ("pipeline", pipelineName) ("ret", ret));
        }
    }
    
    LOG_INFO(sLogger, ("begin to stop timer", ""));
    mScheduler->Stop();

    mRunning = false;
    std::future_status s1 = mPoller.wait_for(std::chrono::seconds(1));
    std::future_status s2 = mHandler.wait_for(std::chrono::seconds(1));
    if (mPoller.valid()) {
        if (s1 == std::future_status::ready) {
            LOG_INFO(sLogger, ("poller thread", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("poller thread", "forced to stopped"));
        }
    }

    if (mHandler.valid()) {
        if (s2 == std::future_status::ready) {
            LOG_INFO(sLogger, ("handler thread", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("handler thread", "forced to stopped"));
        }
    }
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

    if (prev_pipeline_name.size() && prev_pipeline_name == pipeline_name) {
        // TODO @qianlu.kk for update scenario ...

        // mgr->Update();
        // for file/network security scenario, just update FILTER CONFIG
        // for process scenario, do nothing, just resume
        // for network observer scenario, we just need to update some user config, like enable metric or not ... 

        // mgr->Resume();
        return true;
    }

    UpdatePipelineName(type, pipeline_name, ctx->GetProjectName());

    if (type != PluginType::NETWORK_OBSERVE) {
        LOG_INFO(sLogger, ("hostname", mHostName) ("mHostPathPrefix", mHostPathPrefix));
        mBaseManager = std::make_shared<BaseManager>(mSourceManager, mHostName, mHostPathPrefix, mDataEventQueue);
        auto res = mBaseManager->Init();
        LOG_INFO(sLogger, ("basemanager init ", res));
    }

    // init self monitor
    // mMonitorMgr->Init(type, mgr, pipeline_name, ctx->GetProjectName());

    // step1: convert options to export type
    bool ret = false;
    auto eBPFConfig = std::make_unique<PluginConfig>();
    eBPFConfig->mPluginType = type;
    // call update function
    // step2: call init function
    auto pluginMgr = GetPluginManager(type);
    switch (type) {
        case PluginType::PROCESS_SECURITY: {
            if (!pluginMgr) {
                pluginMgr = ProcessSecurityManager::Create(
                    mBaseManager, mSourceManager, mDataEventQueue, mScheduler);
                UpdatePluginManager(type, pluginMgr);
            } else {
                pluginMgr->UpdateBaseManager(mBaseManager);
            }
            pluginMgr->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
            ret = (pluginMgr->Init(options) == 0);
            break;
        }

        case PluginType::NETWORK_OBSERVE: {
            if (!pluginMgr) {
                pluginMgr = NetworkObserverManager::Create(
                    mBaseManager, mSourceManager, mDataEventQueue, mScheduler);
                UpdatePluginManager(type, pluginMgr);
            } else {
                pluginMgr->UpdateBaseManager(mBaseManager);
            }
            NetworkObserveConfig nconfig;
            // TODO @qianlu.kk register k8s metadata callback for metric ??

            pluginMgr->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);

            ret = (pluginMgr->Init(options) == 0);
            break;
        }

        case PluginType::NETWORK_SECURITY: {
            if (!pluginMgr) {
                pluginMgr = NetworkSecurityManager::Create(
                mBaseManager, mSourceManager, mDataEventQueue, mScheduler);
                UpdatePluginManager(type, pluginMgr);
            } else {
                pluginMgr->UpdateBaseManager(mBaseManager);
            }

            pluginMgr->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
            ret = (pluginMgr->Init(options) == 0);
            break;
        }

        case PluginType::FILE_SECURITY: {
            if (!pluginMgr) {
                pluginMgr = FileSecurityManager::Create(
                mBaseManager, mSourceManager, mDataEventQueue, mScheduler);
                UpdatePluginManager(type, pluginMgr);
            } else {
                pluginMgr->UpdateBaseManager(mBaseManager);
            }
            pluginMgr->UpdateContext(ctx, ctx->GetProcessQueueKey(), plugin_index);
            ret = (pluginMgr->Init(options) == 0);
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

bool eBPFServer::DisablePlugin(const std::string& pipeline_name, PluginType type) {
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

    LOG_INFO(sLogger, ("begin to stop plugin for ", magic_enum::enum_name(type)) ("pipeline", pipeline_name));
    auto pluginManager = GetPluginManager(type);
    if (pluginManager && pluginManager->IsRunning()) {
        pluginManager->UpdateContext(nullptr, -1, -1);
        int ret = pluginManager->Destroy();
        if (ret == 0) {
            pluginManager->UpdateBaseManager(nullptr);
            LOG_DEBUG(sLogger, ("stop plugin for", magic_enum::enum_name(type)) ("pipeline", pipeline_name));
            if (type == PluginType::NETWORK_SECURITY || type == PluginType::PROCESS_SECURITY || type == PluginType::FILE_SECURITY) {
                // check if we need stop basemanager
                auto nsMgr = GetPluginManager(PluginType::NETWORK_SECURITY);
                auto psMgr = GetPluginManager(PluginType::PROCESS_SECURITY);
                auto fsMgr = GetPluginManager(PluginType::FILE_SECURITY);
                if ((!nsMgr || !nsMgr->IsRunning()) && (!psMgr || !psMgr->IsRunning()) && (!fsMgr || !fsMgr->IsRunning())) {
                    LOG_WARNING(sLogger, ("no security plugin registerd", "begin to stop base manager ... "));
                    mBaseManager->Stop();
                }
            }
        } else {
            LOG_ERROR(sLogger, ("failed to stop plugin for", magic_enum::enum_name(type)) ("pipeline", pipeline_name));
        }
    } else {
        LOG_WARNING(sLogger, ("no plugin registered or not running, plugin type", magic_enum::enum_name(type))("pipeline", pipeline_name));
    }
    return true;
}

std::string eBPFServer::CheckLoadedPipelineName(PluginType type) {
    std::lock_guard<std::mutex> lk(mMtx);
    return mLoadedPipeline[int(type)];
}

std::string eBPFServer::GetAllProjects() {
    std::lock_guard<std::mutex> lk(mMtx);
    std::string res;
    for (int i = 0; i < int(PluginType::MAX); i++) {
        if (mPluginProject[i] != "") {
            res += mPluginProject[i];
            res += " ";
        }
    }
    return res;
}

void eBPFServer::UpdatePipelineName(PluginType type,
                                    const std::string& name,
                                    const std::string& project) {
    std::lock_guard<std::mutex> lk(mMtx);
    mLoadedPipeline[int(type)] = name;
    mPluginProject[int(type)] = project;
    return;
}

bool eBPFServer::SuspendPlugin(const std::string& pipeline_name, PluginType type) {
    if (!IsSupportedEnv(type)) {
        return false;
    }

    auto mgr = GetPluginManager(type);
    if (!mgr || !mgr->IsRunning()) {
        LOG_DEBUG(sLogger, ("plugin not registered or stopped", ""));
        return true;
    }

    // TODO @qianlu.kk
    // this will stop handle or push eventGroup to queue,
    // we need to resume plugin when we restart plugin, and we don't implement yet...
    // pay attention to timer process
    // and we need to figure out wether we need to STOP plugin... especially for perf workers.
    mgr->UpdateContext(nullptr, -1, -1);
    mgr->Suspend();
    mSuspendPluginTotal->Add(1);
    return true;
}

void eBPFServer::PollPerfBuffers() {
    mFrequencyMgr.SetPeriod(std::chrono::milliseconds(100));
    while(mRunning) {
        auto now = std::chrono::steady_clock::now();
        auto next_window = mFrequencyMgr.Next();
        if (!mFrequencyMgr.Expired(now)) {
          std::this_thread::sleep_until(next_window);
          mFrequencyMgr.Reset(next_window);
        } else {
          mFrequencyMgr.Reset(now);
        }
        for (int i = 0; i < int(PluginType::MAX); i ++) {
            auto plugin = GetPluginManager(PluginType(i));
            if (!plugin || !plugin->IsRunning()) continue;
            int cnt = plugin->PollPerfBuffer();
            LOG_DEBUG(sLogger, ("poll buffer for ", magic_enum::enum_name(PluginType(i)))("cnt", cnt)("running status", plugin->IsRunning()));
        }
    }
}

std::shared_ptr<AbstractManager> eBPFServer::GetPluginManager(PluginType type) {
    std::lock_guard<std::mutex> lk(mMtx);
    if (type == PluginType::MAX) {
        return nullptr;
    } else {
        return mPlugins[static_cast<int>(type)];
    }
}

void eBPFServer::UpdatePluginManager(PluginType type, std::shared_ptr<AbstractManager> mgr) {
    std::lock_guard<std::mutex> lk(mMtx);
    if (type == PluginType::MAX) {
        return;
    } else {
        mPlugins[static_cast<int>(type)] = mgr;
    }
}

void eBPFServer::HandlerEvents() {
    std::vector<std::shared_ptr<CommonEvent>> items(1024);
    while(mRunning) {
        // consume queue
        size_t count = mDataEventQueue.wait_dequeue_bulk_timed(items.data(), 4096, std::chrono::milliseconds(200));
        LOG_DEBUG(sLogger, ("get data events, number", count));
        // handle ....
        if (count == 0) {
            continue;
        }

        for (size_t i = 0; i < count; i ++) {
            auto event = items[i];
            auto pluginType = event->GetPluginType();
            auto plugin = GetPluginManager(pluginType);
            if (plugin) {
                // handle event and put into aggregator ...
                plugin->HandleEvent(event);
            }
        }

        // handle
        items.clear();
        items.resize(1024);
    }
}

} // namespace ebpf
} // namespace logtail
