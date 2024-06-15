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
#include <ctime>
#include <mutex>
#include <unordered_map>

namespace logtail {

class PackIdManager {
public:
    PackIdManager(const PackIdManager&) = delete;
    PackIdManager& operator=(const PackIdManager&) = delete;

    static PackIdManager* GetInstance() {
        static PackIdManager instance;
        return &instance;
    }

    int64_t GetAndIncPackSeq(int64_t key);
    void CleanTimeoutEntry();

private:
    PackIdManager() = default;
    ~PackIdManager() = default;

    std::unordered_map<int64_t, std::pair<uint32_t, time_t>> mPackIdSeq;
    std::mutex mMux;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PackIdManagerUnittest;
#endif
};

} // namespace logtail
