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

#include "ebpf/EBPFServer.h"

#include <future>
#include <string>

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/LogtailCommonFlags.h"
#include "common/MachineInfoUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/include/export.h"
#include "logger/Logger.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"
// #include "plugin/file_security/FileSecurityManager.h"
#include "plugin/network_observer/NetworkObserverManager.h"
#include "plugin/network_security/NetworkSecurityManager.h"
#include "plugin/process_security/ProcessSecurityManager.h"

DEFINE_FLAG_INT64(kernel_min_version_for_ebpf,
                  "the minimum kernel version that supported eBPF normal running, 4.19.0.0 -> 4019000000",
                  4019000000);

namespace logtail::ebpf {

static const uint16_t kKernelVersion310 = 3010; // for centos7
static const std::string kKernelNameCentos = "CentOS";
static const uint16_t kKernelCentosMinVersion = 7006;

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
    if (mInited) {
        return;
    }
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
    int64_t version = 0;
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
    if (version / 1000000 != kKernelVersion310) {
        LOG_WARNING(sLogger, ("unsupported kernel version, will not start eBPF plugin ... version", version));
        m310Support = false;
        return;
    }

    std::string os;
    int64_t osVersion = 0;
    if (GetRedHatReleaseInfo(os, osVersion, STRING_FLAG(default_container_host_path))
        || GetRedHatReleaseInfo(os, osVersion)) {
        if (os == kKernelNameCentos && osVersion >= kKernelCentosMinVersion) {
            m310Support = true;
            return;
        }
        LOG_WARNING(
            sLogger,
            ("unsupported os for 310 kernel, will not start eBPF plugin ...", "")("os", os)("version", osVersion));
        m310Support = false;
        return;
    }
    LOG_WARNING(sLogger, ("not redhat release, will not start eBPF plugin ...", ""));
    m310Support = false;
}

bool EBPFServer::IsSupportedEnv(PluginType type) {
    return mEnvMgr.IsSupportedEnv(type);
}

void EBPFServer::Init() {
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

    // read host path prefix
    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
        mHostPathPrefix = STRING_FLAG(default_container_host_path);
        LOG_INFO(sLogger, ("running in container mode, would set host path prefix to ", mHostPathPrefix));
    } else {
        LOG_INFO(sLogger, ("running in host mode", "would not set host path prefix ..."));
        mHostPathPrefix = "/";
    }

    LOG_DEBUG(sLogger, ("begin to init timer", ""));
    Timer::GetInstance()->Init();
    AsynCurlRunner::GetInstance()->Init();
    LOG_DEBUG(sLogger, ("begin to start poller", ""));
    mPoller = async(std::launch::async, &EBPFServer::PollPerfBuffers, this);
    LOG_DEBUG(sLogger, ("begin to start handler", ""));
    mHandler = async(std::launch::async, &EBPFServer::HandlerEvents, this);
    // check env

    // mMonitorMgr = std::make_unique<eBPFSelfMonitorMgr>();
    DynamicMetricLabels dynamicLabels;
    dynamicLabels.emplace_back(METRIC_LABEL_KEY_PROJECT, [this]() -> std::string { return this->GetAllProjects(); });
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER}},
        std::move(dynamicLabels));

    auto pollProcessEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_EBPF_POLL_PROCESS_EVENTS_TOTAL);
    auto lossProcessEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_EBPF_LOSS_PROCESS_EVENTS_TOTAL);
    auto processCacheMissTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_EBPF_PROCESS_CACHE_MISS_TOTAL);
    auto processCacheSize = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_CACHE_SIZE);
    auto processDataMapSize = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_DATA_MAP_SIZE);
    auto retryableEventCacheSize = mMetricsRecordRef.CreateIntGauge(
        METRIC_RUNNER_EBPF_RETRYABLE_EVENT_CACHE_SIZE); // TODO: shoud be shared across network connection retry

    mEBPFAdapter->Init();

    mProcessCacheManager = std::make_shared<ProcessCacheManager>(mEBPFAdapter,
                                                                 mHostName,
                                                                 mHostPathPrefix,
                                                                 mCommonEventQueue,
                                                                 pollProcessEventsTotal,
                                                                 lossProcessEventsTotal,
                                                                 processCacheMissTotal,
                                                                 processCacheSize,
                                                                 processDataMapSize,
                                                                 retryableEventCacheSize);
    // ebpf config
    auto configJson = AppConfig::GetInstance()->GetConfig();
    mAdminConfig.LoadEbpfConfig(configJson);
}

