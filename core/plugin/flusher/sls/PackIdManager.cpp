// Copyright 2024 iLogtail Authors
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

#include "plugin/flusher/sls/PackIdManager.h"

using namespace std;

namespace logtail {

optional<uint32_t> PackIdManager::TryGetAndIncPackSeq(int64_t key, uint32_t maxSequence) {
    lock_guard<mutex> lock(mMux);
    auto it = mPackIdSeq.find(key);
    if (it == mPackIdSeq.end()) {
        mPackIdSeq[key] = {1, time(nullptr)};
        return 0;
    }
    if (it->second.first > maxSequence) {
        return nullopt;
    }
    it->second.second = time(nullptr);
    return it->second.first++;
}

void PackIdManager::CleanTimeoutEntry() {
    lock_guard<mutex> lock(mMux);
    time_t curTime = time(nullptr);
    int64_t timeoutInterval = mPackIdSeq.size() > 100000 ? 86400 : (86400 * 30);
    for (auto it = mPackIdSeq.begin(); it != mPackIdSeq.end();) {
        if (curTime - it->second.second > timeoutInterval) {
            it = mPackIdSeq.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace logtail
