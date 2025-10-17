/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "host_monitor/collector/BaseCollector.h"

#include <iostream>

#include "host_monitor/HostMonitorContext.h"
#include "logger/Logger.h"

namespace logtail {

bool CollectorInstance::Init(HostMonitorContext& collectContext) {
    mStartTime = collectContext.mStartTime;
    return mCollector->Init(collectContext);
}

bool BaseCollector::Init(HostMonitorContext& collectContext) {
    switch (collectContext.mCollectType) {
        case HostMonitorCollectType::kSingleValue:
            collectContext.mCountPerReport = 1;
            collectContext.mCount = 0;
            collectContext.mCollectInterval = collectContext.mReportInterval;
            return true;
        case HostMonitorCollectType::kMultiValue: {
            auto collectInterval = GetCollectInterval();
            if (collectInterval.count() == 0) {
                return false;
            }
            if (collectInterval.count() > collectContext.mReportInterval.count()) {
                LOG_ERROR(sLogger,
                          ("host monitor", "collect interval is greater than report interval")(
                              "collect interval", collectInterval.count())("report interval",
                                                                           collectContext.mReportInterval.count()));
                return false;
            }
            if (collectContext.mReportInterval.count() % collectInterval.count() != 0) {
                LOG_ERROR(sLogger,
                          ("host monitor", "report interval is not divisible by collect interval")(
                              "report interval", collectContext.mReportInterval.count())("collect interval",
                                                                                         collectInterval.count()));
                return false;
            }
            collectContext.mCountPerReport = collectContext.mReportInterval.count() / collectInterval.count();
            collectContext.mCount = 0;
            collectContext.mCollectInterval = collectInterval;
            return true;
        }
        default:
            return false;
    }
}

} // namespace logtail
