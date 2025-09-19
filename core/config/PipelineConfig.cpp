// Copyright 2024 iLogtail Authors
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

#include "config/PipelineConfig.h"

#include "common/JsonUtil.h"
#include "config/OnetimeConfigInfoManager.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

static constexpr uint32_t minExpireTime = 600; // 10 minutes
static constexpr uint32_t maxExpireTime = 604800; // 1 week

static bool IsOneTime(const string& configName, const Json::Value& global, uint32_t* timeout) {
    const char* key = "ExcutionTimeout";
    auto it = global.find(key, key + strlen(key));
    if (it == nullptr) {
        return false;
    }
    if (it->isUInt()) {
        *timeout = it->asUInt();
        if (*timeout > maxExpireTime) {
            *timeout = maxExpireTime;
            LOG_WARNING(sLogger,
                        ("param global.ExcutionTimeout is too large",
                         "use maximum instead")("maximum", maxExpireTime)("config", configName));
        } else if (*timeout < minExpireTime) {
            *timeout = minExpireTime;
            LOG_WARNING(sLogger,
                        ("param global.ExcutionTimeout is too small",
                         "use minimum instead")("minimum", minExpireTime)("config", configName));
        }
    } else {
        *timeout = maxExpireTime;
        LOG_WARNING(sLogger,
                    ("param global.ExcutionTimeout is not of type uint",
                     "use maximum instead")("maximum", maxExpireTime)("config", configName));
    }
    return true;
}

PipelineConfig::PipelineConfig(const string& name, unique_ptr<Json::Value>&& detail, const filesystem::path& filepath)
    : mName(name), mDetail(std::move(detail)), mFilePath(filepath) {
    mDetail->removeMember("enable");
    mConfigHash = static_cast<uint64_t>(Hash(*mDetail));
}

bool PipelineConfig::GetExpireTimeIfOneTime(const Json::Value& global) {
    uint32_t timeout = 0;
    if (!IsOneTime(mName, global, &timeout)) {
        return true;
    }
    uint32_t expireTime = 0;
    auto status = OnetimeConfigInfoManager::GetInstance()->GetOnetimeConfigStatusFromCheckpoint(
        mName, mConfigHash, &expireTime);
    switch (status) {
        case OnetimeConfigStatus::OLD:
            mOnetimeStartTime = expireTime - timeout;
            mOnetimeExpireTime = expireTime;
            mIsRunningBeforeStart = true;
            LOG_INFO(sLogger, ("recover config expire time from checkpoint, expire time", expireTime)("config", mName));
            return true;
        case OnetimeConfigStatus::NEW:
            mOnetimeStartTime = time(nullptr);
            mOnetimeExpireTime = mOnetimeStartTime.value() + timeout;
            return true;
        case OnetimeConfigStatus::OBSOLETE: {
            error_code ec;
            if (filesystem::remove(mFilePath, ec)) {
                LOG_INFO(sLogger,
                         ("onetime config expired on init",
                          "delete config file succeeded")("expire time", expireTime)("config", mName));
            } else if (ec) {
                LOG_WARNING(
                    sLogger,
                    ("onetime config expired on init", "failed to delete config file")("error code", ec.value())(
                        "error msg", ec.message())("expire time", expireTime)("config", mName));
            } else {
                LOG_WARNING(sLogger,
                            ("onetime config expired on init", "failed to delete config file")(
                                "error msg", "config file not existed")("expire time", expireTime)("config", mName));
            }
            return false;
        }
        default:
            // should not happen
            break;
    }
    return false;
}

} // namespace logtail
