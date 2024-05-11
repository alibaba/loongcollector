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

#include "queue/QueueKeyManager.h"

using namespace std;

namespace logtail {

QueueKey QueueKeyManager::GetKey(const string& name) {
    lock_guard<mutex> lock(mMux);
    auto iter = mNameKeyMap.find(name);
    if (iter != mNameKeyMap.end()) {
        return iter->second;
    }
    mNameKeyMap[name] = mNextKey++;
    mKeyNameMap[mNextKey - 1] = name;
    return mNextKey - 1;
}

std::string QueueKeyManager::GetName(QueueKey key) {
    lock_guard<mutex> lock(mMux);
    auto iter = mKeyNameMap.find(key);
    if (iter == mKeyNameMap.end()) {
        return "";
    }
    return iter->second;
}

void QueueKeyManager::RemoveKey(QueueKey key) {
    lock_guard<mutex> lock(mMux);
    auto iter = mKeyNameMap.find(key);
    if (iter != mKeyNameMap.end()) {
        mNameKeyMap.erase(iter->second);
        mKeyNameMap.erase(iter);
    }
}

#ifdef APSARA_UNIT_TEST_MAIN
void QueueKeyManager::Clear() {
    mNextKey = 0;
    mKeyNameMap.clear();
    mNameKeyMap.clear();
}
#endif

} // namespace logtail
