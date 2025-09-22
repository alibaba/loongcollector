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

#include <gtest/gtest.h>
#include <memory>

#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/plugin/network_observer/NetworkObserverManager.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "unittest/ebpf/ManagerConfigPairTestFramework.h"
#include "unittest/Unittest.h"

using namespace logtail;
using namespace logtail::ebpf;

class NetworkObserverManagerConfigPairUnittest : public NetworkObserverManagerConfigPairTest {
protected:
    std::shared_ptr<AbstractManager> CreateManagerInstance() override {
        return NetworkObserverManager::Create(
            mProcessCacheManager,
            mMockEBPFAdapter,
            *mEventQueue,
            mEventPool.get());
    }
};

TEST_F(NetworkObserverManagerConfigPairUnittest, TestDifferentConfigNamesReplacement) {
    TestDifferentConfigNamesReplacement();
}

TEST_F(NetworkObserverManagerConfigPairUnittest, TestSameConfigNameUpdate) {
    TestSameConfigNameUpdate();
}

TEST_F(NetworkObserverManagerConfigPairUnittest, TestMultipleConfigsComplexScenario) {
    TestMultipleConfigsComplexScenario();
}

UNIT_TEST_MAIN
