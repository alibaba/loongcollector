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

#include <memory>
#include <unordered_map>

#include "host_monitor/collector/BaseCollector.h"
namespace logtail {

class CollectorManager {
public:
    static CollectorManager* GetInstance() {
        static CollectorManager sInstance;
        return &sInstance;
    }

    std::shared_ptr<BaseCollector> GetCollector(const std::string& collectorName);

private:
    CollectorManager();
    ~CollectorManager() = default;

    template <typename T>
    void RegisterCollector();

    std::unordered_map<std::string, std::shared_ptr<BaseCollector>> mCollectorMap;
};

} // namespace logtail
