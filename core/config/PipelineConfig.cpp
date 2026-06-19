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
#include "common/TimeUtil.h"
#include "config/OnetimeConfigInfoManager.h"
#include "config/feedbacker/ConfigFeedbackReceiver.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

static constexpr uint32_t minExpireTime = 600; // 10 minutes
static constexpr uint32_t maxExpireTime = 604800; // 1 week

static bool
IsOneTime(const string& configName, const Json::Value& global, uint32_t* timeout, bool* forceRerunWhenUpdate) {
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
    // Read ForceRerunWhenUpdate for onetime pipeline
    if (forceRerunWhenUpdate != nullptr) {
        *forceRerunWhenUpdate = false;
        const char* key = "ForceRerunWhenUpdate";
        const auto* it = global.find(key, key + strlen(key));
        if (it != nullptr && it->isBool()) {
            *forceRerunWhenUpdate = it->asBool();
        } else if (it != nullptr && !it->isBool()) {
            LOG_WARNING(sLogger,
                        ("param global.ForceRerunWhenUpdate is not of type bool",
                         "use default instead")("default", false)("config", configName));
        }
    }
    return true;
}

PipelineConfig::PipelineConfig(const string& name, unique_ptr<Json::Value>&& detail, const filesystem::path& filepath)
    : mName(name), mDetail(std::move(detail)), mFilePath(filepath) {
    mDetail->removeMember("enable");
    // Calculate inputs hash for onetime config
    const char* inputsKey = "inputs";
    const auto* inputsIt = mDetail->find(inputsKey, inputsKey + strlen(inputsKey));
    if (inputsIt != nullptr && inputsIt->isArray()) {
        mInputsHash = static_cast<uint64_t>(Hash(*inputsIt));
    }
    // Fold the onetime generation (if present) into the inputs hash.
    // The config server injects global.__onetime_generation__ (the command's CreatedAt)
    // when delivering an onetime command. A delete+recreate of a same-named command yields
    // a new generation but typically identical inputs, so without this fold the execution
    // layer would classify it as UPDATED (resume from checkpoint) instead of NEW (fresh
    // rerun). Folding the generation into mInputsHash makes a generation change alter the
    // inputs hash, which OnetimeConfigInfoManager::GetOnetimeConfigStatus treats as NEW.
    // A genuine in-place edit of an existing command keeps the same generation, so its
    // inputs hash is unchanged and the UPDATED/resume semantics are preserved.
    // Non-onetime configs never carry this key, so their inputs hash is unaffected.
    const char* globalKey = "global";
    const auto* globalIt = mDetail->find(globalKey, globalKey + strlen(globalKey));
    if (globalIt != nullptr && globalIt->isObject()) {
        const char* genKey = "__onetime_generation__";
        const auto* genIt = globalIt->find(genKey, genKey + strlen(genKey));
        if (genIt != nullptr && genIt->isNumeric()) {
            uint64_t generation = genIt->asUInt64();
            // Order-independent mix (same constant as boost::hash_combine's golden ratio).
            mInputsHash ^= generation + 0x9e3779b97f4a7c15ULL + (mInputsHash << 6) + (mInputsHash >> 2);
        }
    }
    mConfigHash = static_cast<uint64_t>(Hash(*mDetail));
}

bool PipelineConfig::GetExpireTimeIfOneTime(const Json::Value& global) {
    if (!IsOneTime(mName, global, &mExcutionTimeout, &mForceRerunWhenUpdate)) {
        return true;
    }
    uint32_t expireTime = 0;
    auto status = OnetimeConfigInfoManager::GetInstance()->GetOnetimeConfigStatus(
        mName, mConfigHash, mForceRerunWhenUpdate, mInputsHash, mExcutionTimeout, &expireTime);
    switch (status) {
        case OnetimeConfigStatus::OLD:
            // OLD状态表示是已经存在配置，保持原样
            mOnetimeStartTime = expireTime - mExcutionTimeout;
            mOnetimeExpireTime = expireTime;
            mIsRunningBeforeStart = true;
            LOG_INFO(sLogger, ("recover config expire time from checkpoint, expire time", expireTime)("config", mName));
            return true;
        case OnetimeConfigStatus::NEW:
            // NEW状态表示是新配置，或已有配置Rerun了
            mOnetimeStartTime = static_cast<uint32_t>(GetCurrentTimeInSeconds());
            mOnetimeExpireTime = mOnetimeStartTime.value() + mExcutionTimeout;
            return true;
        case OnetimeConfigStatus::UPDATED:
            // UPDATED状态表示配置hash改变但input hash未变，保持原有checkpoint，但是更新过期时间
            mOnetimeStartTime = static_cast<uint32_t>(GetCurrentTimeInSeconds());
            mOnetimeExpireTime = mOnetimeStartTime.value() + mExcutionTimeout;
            mIsRunningBeforeStart = true;
            LOG_INFO(sLogger,
                     ("config hash changed but inputs hash unchanged", "keep existing checkpoint, update expire time")(
                         "expire time", mOnetimeExpireTime.value())("config", mName));
            return true;
        case OnetimeConfigStatus::OBSOLETE: {
            // OBSOLETE状态表示配置过期，删除配置文件。
            // 注意：此处不调用 FeedbackOnetimePipelineConfigStatus(DELETED)。
            // LoadConfigFile() 已在启动时将 mOnetimePipelineConfigInfoMap 中的状态设置为 APPLIED，
            // 若在此覆盖为 DELETED，下次心跳会发送 version=-1 且 proto status=0 (UNSET)，
            // 导致服务端记录状态变为 "0 (UNKNOWN)"。
            // 服务端会独立判断 onetime 配置已过期（expire_time < now），并发 expire_time=-1
            // cancel 给 agent，由 agent 负责清理 mOnetimePipelineConfigInfoMap 中的条目，
            // 服务端最终保留的状态为 APPLIED，符合预期。
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
