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

#include "MetricManager.h"

#include <iomanip>
#include <set>
#include <sstream>

#include "json/json.h"

#include "Monitor.h"
#include "app_config/AppConfig.h"
#include "go_pipeline/LogtailPlugin.h"
#include "logger/Logger.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "provider/Provider.h"


using namespace sls_logs;
using namespace std;

namespace logtail {

const string METRIC_EXPORT_TYPE_GO = "direct";
const string METRIC_EXPORT_TYPE_CPP = "cpp_provided";

namespace {
// Format a gauge value to match the legacy GetGoMetrics map form byte-for-byte.
// The Go side serializes gauges via strconv.FormatFloat(v, 'f', 4, 64) (see
// pkg/selfmonitor/metrics_imp_v2.go), i.e. fixed notation with 4 fractional
// digits. std::to_string(double) instead emits 6 digits ("12" -> "12.000000"),
// which diverges from the legacy string shape downstream parsers expect.
std::string FormatGoMetricGauge(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << value;
    return oss.str();
}
} // namespace

WriteMetrics::~WriteMetrics() {
    Clear();
}

void WriteMetrics::CreateMetricsRecordRef(MetricsRecordRef& ref,
                                          const std::string& category,
                                          MetricLabels&& labels,
                                          DynamicMetricLabels&& dynamicLabels) {
    MetricsRecord* cur = new MetricsRecord(
        category, std::make_shared<MetricLabels>(labels), std::make_shared<DynamicMetricLabels>(dynamicLabels));
    ref.SetMetricsRecord(cur);
}

void WriteMetrics::CommitMetricsRecordRef(MetricsRecordRef& ref) {
    std::lock_guard<std::mutex> lock(mMutex);
    ref.mMetrics->SetNext(mHead);
    mHead = ref.mMetrics;
    ref.mMetrics->MarkCommitted();
}

MetricsRecord* WriteMetrics::GetHead() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mHead;
}

void WriteMetrics::Clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    while (mHead) {
        MetricsRecord* toDeleted = mHead;
        mHead = mHead->GetNext();
        delete toDeleted;
    }
}

MetricsRecord* WriteMetrics::DoSnapshot() {
    // new read head
    MetricsRecord* snapshot = nullptr;
    MetricsRecord* toDeleteHead = nullptr;
    MetricsRecord* tmp = nullptr;

    MetricsRecord emptyHead;
    MetricsRecord* preTmp = nullptr;

    int writeMetricsTotal = 0;
    int writeMetricsDeleteTotal = 0;
    int metricsSnapshotTotal = 0;

    // find the first undeleted node and set as new mHead
    {
        std::lock_guard<std::mutex> lock(mMutex);
        emptyHead.SetNext(mHead);
        preTmp = &emptyHead;
        tmp = preTmp->GetNext();
        bool findHead = false;
        while (tmp) {
            if (tmp->IsDeleted()) {
                preTmp->SetNext(tmp->GetNext());
                tmp->SetNext(toDeleteHead);
                toDeleteHead = tmp;
                tmp = preTmp->GetNext();
                writeMetricsTotal++;
            } else {
                // find head
                mHead = tmp;
                preTmp = mHead;
                tmp = tmp->GetNext();
                findHead = true;
                break;
            }
        }
        // if no undeleted node, set null to mHead
        if (!findHead) {
            mHead = nullptr;
            preTmp = mHead;
        }
    }

    // copy head
    if (preTmp) {
        MetricsRecord* newMetrics = preTmp->Collect();
        newMetrics->SetNext(snapshot);
        snapshot = newMetrics;
        metricsSnapshotTotal++;
        writeMetricsTotal++;
    }

    while (tmp) {
        writeMetricsTotal++;
        if (tmp->IsDeleted()) {
            preTmp->SetNext(tmp->GetNext());
            tmp->SetNext(toDeleteHead);
            toDeleteHead = tmp;
            tmp = preTmp->GetNext();
        } else {
            MetricsRecord* newMetrics = tmp->Collect();
            newMetrics->SetNext(snapshot);
            snapshot = newMetrics;
            preTmp = tmp;
            tmp = tmp->GetNext();
            metricsSnapshotTotal++;
        }
    }

    while (toDeleteHead) {
        MetricsRecord* toDelete = toDeleteHead;
        toDeleteHead = toDeleteHead->GetNext();
        delete toDelete;
        writeMetricsDeleteTotal++;
    }
    LOG_INFO(sLogger,
             ("writeMetricsTotal", writeMetricsTotal)("writeMetricsDeleteTotal", writeMetricsDeleteTotal)(
                 "metricsSnapshotTotal", metricsSnapshotTotal));
    return snapshot;
}

ReadMetrics::~ReadMetrics() {
    Clear();
}

void ReadMetrics::ReadAsSelfMonitorMetricEvents(std::vector<SelfMonitorMetricEvent>& metricEventList) const {
    ReadLock lock(mReadWriteLock);
    // c++ metrics
    MetricsRecord* tmp = mHead;
    while (tmp) {
        metricEventList.emplace_back(SelfMonitorMetricEvent(tmp));
        tmp = tmp->GetNext();
    }
    // go metrics
    for (auto metrics : mGoMetrics) {
        metricEventList.emplace_back(SelfMonitorMetricEvent(move(metrics)));
    }
}

