// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "config/OnetimeConfigManager.h"

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/ParamExtractor.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

OnetimeConfigManager::OnetimeConfigManager()
    : mCheckpointFilePath(filesystem::path(GetAgentDataDir()) / "onetime_config_info.json") {
}

OnetimeConfigStatus OnetimeConfigManager::GetOnetimeConfigStatusFromCheckpoint(const string& configName,
                                                                               uint64_t hash,
                                                                               uint32_t* expireTime) {
    auto it = mConfigExpireTimeCheckpoint.find(configName);
    if (it == mConfigExpireTimeCheckpoint.end()) {
        return OnetimeConfigStatus::NEW;
    }
    OnetimeConfigStatus status = OnetimeConfigStatus::OBSOLETE;
    if (it->second.first != hash) {
        status = OnetimeConfigStatus::NEW;
    } else if (time(nullptr) + 10 < it->second.second) {
        // we give 10 seconds buffer in case a pipeline is stopped immediately after creation
        status = OnetimeConfigStatus::OLD;
    }
    if (expireTime) {
        *expireTime = it->second.second;
    }
    mConfigExpireTimeCheckpoint.erase(it);
    return status;
}

bool OnetimeConfigManager::AddConfig(
    const string& configName, ConfigType type, const filesystem::path& filepath, uint64_t hash, uint32_t expireTime) {
    if (mConfigInfoMap.find(configName) != mConfigInfoMap.end()) {
        // should not happen
        return false;
    }
    mConfigInfoMap.try_emplace(configName, type, filepath, hash, expireTime);
    LOG_INFO(sLogger, ("onetime pipeline expire time", expireTime)("config", configName));
    return true;
}

bool OnetimeConfigManager::RemoveConfig(const string& configName) {
    auto it = mConfigInfoMap.find(configName);
    if (it == mConfigInfoMap.end()) {
        return false;
    }
    mConfigInfoMap.erase(it);
    return true;
}

void OnetimeConfigManager::DeleteTimeoutConfigFiles() {
    for (auto it = mConfigInfoMap.begin(); it != mConfigInfoMap.end();) {
        if (time(nullptr) >= it->second.mExpireTime) {
            error_code ec;
            if (filesystem::remove(it->second.mFilepath, ec)) {
                LOG_INFO(sLogger, ("onetime pipeline timeout", "delete config file succeeded")("config", it->first));
            } else if (ec) {
                LOG_WARNING(sLogger,
                            ("onetime pipeline timeout", "failed to delete config file")("error code", ec.value())(
                                "error msg", ec.message())("config", it->first));
            } else {
                LOG_WARNING(sLogger,
                            ("onetime pipeline timeout", "failed to delete config file")(
                                "error msg", "config file not existed")("config", it->first));
            }
            it = mConfigInfoMap.erase(it);
        } else {
            ++it;
        }
    }
}

void OnetimeConfigManager::ClearUnusedCheckpoints() {
    if (mConfigExpireTimeCheckpoint.empty()
        || time(nullptr) - Application::GetInstance()->GetStartTime()
            < INT32_FLAG(unused_checkpoints_clear_interval_sec)) {
        return;
    }
    mConfigExpireTimeCheckpoint.clear();
}

bool OnetimeConfigManager::LoadCheckpointFile() {
    error_code ec;
    filesystem::file_status s = filesystem::status(mCheckpointFilePath, ec);
    if (ec) {
        LOG_WARNING(sLogger,
                    ("failed to get checkpoint file status", "skip")("filepath", mCheckpointFilePath.string()));
        return false;
    }
    if (!filesystem::exists(s)) {
        LOG_WARNING(sLogger, ("checkpoint file not existed", "skip")("filepath", mCheckpointFilePath.string()));
        return false;
    }
    if (!filesystem::is_regular_file(s)) {
        LOG_WARNING(sLogger,
                    ("checkpoint file is not a regular file", "skip")("filepath", mCheckpointFilePath.string()));
        return false;
    }
    string content;
    if (!ReadFile(mCheckpointFilePath.string(), content)) {
        LOG_WARNING(sLogger, ("failed to open checkpoint file", "skip")("filepath", mCheckpointFilePath.string()));
        return false;
    }
    if (content.empty()) {
        LOG_WARNING(sLogger, ("empty checkpoint file", "skip")("filepath", mCheckpointFilePath.string()));
        return false;
    }
    Json::Value res;
    string errorMsg;
    if (!ParseJsonTable(content, res, errorMsg)) {
        LOG_WARNING(
            sLogger,
            ("checkpoint file corrupted", "skip")("error msg", errorMsg)("filepath", mCheckpointFilePath.string()));
        return false;
    }
    if (!res.isObject()) {
        LOG_WARNING(sLogger, ("checkpoint file is not json object", "skip")("filepath", mCheckpointFilePath.string()));
        return false;
    }
    for (const auto& config : res.getMemberNames()) {
        const auto& item = res[config];
        if (!item.isObject()) {
            LOG_WARNING(
                sLogger,
                ("checkpoint format invalid", "skip current config")("error msg", "value is a valid json object")(
                    "filepath", mCheckpointFilePath.string())("config", config));
            continue;
        }

        uint64_t hash = 0;
        if (!GetMandatoryUInt64Param(item, "config_hash", hash, errorMsg)) {
            LOG_WARNING(sLogger,
                        ("checkpoint format invalid", "skip current config")("error msg", errorMsg)(
                            "filepath", mCheckpointFilePath.string())("config", config));
            continue;
        }

        uint32_t expireTime = 0;
        if (!GetMandatoryUIntParam(item, "expire_time", expireTime, errorMsg)) {
            LOG_WARNING(sLogger,
                        ("checkpoint format invalid", "skip current config")("error msg", errorMsg)(
                            "filepath", mCheckpointFilePath.string())("config", config));
            continue;
        }
        mConfigExpireTimeCheckpoint.try_emplace(config, hash, expireTime);
    }
    return true;
}

void OnetimeConfigManager::DumpCheckpointFile() const {
    Json::Value res;
    // checkpoint must be dumped again, in case remote configs are partially loaded (which happens when local config
    // files are removed on restart and multiple remote config sources exists)
    // also, checkpoint must be dumped first, in case of confilict (e.g., config becomes continuous from onetime on
    // restart but soon turns back, where checkpoint should be overrided with new expire time)
    for (const auto& [config, item] : mConfigExpireTimeCheckpoint) {
        res[config] = Json::objectValue;
        auto& itemJson = res[config];
        itemJson["config_hash"] = item.first;
        itemJson["expire_time"] = item.second;
    }
    for (const auto& [config, info] : mConfigInfoMap) {
        res[config] = Json::objectValue;
        auto& itemJson = res[config];
        itemJson["config_hash"] = info.mHash;
        itemJson["expire_time"] = info.mExpireTime;
    }
    string errMsg;
    if (!WriteFile(mCheckpointFilePath.string(), res.toStyledString(), errMsg)) {
        LOG_WARNING(sLogger, ("failed to write checkpoint file", errMsg)("filepath", mCheckpointFilePath.string()));
    }
}

} // namespace logtail