void EBPFServer::Stop() {
    if (!mInited) {
        return;
    }
    mInited = false;

    mRunning = false;

    LOG_INFO(sLogger, ("begin to stop all plugins", ""));
    for (int i = 0; i < int(PluginType::MAX); i++) {
        auto pipelineName = mLoadedPipeline[i];
        if (pipelineName.size()) {
            bool ret = DisablePlugin(pipelineName, static_cast<PluginType>(i));
            LOG_INFO(sLogger,
                     ("force stop plugin",
                      magic_enum::enum_name(static_cast<PluginType>(i)))("pipeline", pipelineName)("ret", ret));
        }
    }

    std::future_status s1 = mPoller.wait_for(std::chrono::seconds(1));
    std::future_status s2 = mHandler.wait_for(std::chrono::seconds(1));
    if (mPoller.valid()) {
        if (s1 == std::future_status::ready) {
            LOG_DEBUG(sLogger, ("poller thread", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("poller thread", "forced to stopped"));
        }
    }

    if (mHandler.valid()) {
        if (s2 == std::future_status::ready) {
            LOG_DEBUG(sLogger, ("handler thread", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("handler thread", "forced to stopped"));
        }
    }
}

// maybe update or create
bool EBPFServer::startPluginInternal(const std::string& pipelineName,
                                     uint32_t pluginIndex,
                                     PluginType type,
                                     const logtail::CollectionPipelineContext* ctx,
                                     const std::variant<SecurityOptions*, ObserverNetworkOption*>& options,
                                     const PluginMetricManagerPtr& metricManager) {
    std::string prevPipelineName = CheckLoadedPipelineName(type);
    if (prevPipelineName == pipelineName) {
        auto pluginMgr = GetPluginManager(type);
        if (pluginMgr) {
            int res = pluginMgr->Update(options);
            if (res) {
                LOG_WARNING(sLogger, ("update plugin failed, type", magic_enum::enum_name(type))("res", res));
                return false;
            }
            res = pluginMgr->Resume(options);
            if (res) {
                LOG_WARNING(sLogger, ("resume plugin failed, type", magic_enum::enum_name(type))("res", res));
                return false;
            }
            pluginMgr->UpdateContext(ctx, ctx->GetProcessQueueKey(), pluginIndex);
            return true;
        }
        LOG_ERROR(sLogger, ("no plugin registered, should not happen", magic_enum::enum_name(type)));
        return false;
    }

    UpdatePipelineName(type, pipelineName, ctx->GetProjectName());

    if (type != PluginType::NETWORK_OBSERVE) {
        if (mProcessCacheManager->Init()) {
            LOG_INFO(sLogger, ("ProcessCacheManager initialization", "succeeded"));
        } else {
            LOG_ERROR(sLogger, ("ProcessCacheManager initialization", "failed"));
            return false;
        }
    }

    // step1: convert options to export type
    auto eBPFConfig = std::make_unique<PluginConfig>();
    eBPFConfig->mPluginType = type;
    // call update function
    // step2: call init function
    auto pluginMgr = GetPluginManager(type);
    switch (type) {
        case PluginType::PROCESS_SECURITY: {
            if (!pluginMgr) {
                pluginMgr = ProcessSecurityManager::Create(
                    mProcessCacheManager, mEBPFAdapter, mCommonEventQueue, metricManager);
                UpdatePluginManager(type, pluginMgr);
            }
            break;
        }

        case PluginType::NETWORK_OBSERVE: {
            if (!pluginMgr) {
                pluginMgr = NetworkObserverManager::Create(
                    mProcessCacheManager, mEBPFAdapter, mCommonEventQueue, metricManager);
                UpdatePluginManager(type, pluginMgr);
            }
            break;
        }

        case PluginType::NETWORK_SECURITY: {
            if (!pluginMgr) {
                pluginMgr = NetworkSecurityManager::Create(
                    mProcessCacheManager, mEBPFAdapter, mCommonEventQueue, metricManager);
                UpdatePluginManager(type, pluginMgr);
            }
            break;
        }

        // case PluginType::FILE_SECURITY: {
        //     if (!pluginMgr) {
        //         pluginMgr
        //             = FileSecurityManager::Create(mProcessCacheManager, mEBPFAdapter, mDataEventQueue,
        //             metricManager);
        //         UpdatePluginManager(type, pluginMgr);
        //     }
        //     break;
        // }
        default:
            LOG_ERROR(sLogger, ("unknown plugin type", int(type)));
            return false;
    }

    pluginMgr->UpdateContext(ctx, ctx->GetProcessQueueKey(), pluginIndex);
    return (pluginMgr->Init(options) == 0);
}

bool EBPFServer::HasRegisteredPlugins() const {
    std::lock_guard<std::mutex> lk(mMtx);
    for (const auto& pipeline : mLoadedPipeline) {
        if (!pipeline.empty()) {
            return true;
        }
    }
    return false;
}

bool EBPFServer::EnablePlugin(const std::string& pipelineName,
                              uint32_t pluginIndex,
                              PluginType type,
                              const CollectionPipelineContext* ctx,
                              const std::variant<SecurityOptions*, ObserverNetworkOption*>& options,
                              const PluginMetricManagerPtr& mgr) {
    if (!IsSupportedEnv(type)) {
        return false;
    }
    return startPluginInternal(pipelineName, pluginIndex, type, ctx, options, mgr);
}

bool EBPFServer::CheckIfNeedStopProcessCacheManager() const {
    std::lock_guard<std::mutex> lk(mMtx);
    auto nsMgr = mPlugins[static_cast<int>(PluginType::NETWORK_SECURITY)];
    auto psMgr = mPlugins[static_cast<int>(PluginType::PROCESS_SECURITY)];
    auto fsMgr = mPlugins[static_cast<int>(PluginType::FILE_SECURITY)];
    if ((nsMgr && nsMgr->IsExists()) || (psMgr && psMgr->IsExists()) || (fsMgr && fsMgr->IsExists())) {
        return false;
    }
    LOG_INFO(sLogger, ("no security plugin registerd", "begin to stop base manager ... "));
    return true;
}

bool EBPFServer::DisablePlugin(const std::string& pipelineName, PluginType type) {
    if (!IsSupportedEnv(type)) {
        return true;
    }
    std::string prevPipeline = CheckLoadedPipelineName(type);
    if (prevPipeline == pipelineName) {
        UpdatePipelineName(type, "", "");
    } else {
        LOG_WARNING(
            sLogger,
            ("the specified config is not running, prev pipeline", prevPipeline)("curr pipeline", pipelineName));
        return true;
    }

    LOG_INFO(sLogger, ("begin to stop plugin for ", magic_enum::enum_name(type))("pipeline", pipelineName));
    auto pluginManager = GetPluginManager(type);
    if (pluginManager && pluginManager->IsExists()) {
        pluginManager->UpdateContext(nullptr, -1, -1);
        int ret = pluginManager->Destroy();
        if (ret == 0) {
            UpdatePluginManager(type, nullptr);
            // pluginManager->UpdateProcessCacheManager(nullptr); // deprecated ... TODO @qianlu.kk
            LOG_DEBUG(sLogger, ("stop plugin for", magic_enum::enum_name(type))("pipeline", pipelineName));
            if (type == PluginType::NETWORK_SECURITY || type == PluginType::PROCESS_SECURITY
                || type == PluginType::FILE_SECURITY) {
                // check if we need stop ProcessCacheManager
                if (CheckIfNeedStopProcessCacheManager()) {
                    mProcessCacheManager->Stop();
                }
            }
        } else {
            LOG_ERROR(sLogger, ("failed to stop plugin for", magic_enum::enum_name(type))("pipeline", pipelineName));
        }
    } else {
        LOG_WARNING(sLogger,
                    ("no plugin registered or not running, plugin type", magic_enum::enum_name(type))("pipeline",
                                                                                                      pipelineName));
    }
    return true;
}

std::string EBPFServer::CheckLoadedPipelineName(PluginType type) {
    std::lock_guard<std::mutex> lk(mMtx);
    return mLoadedPipeline[int(type)];
}

std::string EBPFServer::GetAllProjects() {
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

void EBPFServer::UpdatePipelineName(PluginType type, const std::string& name, const std::string& project) {
    std::lock_guard<std::mutex> lk(mMtx);
    mLoadedPipeline[int(type)] = name;
    mPluginProject[int(type)] = project;
}

bool EBPFServer::SuspendPlugin(const std::string&, PluginType type) {
    if (!IsSupportedEnv(type)) {
        return false;
    }

    auto mgr = GetPluginManager(type);
    if (!mgr || !mgr->IsRunning()) {
        LOG_DEBUG(sLogger, ("plugin not registered or stopped", ""));
        return true;
    }

    mgr->UpdateContext(nullptr, -1, -1);

    int ret = mgr->Suspend();
    if (ret) {
        LOG_ERROR(sLogger, ("failed to suspend plugin", magic_enum::enum_name(type)));
        return false;
    }
    return true;
}

void EBPFServer::PollPerfBuffers() {
    mFrequencyMgr.SetPeriod(std::chrono::milliseconds(100));
    while (mRunning) {
        auto now = std::chrono::steady_clock::now();
        auto nextWindow = mFrequencyMgr.Next();
        if (!mFrequencyMgr.Expired(now)) {
            std::this_thread::sleep_until(nextWindow);
            mFrequencyMgr.Reset(nextWindow);
        } else {
            mFrequencyMgr.Reset(now);
        }
        for (int i = 0; i < int(PluginType::MAX); i++) {
            auto plugin = GetPluginManager(PluginType(i));
            if (!plugin || !plugin->IsRunning()) {
                continue;
            }
            int cnt = plugin->PollPerfBuffer();
            LOG_DEBUG(sLogger,
                      ("poll buffer for ", magic_enum::enum_name(PluginType(i)))("cnt", cnt)("running status",
                                                                                             plugin->IsRunning()));
        }
    }
}

std::shared_ptr<AbstractManager> EBPFServer::GetPluginManager(PluginType type) {
    std::lock_guard<std::mutex> lk(mMtx);
    if (type >= PluginType::MAX) {
        return nullptr;
    }
    return mPlugins[static_cast<int>(type)];
}

void EBPFServer::UpdatePluginManager(PluginType type, std::shared_ptr<AbstractManager> mgr) {
    std::lock_guard<std::mutex> lk(mMtx);
    if (type >= PluginType::MAX) {
        return;
    }
    mPlugins[static_cast<int>(type)] = mgr;
}

void EBPFServer::HandlerEvents() {
    std::array<std::shared_ptr<CommonEvent>, 4096> items;
    while (mRunning) {
        // consume queue
        size_t count
            = mCommonEventQueue.wait_dequeue_bulk_timed(items.data(), items.size(), std::chrono::milliseconds(200));
        // handle ....
        if (count == 0) {
            continue;
        }

        for (size_t i = 0; i < count; i++) {
            auto& event = items[i];
            if (!event) {
                LOG_ERROR(sLogger, ("Encountered null event in DataEventQueue at index", i));
                continue;
            }
            auto pluginType = event->GetPluginType();
            auto plugin = GetPluginManager(pluginType);
            if (plugin && plugin->IsRunning()) {
                // handle event and put into aggregator ...
                plugin->HandleEvent(event);
            }
        }

        // clear
        for (size_t i = 0; i < count; i++) {
            items[i].reset();
        }
    }
}

} // namespace logtail::ebpf
