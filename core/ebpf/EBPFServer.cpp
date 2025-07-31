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
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/LogtailCommonFlags.h"
#include "common/MachineInfoUtil.h"
#include "common/TimeKeeper.h"
#include "common/http/AsynCurlRunner.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/include/export.h"
#include "ebpf/plugin/AbstractManager.h"
#include "logger/Logger.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"
#include "plugin/file_security/FileSecurityManager.h"
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

EBPFServer::EBPFServer()
    : mEBPFAdapter(std::make_shared<EBPFAdapter>()),
      mHostIp(GetHostIp()),
      mHostName(GetHostName()),
      mCommonEventQueue(8192) {
    mEnvMgr.InitEnvInfo();

    // read host path prefix
    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
        mHostPathPrefix = STRING_FLAG(default_container_host_path);
        LOG_INFO(sLogger, ("running in container mode, would set host path prefix to ", mHostPathPrefix));
    } else {
        LOG_INFO(sLogger, ("running in host mode", "would not set host path prefix ..."));
        mHostPathPrefix = "/";
    }

    DynamicMetricLabels dynamicLabels;
    dynamicLabels.emplace_back(METRIC_LABEL_KEY_PROJECT, [this]() -> std::string { return this->GetAllProjects(); });
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER}},
        std::move(dynamicLabels));

    auto pollProcessEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_EBPF_POLL_PROCESS_EVENTS_TOTAL);
    auto lossProcessEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_EBPF_LOSS_PROCESS_EVENTS_TOTAL);
    auto processCacheMissTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_EBPF_PROCESS_CACHE_MISS_TOTAL);
    auto processCacheSize = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_CACHE_SIZE);
    auto processDataMapSize = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_DATA_MAP_SIZE);
    mRetryableEventCacheSize = mMetricsRecordRef.CreateIntGauge(
        METRIC_RUNNER_EBPF_RETRYABLE_EVENT_CACHE_SIZE); // TODO: shoud be shared across network connection retry
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);

    mRecvKernelEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_EBPF_POLL_KERNEL_EVENTS_TOTAL);
    mLossKernelEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_EBPF_LOST_KERNEL_EVENTS_TOTAL);
    mConnectionCacheSize = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_EBPF_CONNECTION_CACHE_SIZE);

    mProcessCacheManager = std::make_shared<ProcessCacheManager>(mEBPFAdapter,
                                                                 mHostName,
                                                                 mHostPathPrefix,
                                                                 mCommonEventQueue,
                                                                 pollProcessEventsTotal,
                                                                 lossProcessEventsTotal,
                                                                 processCacheMissTotal,
                                                                 processCacheSize,
                                                                 processDataMapSize,
                                                                 mRetryableEventCache);
    // ebpf config
    auto configJson = AppConfig::GetInstance()->GetConfig();
    mAdminConfig.LoadEbpfConfig(configJson);
    initUnifiedEpollMonitoring();
}

EBPFServer::~EBPFServer() {
    Stop();
    cleanupUnifiedEpollMonitoring();
}

void EBPFServer::Init() {
    if (mInited) {
        return;
    }
    if (!mEnvMgr.AbleToLoadDyLib()) {
        return;
    }
    mInited = true;
    mRunning = true;

    AsynCurlRunner::GetInstance()->Init();
    LOG_DEBUG(sLogger, ("begin to start poller", ""));
    mPoller = async(std::launch::async, &EBPFServer::pollPerfBuffers, this);
    LOG_DEBUG(sLogger, ("begin to start handler", ""));
    mHandler = async(std::launch::async, &EBPFServer::handlerEvents, this);

    mEBPFAdapter->Init(); // Idempotent
}

