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

#pragma once
#include "network/NetworkObserver.h"


namespace logtail {

enum LOGTAIL_OBSERVER_CATEGORY {
    LOGTAILOBSERVERCATEGORY_NETWORK_OBSERVER,
    LOGTAILOBSERVERCATEGORY_NUMBER,
};

class ObserverManager {
public:
    static ObserverManager* GetInstance() {
        static auto sInstance = new ObserverManager;
        return sInstance;
    }
    // lock
    void HoldOn(bool exitFlag = false) { NetworkObserver::GetInstance()->HoldOn(exitFlag); }

    // unlock
    void Resume() { NetworkObserver::GetInstance()->Resume(); }

    // processing under locked
    void Reload() { NetworkObserver::GetInstance()->Reload(); }

    int Status() {
        int flag = 0;
        flag |= NetworkConfig::GetInstance()->mEnabled ? 1 << LOGTAILOBSERVERCATEGORY_NETWORK_OBSERVER : 0;
        return flag;
    }

private:
    ObserverManager() = default;
    ~ObserverManager() = default;
};
} // namespace logtail