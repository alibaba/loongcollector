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

#include "file_server/WatchManager.h"

using namespace std;

namespace logtail {

WatchManager::WatchManager() : mEventListener(EventListener::GetInstance()) {
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_WATCH_MANAGER}});
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

int WatchManager::AcquireWatch(const string& path) {
    return DoAddWatch(path.c_str());
}

bool WatchManager::ReleaseWatch(int wd) {
    return DoRemoveWatch(wd);
}

int WatchManager::DoAddWatch(const char* path) {
    return mEventListener->AddWatch(path);
}

bool WatchManager::DoRemoveWatch(int wd) {
    return mEventListener->RemoveWatch(wd);
}

} // namespace logtail
