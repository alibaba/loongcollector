// Copyright 2025 LoongCollector Authors
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

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ebpf/type/NetworkObserverEvent.h"

namespace logtail::ebpf {

enum class ConvType {
    kUrl,
    kSql,
};

class Converger {
public:
    explicit Converger(size_t threshold = 1024) : mThreshold(threshold) { mIds.reserve(threshold); }
    void DoConverge(ConvType type, std::string& val);

private:
    size_t mThreshold;
    std::unordered_set<std::string> mIds;
    static std::string kDefaultVal;
};

class AppConvergerManager {
public:
    AppConvergerManager() = default;
    ~AppConvergerManager() = default;

    void RegisterApp(const std::shared_ptr<AppDetail>& app);

    void DeregisterApp(const std::shared_ptr<AppDetail>& app);

    void DoConverge(const std::shared_ptr<AppDetail>& app, ConvType type, std::string& val);

private:
    std::unordered_map<std::string, std::shared_ptr<Converger>> mAppConvergers;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConvergerUnittest;
#endif
};

} // namespace logtail::ebpf
