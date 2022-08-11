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

#include "ProcessObserver.h"
#include "logger/Logger.h"

namespace logtail {

ProcessObserver::ProcessObserver(uint64_t time) : mLastDataTimeNs(time) {
}


bool ProcessObserver::GarbageCollection(size_t size_limit_bytes, uint64_t nowTimeNs) {
    static auto sNetStatistic = NetworkStatistic::GetInstance();
    if (nowTimeNs - mLastDataTimeNs
        > (uint64_t)INT64_FLAG(sls_observer_network_process_timeout) * 1000LL * 1000LL * 1000LL) {
        return true;
    }
    for (auto iter = mAllConnections.begin(); iter != mAllConnections.end();) {
        ConnectionObserver* conn = iter->second;
        if (conn->GarbageCollection(size_limit_bytes, nowTimeNs)) {
            LOG_DEBUG(sLogger,
                      ("delete connection observer when gc, id", GetProcessMeta()->ToString())("conn id", iter->first));
            delete conn;
            iter = mAllConnections.erase(iter);
            ++sNetStatistic->mGCReleaseConnCount;
        } else {
            ++iter;
        }
    }
    if (!mAllConnections.empty()) {
        return false;
    }
    return nowTimeNs - mLastDataTimeNs
        > (uint64_t)INT64_FLAG(sls_observer_network_process_no_connection_timeout) * 1000LL * 1000LL * 1000LL;
}

} // namespace logtail
