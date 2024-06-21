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
#include <map>
#include <mutex>

#include "sender/Limiter.h"

namespace logtail {

class ConcurrencyLimiter : public Limiter {
public:
    bool IsValidToPop(const std::string& key) override;
    void PostPop(const std::string& key) override;
    void OnSendDone(const std::string& key);

    void Reset(const std::string& key); // TODO: temporarily used

private:
    mutable std::mutex mMux;
    std::map<std::string, int32_t> mLimitMap; // should be uint32_t after refactor
};

} // namespace logtail