void ReadMetrics::UpdateMetrics() {
// Todo: windows 上的 cgo 内存有问题，调用 GetGoMetrics 函数时可能会 crash，需要修复
#ifndef _MSC_VER
    // go指标在Cpp指标前获取，是为了在 Cpp 部分指标做 SnapShot
    // 前（即调用 ReadMetrics::GetInstance()->UpdateMetrics() 函数），把go部分的进程级指标填写到 Cpp
    // 的进程级指标中去，随Cpp的进程级指标一起输出
    if (LogtailPlugin::GetInstance()->IsPluginOpened()) {
        vector<map<string, string>> goCppProvidedMetircsList;
        LogtailPlugin::GetInstance()->GetGoMetrics(goCppProvidedMetircsList, METRIC_EXPORT_TYPE_CPP);
        UpdateGoCppProvidedMetrics(goCppProvidedMetircsList);

        {
            WriteLock lock(mReadWriteLock);
            mGoMetrics.clear();
            LogtailPlugin::GetInstance()->GetGoMetrics(mGoMetrics, METRIC_EXPORT_TYPE_GO);
        }
    }
#endif
    // 获取c++指标
    MetricsRecord* snapshot = WriteMetrics::GetInstance()->DoSnapshot();
    MetricsRecord* toDelete;
    {
        // Only lock when change head
        WriteLock lock(mReadWriteLock);
        toDelete = mHead;
        mHead = snapshot;
    }
    // delete old linklist
    while (toDelete) {
        MetricsRecord* obj = toDelete;
        toDelete = toDelete->GetNext();
        delete obj;
    }
}

MetricsRecord* ReadMetrics::GetHead() {
    WriteLock lock(mReadWriteLock);
    return mHead;
}

void ReadMetrics::Clear() {
    WriteLock lock(mReadWriteLock);
    while (mHead) {
        MetricsRecord* toDelete = mHead;
        mHead = mHead->GetNext();
        delete toDelete;
    }
}

// metrics from Go that are provided by cpp
void ReadMetrics::UpdateGoCppProvidedMetrics(vector<map<string, string>>& metricsList) {
    if (metricsList.size() == 0) {
        return;
    }

    for (auto metrics : metricsList) {
        for (auto metric : metrics) {
            if (metric.first == METRIC_AGENT_MEMORY_GO) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoMemory(stoi(metric.second));
            }
            if (metric.first == METRIC_AGENT_GO_ROUTINES_TOTAL) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoRoutinesTotal(stoi(metric.second));
            }
        }
    }
}

bool ReadMetrics::ParseGoMetricsPB(const std::string& pbData,
                                   std::vector<std::map<std::string, std::string>>& metricsList) {
    if (pbData.empty()) {
        return true;
    }
    models::PipelineEventGroup pbGroup;
    if (!pbGroup.ParseFromString(pbData)) {
        LOG_WARNING(sLogger, ("receive go metrics pb", "failed to parse protobuf")("size", pbData.size()));
        return false;
    }
    PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
    std::string errMsg;
    if (!TransferPBToPipelineEventGroup(pbGroup, eventGroup, errMsg)) {
        LOG_WARNING(sLogger, ("receive go metrics pb", "failed to transfer pb")("err", errMsg));
        return false;
    }

    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    for (auto& eventPtr : eventGroup.GetEvents()) {
        if (!eventPtr.Is<MetricEvent>()) {
            continue;
        }
        const MetricEvent& metricEvent = eventPtr.Cast<MetricEvent>();
        // category is carried in MetricEvent.Name; reinsert it as the metric_category
        // label so the record matches the GetGoMetrics map form expected by
        // SelfMonitorMetricEvent(const std::map&).
        Json::Value labels(Json::objectValue);
        labels["metric_category"] = metricEvent.GetName().to_string();
        for (auto tag = metricEvent.TagsBegin(); tag != metricEvent.TagsEnd(); ++tag) {
            labels[tag->first.to_string()] = tag->second.to_string();
        }
        Json::Value counters(Json::objectValue);
        Json::Value gauges(Json::objectValue);
        const auto* multiValues = metricEvent.GetValue<UntypedMultiDoubleValues>();
        if (multiValues != nullptr) {
            for (auto it = multiValues->ValuesBegin(); it != multiValues->ValuesEnd(); ++it) {
                const std::string name = it->first.to_string();
                const UntypedMultiDoubleValue& value = it->second;
                if (value.MetricType == UntypedValueMetricType::MetricTypeCounter) {
                    counters[name] = std::to_string(static_cast<uint64_t>(value.Value));
                } else {
                    gauges[name] = FormatGoMetricGauge(value.Value);
                }
            }
        }
        std::map<std::string, std::string> record;
        record["labels"] = Json::writeString(writerBuilder, labels);
        record["counters"] = Json::writeString(writerBuilder, counters);
        record["gauges"] = Json::writeString(writerBuilder, gauges);
        metricsList.emplace_back(std::move(record));
    }
    return true;
}

} // namespace logtail
