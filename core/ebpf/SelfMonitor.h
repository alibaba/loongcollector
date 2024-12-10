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

#pragma once

#include <vector>
#include <array>
#include <atomic>

#include "ebpf/include/export.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"
#include "common/Lock.h"
#include "monitor/metric_models/MetricTypes.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {
namespace ebpf {

class BaseBPFMonitor {
public:
    virtual void HandleStatistic(nami::eBPFStatistics& stats);
    virtual void InitMetric();
    virtual void ReleaseMetric();
    virtual ~BaseBPFMonitor() = default;
protected:
    BaseBPFMonitor(const std::string& name, PluginMetricManagerPtr mgr) 
        : mPipelineName(name), mPluginMetricMgr(mgr) {}

    // attention: not thread safe!!
    void InitMetricInner();

    // attention: not thread safe!!
    void UpdateMetricInner(nami::eBPFStatistics& currStat);

    std::string mPipelineName;
    PluginMetricManagerPtr mPluginMetricMgr;
    // MetricsRecordRef& mRef;
    std::vector<std::pair<ReentrantMetricsRecordRef, MetricLabels>> mRefAndLabels;

    std::atomic_bool mMetricInited = false;

    CounterPtr mRecvKernelEventsTotal;
    CounterPtr mLossKernelEventsTotal;
    CounterPtr mPushEventsTotal;
    CounterPtr mPushSpansTotal;
    CounterPtr mPushMetricsTotal;
    IntGaugePtr mProcessCacheEntitiesNum;
    CounterPtr mProcessCacheMissTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
};

class NetworkObserverSelfMonitor : public BaseBPFMonitor {
public:
    NetworkObserverSelfMonitor(const std::string& name, PluginMetricManagerPtr mgr/**/) 
        : BaseBPFMonitor(name, mgr) {}

    void InitMetric() override;

    void HandleStatistic(nami::eBPFStatistics& stats) override;

private:

    // recv kernel events metric
    CounterPtr mRecvConnStatsTotal;
    CounterPtr mRecvCtrlEventsTotal;
    CounterPtr mRecvHTTPDataEventsTotal;

    // cache relative metric
    IntGaugePtr mConnTrackerNum;

    // protocol parsing metric
    CounterPtr mParseHTTPEventsSuccessTotal;
    CounterPtr mParseHTTPEventsFailTotal;

    // aggregation relative metric
    IntGaugePtr mAggMapEntitiesNum;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
};

class NetworkSecuritySelfMonitor : public BaseBPFMonitor {
public:
    NetworkSecuritySelfMonitor(const std::string& name, PluginMetricManagerPtr mgr) 
        : BaseBPFMonitor(name, mgr) {}

    void HandleStatistic(nami::eBPFStatistics& stats) override {
        if (!stats.updated_) return;
        UpdateMetricInner(stats);
    }
};

class ProcessSecuritySelfMonitor : public BaseBPFMonitor {
public:
    ProcessSecuritySelfMonitor(const std::string& name, PluginMetricManagerPtr mgr) 
        : BaseBPFMonitor(name, mgr) {}
};

class FileSecuritySelfMonitor : public BaseBPFMonitor {
public:
    FileSecuritySelfMonitor(const std::string& name, PluginMetricManagerPtr mgr) 
        : BaseBPFMonitor(name, mgr) {}
};

/**
 * eBPFSelfMonitorMgr is only used to manage the self-monitoring data in libnetwork_observer.so, updating the statistics through callbacks.
 */
class eBPFSelfMonitorMgr {
public:
    eBPFSelfMonitorMgr();
    void Init(const nami::PluginType type, PluginMetricManagerPtr mgr, const std::string& name, const std::string& project);
    void Release(const nami::PluginType type);
    void Suspend(const nami::PluginType type);
    void HandleStatistic(std::vector<nami::eBPFStatistics>& stats);
private:
    // `mLock` is used to protect mSelfMonitors
    ReadWriteLock mLock;
    std::array<std::unique_ptr<BaseBPFMonitor>, int(nami::PluginType::MAX)> mSelfMonitors;
    std::array<std::atomic_bool, int(nami::PluginType::MAX)> mInited;
    
#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
};

} // ebpf
} // logtail