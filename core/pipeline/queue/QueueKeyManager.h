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

#include <mutex>
#include <string>
#include <unordered_map>

#include "pipeline/queue/QueueKey.h"

namespace logtail {

class QueueKeyManager {
public:
    QueueKeyManager(const QueueKeyManager&) = delete;
    QueueKeyManager& operator=(const QueueKeyManager&) = delete;

    static QueueKeyManager* GetInstance() {
        static QueueKeyManager instance;
        return &instance;
    }

    QueueKey GetKey(const std::string& name);
    bool HasKey(const std::string& name);
    bool RemoveKey(QueueKey key);
    const std::string& GetName(QueueKey key);

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    QueueKeyManager() = default;
    ~QueueKeyManager() = default;

    mutable std::mutex mMux;
    QueueKey mNextKey = 0;
    std::unordered_map<std::string, QueueKey> mNameKeyMap;
    std::unordered_map<QueueKey, std::string> mKeyNameMap;
};

} // namespace logtail
