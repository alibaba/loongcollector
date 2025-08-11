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

#include "ebpf/util/Converger.h"

#include "common/Flags.h"

DEFINE_FLAG_INT32(ebpf_apm_default_url_threshold, "apm default url threshold", 1024);

namespace logtail::ebpf {

std::string Converger::kDefaultVal = "{DEFAULT}";

void Converger::DoConverge(ConvType type, std::string& val) {
    if (type != ConvType::kUrl) {
        return;
    }
    if (mIds.size() < mThreshold) {
        mIds.insert(val);
        return;
    }

    if (mIds.find(val) == mIds.end()) {
        val = kDefaultVal;
    }
}

void AppConvergerManager::RegisterApp(const std::shared_ptr<AppDetail>& app) {
    if (app == nullptr) {
        return;
    }
    auto& converger = mAppConvergers[app->mConfigName];
    if (!converger) {
        // TODO (@qianlu.kk) apm server didn't implement any limit yet, so we cannot expose this param in COLLECTION
        // CONFIG
        converger = std::make_shared<Converger>(INT32_FLAG(ebpf_apm_default_url_threshold));
    }
}

void AppConvergerManager::DeregisterApp(const std::shared_ptr<AppDetail>& app) {
    if (app == nullptr) {
        return;
    }
    mAppConvergers.erase(app->mConfigName);
}

void AppConvergerManager::DoConverge(const std::shared_ptr<AppDetail>& app, ConvType type, std::string& val) {
    if (app == nullptr) {
        return;
    }

    std::shared_ptr<Converger> converger;
    auto it = mAppConvergers.find(app->mConfigName);
    if (it != mAppConvergers.end()) {
        converger = it->second;
    }

    if (converger == nullptr) {
        return;
    }

    converger->DoConverge(type, val);
}

} // namespace logtail::ebpf
