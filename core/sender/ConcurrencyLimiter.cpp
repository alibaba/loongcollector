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

#include "sender/ConcurrencyLimiter.h"

// TODO: temporarily used
#include "app_config/AppConfig.h"
#include "sender/Sender.h"

using namespace std;

namespace logtail {

// TODO: this is temporary, should be redefined after sender refactor
bool ConcurrencyLimiter::IsValidToPop(const string& key) {
    lock_guard<mutex> lock(mMux);
    auto iter = mLimitMap.find(key);
    if (iter == mLimitMap.end()) {
        mLimitMap[key] = -1;
        return true;
    }
    if (iter->second != 0) {
        return true;
    }
    return false;
}

void ConcurrencyLimiter::PostPop(const string& key) {
    lock_guard<mutex> lock(mMux);
    auto iter = mLimitMap.find(key);
    if (iter != mLimitMap.end()) {
        if (iter->second > 0) {
            --iter->second;
        }
    }
}

void ConcurrencyLimiter::OnSendDone(const string& key) {
    lock_guard<mutex> lock(mMux);
    auto iter = mLimitMap.find(key);
    if (iter != mLimitMap.end()) {
        if (iter->second >= 0) {
            if (++iter->second > AppConfig::GetInstance()->GetSendRequestConcurrency()) {
                iter->second = -1;
            }
        }
    }
}

void ConcurrencyLimiter::Reset(const string& key) {
    lock_guard<mutex> lock(mMux);
    auto iter = mLimitMap.find(key);
    if (iter != mLimitMap.end()) {
        if (Sender::Instance()->mRegionRefCntMap.empty()) {
            iter->second = AppConfig::GetInstance()->GetSendRequestConcurrency();
        } else {
            iter->second
                = AppConfig::GetInstance()->GetSendRequestConcurrency() / Sender::Instance()->mRegionRefCntMap.size();
        }
    }
}

} // namespace logtail
