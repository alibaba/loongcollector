// Copyright 2023 iLogtail Authors
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

#include "event/BlockEventManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class BlockedEventManagerUnittest : public testing::Test {
public:
    void OnFeedback() const;
};

void BlockedEventManagerUnittest::OnFeedback() const {
    DevInode devInode(1, 2);
    Event e("dir", "file", EVENT_MODIFY, 0);

    BlockedEventManager::GetInstance()->UpdateBlockEvent(2, "test_config_2", e, devInode, 0);
    BlockedEventManager::GetInstance()->UpdateBlockEvent(0, "test_config_0", e, devInode, 12345678901);
    BlockedEventManager::GetInstance()->Feedback(1);
    BlockedEventManager::GetInstance()->UpdateBlockEvent(0, "test_config_0", e, devInode, time(nullptr));
    BlockedEventManager::GetInstance()->Feedback(0);

    vector<Event*> res;
    BlockedEventManager::GetInstance()->GetFeedbackEvent(res);
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL("dir", res[0]->GetSource());
    APSARA_TEST_EQUAL("file", res[0]->GetObject());
    APSARA_TEST_EQUAL(1U, BlockedEventManager::GetInstance()->mEventMap.size());
}

UNIT_TEST_CASE(BlockedEventManagerUnittest, OnFeedback)

} // namespace logtail

UNIT_TEST_MAIN
