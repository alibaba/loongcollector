/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <unordered_set>

#include "common/FeedbackInterface.h"
#include "queue/QueueKey.h"

namespace logtail {

class FeedbackInterfaceMock : public FeedbackInterface {
public:
    void Feedback(int64_t key) override { mFeedbackedKeys.insert(key); };

    size_t HasFeedback(QueueKey key) const { return mFeedbackedKeys.find(key) != mFeedbackedKeys.end(); }

    void Clear() { mFeedbackedKeys.clear(); }

private:
    std::unordered_set<QueueKey> mFeedbackedKeys;
};

} // namespace logtail
