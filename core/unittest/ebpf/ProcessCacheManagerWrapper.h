// Copyright 2025 LoongCollector Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include "MetricConstants.h"
#include "MetricRecord.h"
#include "ProcParser.h"
#include "common/memory/SourceBuffer.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/ProcessEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/MetricManager.h"
#include "security/bpf_process_event_type.h"
#include "type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/EBPFRawEventStub.h"
#include "unittest/ebpf/ProcFsStub.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessCacheManagerWrapper {
public:
    ProcessCacheManagerWrapper() {
        mEBPFAdapter = std::make_shared<EBPFAdapter>();
        auto now = std::chrono::system_clock::now();
        auto nowTm = std::chrono::system_clock::to_time_t(now);
        pid_t pid = getpid();
        std::stringstream ss;
        ss << "testroot_" << std::put_time(std::localtime(&nowTm), "%Y%m%d_%H%M%S_") << pid;

        mTestRoot = std::filesystem::path(GetProcessExecutionDir()) / ss.str();
        mProcDir = mTestRoot / "proc";
        DynamicMetricLabels dynamicLabels;
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
            mMetricRef,
            MetricCategory::METRIC_CATEGORY_RUNNER,
            {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER}},
            std::move(dynamicLabels));
        auto pollProcessEventsTotal = mMetricRef.CreateCounter(METRIC_RUNNER_EBPF_POLL_PROCESS_EVENTS_TOTAL);
        auto lossProcessEventsTotal = mMetricRef.CreateCounter(METRIC_RUNNER_EBPF_LOSS_PROCESS_EVENTS_TOTAL);
        auto processCacheMissTotal = mMetricRef.CreateCounter(METRIC_RUNNER_EBPF_PROCESS_CACHE_MISS_TOTAL);
        auto processCacheSize = mMetricRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_CACHE_SIZE);
        auto processDataMapSize = mMetricRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_DATA_MAP_SIZE);
        auto retryableEventCacheSize = mMetricRef.CreateIntGauge(METRIC_RUNNER_EBPF_RETRYABLE_EVENT_CACHE_SIZE);
        mProcessCacheManager = std::make_shared<ProcessCacheManager>(mEBPFAdapter,
                                                                     "test_host",
                                                                     mTestRoot.string(),
                                                                     mEventQueue,
                                                                     pollProcessEventsTotal,
                                                                     lossProcessEventsTotal,
                                                                     processCacheMissTotal,
                                                                     processCacheSize,
                                                                     processDataMapSize,
                                                                     retryableEventCacheSize);
    }
    ~ProcessCacheManagerWrapper() { std::filesystem::remove_all(mTestRoot); }

    void Clear() {
        mEventQueue = moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>();
        std::filesystem::remove_all(mTestRoot);
    }

    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    MetricsRecordRef mMetricRef;
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
    std::filesystem::path mTestRoot;
    std::filesystem::path mProcDir;
};
