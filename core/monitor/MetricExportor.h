/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Monitor.h"
#include "profile_sender/ProfileSender.h"


namespace logtail {

class MetricExportor {
public:
    static MetricExportor* GetInstance() {
        static MetricExportor* ptr = new MetricExportor();
        return ptr;
    }
    void PushMetrics(bool forceSend);

private:
    MetricExportor();

    void PushCppMetrics();
    void PushGoMetrics();

    // Send Methods
    void SendToSLS(std::map<std::string, sls_logs::LogGroup*>& logGroupMap);
    void SendToLocalFile(std::string& metricsContent, const std::string metricsFileNamePrefix);
    void SendGoAgentLevelMetrics(std::map<std::string, std::string>& metrics);

    // inner methods
    void SerializeGoPluginMetricsListToLogGroupMap(std::vector<std::map<std::string, std::string>>& goPluginMetircsList,
                                                   std::map<std::string, sls_logs::LogGroup*>& goLogGroupMap);
    void SerializeGoPluginMetricsListToString(std::vector<std::map<std::string, std::string>>& goPluginMetircsList,
                                              std::string& metricsContent);

    int32_t mSendInterval;
    int32_t mLastSendTime;
    // go process-level metrics
    DoubleGaugePtr mGlobalCpuGo;
    IntGaugePtr mGlobalMemGo;
};

} // namespace logtail