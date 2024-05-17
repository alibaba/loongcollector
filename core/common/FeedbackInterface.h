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

#pragma once

#include <cstdint>

namespace logtail {

class FeedbackInterface {
public:
    virtual ~FeedbackInterface() = default;

    virtual void Feedback(int64_t key) = 0;

    // TODO: should not be a common method after flusher refactorization
    virtual bool IsValidToPush(int64_t key) const { return true; }
};

} // namespace logtail
