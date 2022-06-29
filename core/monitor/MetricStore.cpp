// Copyright 2022 iLogtail Authors
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

#include "MetricStore.h"
#include <json/json.h>

namespace logtail {

std::string MetricStore::MetricToString() {
    Json::Value rootValue;
    ScopedSpinLock lock(mMonitorMetricLock);
    auto iter = mLogtailMetric.begin();
    for (; iter != mLogtailMetric.end(); ++iter) {
        rootValue[iter->first] = Json::Value(iter->second);
    }
    for (iter = mConstLogtailMetric.begin(); iter != mConstLogtailMetric.end(); ++iter) {
        rootValue[iter->first] = Json::Value(iter->second);
    }
    return rootValue.toStyledString();
}

std::string MetricStore::CheckLogtailStatus() {
    std::string metricStr;
    int32_t processFull = 0;
    int32_t sendFull = 0;
    int32_t senderInvalid = 0;
    int32_t openFdCount = 0;
    double processTps = 0.;

    ScopedSpinLock lock(mMonitorMetricLock);
    auto iter = mLogtailMetric.find("process_queue_full");
    if (iter != mLogtailMetric.end()) {
        processFull = StringTo<int32_t>(iter->second);
    }

    iter = mLogtailMetric.find("send_queue_full");
    if (iter != mLogtailMetric.end()) {
        sendFull = StringTo<int32_t>(iter->second);
    }

    iter = mLogtailMetric.find("sender_invalid");
    if (iter != mLogtailMetric.end()) {
        senderInvalid = StringTo<int32_t>(iter->second);
    }

    iter = mLogtailMetric.find("open_fd");
    if (iter != mLogtailMetric.end()) {
        openFdCount = StringTo<int32_t>(iter->second);
    }

    iter = mLogtailMetric.find("process_tps");
    if (iter != mLogtailMetric.end()) {
        processTps = StringTo<double>(iter->second);
    }

    metricStr = "ok";
    if (processTps > 20.)
        metricStr = "busy";
    if (openFdCount > 1000)
        metricStr = "many_log_files";
    if (processFull > 0)
        metricStr = "process_block";
    if (sendFull > 0)
        metricStr = "send_block";
    if (senderInvalid > 0)
        metricStr = "send_error";
    return metricStr;
}

} // namespace logtail