void EBPFServer::Stop() {
    if (!mInited) {
        return;
    }

    mRunning = false;
    LOG_INFO(sLogger, ("begin to stop all plugins", ""));
    for (int i = 0; i < int(PluginType::MAX); i++) {
        auto pipelines = mPlugins[i].mPipelines;
        for (const auto& entry : pipelines) {
            bool ret = DisablePlugin(entry.first, static_cast<PluginType>(i));
            LOG_INFO(sLogger,
                     ("force stop plugin", magic_enum::enum_name(static_cast<PluginType>(i)))("pipeline",
                                                                                              entry.first)("ret", ret));
        }
    }
    mRetryableEventCache.Clear();

    bool alarmOnce = false;
    while (mPoller.valid()) {
        std::future_status s1 = mPoller.wait_for(std::chrono::seconds(10));
        if (s1 == std::future_status::ready) {
            LOG_DEBUG(sLogger, ("poller thread", "stopped successfully"));
            break;
        }
        if (!alarmOnce) {
            LOG_ERROR(sLogger, ("poller thread stop", "too slow"));
            AlarmManager::GetInstance()->SendAlarmError(CONFIG_UPDATE_ALARM, std::string("EBPFServer stop too slow"));
            alarmOnce = true;
        }
    }

    alarmOnce = false;
    while (mHandler.valid()) {
        std::future_status s2 = mHandler.wait_for(std::chrono::seconds(10));
        if (s2 == std::future_status::ready) {
            LOG_DEBUG(sLogger, ("handler thread", "stopped successfully"));
            break;
        }
        if (!alarmOnce) {
            LOG_ERROR(sLogger, ("handler thread ", " too slow"));
            AlarmManager::GetInstance()->SendAlarmError(CONFIG_UPDATE_ALARM,
                                                        std::string("ProcessCacheManager stop too slow"));
            alarmOnce = true;
        }
    }
    mInited = false;
}

// maybe update or create
bool EBPFServer::startPluginInternal(const std::string& pipelineName,
                                     uint32_t pluginIndex,
                                     PluginType type,
                                     const logtail::CollectionPipelineContext* ctx,
                                     const std::variant<SecurityOptions*, ObserverNetworkOption*>& options,
                                     const PluginMetricManagerPtr& metricManager) {
    bool isNeedProcessCache = false;
    if (type != PluginType::NETWORK_OBSERVE) {
        isNeedProcessCache = true;
    }
    auto& pluginMgr = getPluginState(type).mManager;
    if (!pluginMgr) {
        // create ...
        if (isNeedProcessCache) {
            if (mProcessCacheManager->Init()) {
                registerPluginPerfBuffers(PluginType::PROCESS_SECURITY);
                LOG_INFO(sLogger, ("ProcessCacheManager initialization", "succeeded"));
            } else {
                LOG_ERROR(sLogger, ("ProcessCacheManager initialization", "failed"));
                return false;
            }
        }

        // step1: convert options to export type
        auto eBPFConfig = std::make_unique<PluginConfig>();
        eBPFConfig->mPluginType = type;
        // step2: create plugin manager
        auto& pluginMgr = getPluginState(type).mManager;
        switch (type) {
            case PluginType::PROCESS_SECURITY: {
                if (!pluginMgr) {
                    pluginMgr = ProcessSecurityManager::Create(mProcessCacheManager, mEBPFAdapter, mCommonEventQueue);
                }
                break;
            }

            case PluginType::NETWORK_OBSERVE: {
                if (!pluginMgr) {
                    auto mgr = NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mCommonEventQueue);
                    mgr->SetMetrics(mRecvKernelEventsTotal, mLossKernelEventsTotal, mConnectionCacheSize);
                    pluginMgr = mgr;
                }
                break;
            }

            case PluginType::NETWORK_SECURITY: {
                if (!pluginMgr) {
                    auto mgr = NetworkSecurityManager::Create(mProcessCacheManager, mEBPFAdapter, mCommonEventQueue);
                    mgr->SetMetrics(mRecvKernelEventsTotal, mLossKernelEventsTotal);
                    pluginMgr = mgr;
                }
                break;
            }

            case PluginType::FILE_SECURITY: {
                if (!pluginMgr) {
                    auto mgr = FileSecurityManager::Create(
                        mProcessCacheManager, mEBPFAdapter, mCommonEventQueue, mRetryableEventCache);
                    mgr->SetMetrics(mRecvKernelEventsTotal, mLossKernelEventsTotal);
                    pluginMgr = mgr;
                }
                break;
            }
            default:
                LOG_ERROR(sLogger, ("Unknown plugin type", int(type)));
                return false;
        }

        if (pluginMgr->Init() != 0) {
            pluginMgr.reset();
            if (isNeedProcessCache && checkIfNeedStopProcessCacheManager()) {
                stopProcessCacheManager();
            }
            LOG_WARNING(sLogger, ("Failed to init plugin, type", magic_enum::enum_name(type)));
            return false;
        }
    }

    if (pluginMgr->AddOrUpdateConfig(ctx, pluginIndex, metricManager, options) != 0) {
        pluginMgr.reset();
        LOG_WARNING(sLogger, ("Failed to AddOrUpdateConfig, type", magic_enum::enum_name(type)));
        if (isNeedProcessCache && checkIfNeedStopProcessCacheManager()) {
            stopProcessCacheManager();
        }
        return false;
    }

    updatePluginState(type, pipelineName, ctx->GetProjectName(), PluginStateOperation::kAddPipeline, pluginMgr);
    if (type != PluginType::PROCESS_SECURITY && type != PluginType::NETWORK_OBSERVE) {
        registerPluginPerfBuffers(type);
    }

    return true;
}

