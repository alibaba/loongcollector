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

#include "MetricExportor.h"
#include "sender/Sender.h"
#include "log_pb/sls_logs.pb.h"
#include "LogtailMetric.h"
#include "config_manager/ConfigManager.h"
#include "LogFileProfiler.h"
#include "MetricConstants.h"

using namespace sls_logs;
using namespace std;

namespace logtail {

MetricExportor::MetricExportor() : mSendInterval(60), mLastSendTime(time(NULL) - (rand() % (mSendInterval / 10)) * 10) {}

void MetricExportor::PushMetrics(bool forceSend) {
    int32_t curTime = time(NULL);
    if (!forceSend && (curTime - mLastSendTime < mSendInterval)) {
        return;
    }
    ReadMetrics::GetInstance()->UpdateMetrics();
    
    std::map<std::string, sls_logs::LogGroup*> logGroupMap;
    ReadMetrics::GetInstance()->ReadAsLogGroup(logGroupMap);

    std::map<std::string, sls_logs::LogGroup*>::iterator iter;
    for (iter = logGroupMap.begin(); iter != logGroupMap.end(); iter ++) {
        sls_logs::LogGroup* logGroup = iter->second;
        logGroup->set_category(METRIC_SLS_LOGSTORE_NAME);
        logGroup->set_source(LogFileProfiler::mIpAddr);
        logGroup->set_topic(METRIC_TOPIC_TYPE);
        if (METRIC_REGION_DEFAULT == iter->first) {
            ProfileSender::GetInstance()->SendToProfileProject(ProfileSender::GetInstance()->GetDefaultProfileRegion(), *logGroup);
        } else {
            ProfileSender::GetInstance()->SendToProfileProject(iter->first, *logGroup);
        }
        delete logGroup;
    }
}
}