bool EBPFServer::HasRegisteredPlugins() const {
    for (const auto& p : mPlugins) {
        if (!p.mPipelines.empty()) {
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

    std::unique_lock<std::shared_mutex> lock(getPluginState(type).mMtx);
    return startPluginInternal(pipelineName, pluginIndex, type, ctx, options, mgr);
}

bool EBPFServer::checkIfNeedStopProcessCacheManager() const {
    auto nsMgr = mPlugins[static_cast<int>(PluginType::NETWORK_SECURITY)].mManager;
    auto psMgr = mPlugins[static_cast<int>(PluginType::PROCESS_SECURITY)].mManager;
    auto fsMgr = mPlugins[static_cast<int>(PluginType::FILE_SECURITY)].mManager;
    if ((nsMgr && nsMgr->IsExists()) || (psMgr && psMgr->IsExists()) || (fsMgr && fsMgr->IsExists())) {
        return false;
    }
    return true;
}

void EBPFServer::stopProcessCacheManager() {
    LOG_INFO(sLogger, ("No security plugin registered", "begin to stop ProcessCacheManager ... "));
    mProcessCacheManager->Stop();
    unregisterPluginPerfBuffers(PluginType::PROCESS_SECURITY);
}

bool EBPFServer::DisablePlugin(const std::string& pipelineName, PluginType type) {
    if (!IsSupportedEnv(type)) {
        return true;
    }
    auto& pluginState = getPluginState(type);
    std::unique_lock<std::shared_mutex> lock(pluginState.mMtx);
    LOG_INFO(sLogger, ("begin to stop plugin for ", magic_enum::enum_name(type))("pipeline", pipelineName));
    auto& pluginManager = pluginState.mManager;
    if (pluginManager) {
        // pluginManager->UpdateContext(nullptr, -1, -1);
        int ret = pluginManager->RemoveConfig(pipelineName);
        if (ret) {
            LOG_ERROR(sLogger, ("failed to remove config for", magic_enum::enum_name(type))("pipeline", pipelineName));
            return false;
        }
        updatePluginState(type, pipelineName, "", PluginStateOperation::kRemovePipeline, pluginManager);
        if (pluginManager->RegisteredConfigCount() > 0) {
            return true;
        }

        // do real destroy ...
        unregisterPluginPerfBuffers(type);
        ret = pluginManager->Destroy();
        if (ret != 0) {
            LOG_ERROR(sLogger, ("failed to stop plugin for", magic_enum::enum_name(type))("pipeline", pipelineName));
        }
        updatePluginState(type, "", "", PluginStateOperation::kRemoveAll, nullptr);
        LOG_DEBUG(sLogger, ("stop plugin for", magic_enum::enum_name(type))("pipeline", pipelineName));
        if (type == PluginType::NETWORK_SECURITY || type == PluginType::PROCESS_SECURITY
            || type == PluginType::FILE_SECURITY) {
            // check if we need stop ProcessCacheManager
            if (checkIfNeedStopProcessCacheManager()) {
                stopProcessCacheManager();
            }
        }
    } else {
        LOG_WARNING(sLogger,
                    ("No plugin registered or plugin not running, plugin type",
                     magic_enum::enum_name(type))("pipeline", pipelineName));
    }
    return true;
}

std::string EBPFServer::GetAllProjects() {
    std::string res;
    for (int i = 0; i < int(PluginType::MAX); i++) {
        auto type = PluginType(i);
        auto& pluginState = getPluginState(type);
        if (!pluginState.mValid.load(std::memory_order_acquire)) {
            continue;
        }
        std::shared_lock<std::shared_mutex> lock(pluginState.mMtx);
        for (const auto& entry : mPlugins[i].mPipelines) {
            res += entry.first;
            res += " ";
        }
    }
    return res;
}

bool EBPFServer::SuspendPlugin(const std::string&, PluginType type) {
    if (!IsSupportedEnv(type)) {
        return false;
    }
    auto& pluginState = getPluginState(type);
    std::unique_lock<std::shared_mutex> lock(pluginState.mMtx);
    auto& mgr = pluginState.mManager;
    if (!mgr) {
        LOG_DEBUG(sLogger, ("plugin not registered or stopped", ""));
        return true;
    }

    int ret = mgr->Suspend();
    if (ret) {
        LOG_ERROR(sLogger, ("failed to suspend plugin", magic_enum::enum_name(type)));
        return false;
    }
    return true;
}

void EBPFServer::handleEventCache() {
    auto now = TimeKeeper::GetInstance()->NowSec();
    if (now > mLastEventCacheRetryTime + INT32_FLAG(ebpf_event_retry_interval_sec)) {
        EventCache().HandleEvents();
        mLastEventCacheRetryTime = now;
        SET_GAUGE(mRetryableEventCacheSize, EventCache().Size());
        LOG_DEBUG(sLogger, ("retry cache size", EventCache().Size()));
    }
}

void EBPFServer::handleEpollEvents() {
    if (mUnifiedEpollFd < 0) {
        return;
    }

    int numEvents = epoll_wait(mUnifiedEpollFd, mEpollEvents.data(), mEpollEvents.size(), kDefaultMaxWaitTimeMS);
    if (numEvents <= 0) {
        if (numEvents < 0 && errno != EINTR) {
            LOG_ERROR(sLogger, ("Unified epoll wait error", strerror(errno)));
        }
        return;
    }

    LOG_DEBUG(sLogger, ("Unified epoll detected events", numEvents));

    for (int i = 0; i < numEvents; ++i) {
        auto type = static_cast<PluginType>(mEpollEvents[i].data.u32);

        if (type == PluginType::PROCESS_SECURITY) {
            mProcessCacheManager->ConsumePerfBufferData();
            continue;
        }

        auto& pluginState = getPluginState(type);
        if (!pluginState.mValid.load(std::memory_order_acquire)) {
            continue;
        }
        std::shared_lock<std::shared_mutex> lock(pluginState.mMtx);
        if (pluginState.mManager) {
            const int cnt = pluginState.mManager->ConsumePerfBufferData();
            LOG_DEBUG(sLogger, ("Event-driven consume for", magic_enum::enum_name(type))("cnt", cnt));
        }
    }
}

void EBPFServer::pollPerfBuffers() {
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

        handleEventCache();
        handleEpollEvents();
        mProcessCacheManager->ClearProcessExpiredCache();

        // TODO (@qianlu.kk) adapt to ConsumePerfBufferData
        auto& pluginState = getPluginState(PluginType::NETWORK_OBSERVE);
        if (!pluginState.mValid.load(std::memory_order_acquire)) {
            continue;
        }
        std::shared_lock<std::shared_mutex> lock(pluginState.mMtx);
        if (pluginState.mManager) {
            auto* mgr = static_cast<NetworkObserverManager*>(pluginState.mManager.get());
            mgr->PollPerfBuffer(0); // 0 means non-blocking r(ef: https://libbpf.readthedocs.io/en/latest/api.html)
        }
    }
}

std::shared_ptr<AbstractManager> EBPFServer::GetPluginManager(PluginType type) {
    auto& pluginState = getPluginState(type);
    std::shared_lock<std::shared_mutex> lock(pluginState.mMtx);
    return pluginState.mManager;
}

PluginState& EBPFServer::getPluginState(PluginType type) {
    if (type >= PluginType::MAX) {
        throw std::out_of_range("Plugin type out of range");
    }
    return mPlugins[static_cast<int>(type)];
}

void EBPFServer::updatePluginState(PluginType type,
                                   const std::string& name,
                                   const std::string& project,
                                   PluginStateOperation op,
                                   std::shared_ptr<AbstractManager> mgr) {
    if (type >= PluginType::MAX) {
        return;
    }
    switch (op) {
        case PluginStateOperation::kAddPipeline: {
            mPlugins[static_cast<int>(type)].mPipelines.insert({name, project});
            break;
        }
        case PluginStateOperation::kRemovePipeline: {
            mPlugins[static_cast<int>(type)].mPipelines.erase(name);
            break;
        }
        case PluginStateOperation::kRemoveAll: {
            mPlugins[static_cast<int>(type)].mPipelines.clear();
            break;
        }
        default:
            break;
    }
    mPlugins[static_cast<int>(type)].mValid.store(mgr != nullptr, std::memory_order_release);
    mPlugins[static_cast<int>(type)].mManager = std::move(mgr);
}

void EBPFServer::handlerEvents() {
    std::array<std::shared_ptr<CommonEvent>, 4096> items;
    while (mRunning) {
        // consume queue
        size_t count
            = mCommonEventQueue.wait_dequeue_bulk_timed(items.data(), items.size(), std::chrono::milliseconds(200));
        // handle ....
        handleEvents(items, count);
        sendEvents();
    }
}

void EBPFServer::handleEvents(std::array<std::shared_ptr<CommonEvent>, 4096>& items, size_t count) {
    std::array<std::array<std::shared_ptr<CommonEvent>*, 4096>, int(PluginType::MAX)> groupedItems{};
    std::array<int, int(PluginType::MAX)> groupCounts{};
    for (size_t i = 0; i < count; i++) {
        auto& event = items[i];
        if (!event) {
            LOG_ERROR(sLogger, ("Encountered null event in DataEventQueue at index", i));
            continue;
        }
        auto pluginType = event->GetPluginType();
        groupedItems[int(pluginType)][groupCounts[int(pluginType)]++] = &event;
    }

    for (int i = 0; i < int(PluginType::MAX); ++i) {
        if (groupCounts[i] == 0) {
            continue;
        }
        auto pluginType = static_cast<PluginType>(i);
        auto& pluginState = getPluginState(pluginType);
        if (!pluginState.mValid.load(std::memory_order_acquire)) {
            continue;
        }
        std::shared_lock<std::shared_mutex> lock(pluginState.mMtx);
        auto plugin = pluginState.mManager;
        for (int j = 0; j < groupCounts[i]; ++j) {
            // handle event and put into aggregator ...
            if (plugin) {
                plugin->HandleEvent(*groupedItems[i][j]);
            }
        }
    }

    // clear
    for (size_t i = 0; i < count; i++) {
        items[i].reset();
    }
}

void EBPFServer::sendEvents() {
    for (int i = 0; i < int(PluginType::MAX); i++) {
        auto type = PluginType(i);
        auto& pluginState = getPluginState(type);
        if (!pluginState.mValid.load(std::memory_order_acquire)) {
            continue;
        }
        std::shared_lock<std::shared_mutex> lock(pluginState.mMtx);
        auto& plugin = pluginState.mManager;
        if (plugin) {
            // aggregate and send
            plugin->SendEvents();
        }
    }
}

void EBPFServer::initUnifiedEpollMonitoring() {
    mUnifiedEpollFd = epoll_create1(EPOLL_CLOEXEC);
    if (mUnifiedEpollFd < 0) {
        LOG_ERROR(sLogger, ("Failed to create unified epoll fd", strerror(errno)));
        return;
    }

    mEpollEvents.resize(1024);

    LOG_INFO(sLogger, ("Unified epoll monitoring initialized", mUnifiedEpollFd));
}

void EBPFServer::registerPluginPerfBuffers(PluginType type) {
    if (mUnifiedEpollFd < 0) {
        return;
    }

    auto epollFds = mEBPFAdapter->GetPerfBufferEpollFds(type);

    for (int epollFd : epollFds) {
        if (epollFd >= 0) {
            struct epoll_event event {};
            event.events = EPOLLIN;
            event.data.u32 = static_cast<uint32_t>(type);

            if (epoll_ctl(mUnifiedEpollFd, EPOLL_CTL_ADD, epollFd, &event) == 0) {
                LOG_DEBUG(sLogger,
                          ("Registered perf buffer epoll fd", epollFd)("plugin type", magic_enum::enum_name(type)));
            } else {
                LOG_ERROR(sLogger,
                          ("Failed to register perf buffer epoll fd",
                           epollFd)("error", strerror(errno))("plugin type", magic_enum::enum_name(type)));
            }
        }
    }
}

void EBPFServer::unregisterPluginPerfBuffers(PluginType type) {
    if (mUnifiedEpollFd < 0) {
        return;
    }

    auto epollFds = mEBPFAdapter->GetPerfBufferEpollFds(type);

    for (int epollFd : epollFds) {
        if (epollFd >= 0) {
            epoll_ctl(mUnifiedEpollFd, EPOLL_CTL_DEL, epollFd, nullptr);
            LOG_DEBUG(sLogger,
                      ("Unregistered perf buffer epoll fd", epollFd)("plugin type", magic_enum::enum_name(type)));
        }
    }
}

void EBPFServer::cleanupUnifiedEpollMonitoring() {
    if (mUnifiedEpollFd >= 0) {
        close(mUnifiedEpollFd);
        mUnifiedEpollFd = -1;
    }

    mEpollEvents.clear();
    LOG_INFO(sLogger, ("Unified epoll monitoring cleaned up", ""));
}
} // namespace logtail::ebpf
