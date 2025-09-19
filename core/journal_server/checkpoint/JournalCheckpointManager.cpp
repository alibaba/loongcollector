/*
 * Copyright 2025 iLogtail Authors
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

#include "JournalCheckpointManager.h"
#include "logger/Logger.h"
#include "app_config/AppConfig.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>

namespace logtail {

JournalCheckpointManager& JournalCheckpointManager::GetInstance() {
    static JournalCheckpointManager sInstance;
    return sInstance;
}

//=============================================================================
// 1. 底层基础服务层
//=============================================================================

std::string JournalCheckpointManager::makeCheckpointKey(const std::string& configName, size_t configIndex) const {
    std::ostringstream oss;
    oss << configName << "_" << configIndex;
    return oss.str();
}

bool JournalCheckpointManager::saveCheckpointToDisk(const std::string& key, const JournalCheckpoint& checkpoint) {
    std::string checkpointFile = GetAgentDataDir() + "journal_check_point";
    
    // 读取现有文件（如果存在）
    Json::Value root;
    std::ifstream infile(checkpointFile.c_str());
    if (infile.is_open()) {
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string jsonParseErrs;
        std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
        infile.close();
        if (!reader->parse(content.data(), content.data() + content.size(), &root, &jsonParseErrs)) {
            LOG_WARNING(sLogger, ("failed to parse existing journal checkpoint file", checkpointFile)("error", jsonParseErrs));
            root = Json::Value(Json::objectValue);
        }
    }
    
    // 如果root为空或格式错误，初始化
    if (root.isNull() || !root.isObject()) {
        root = Json::Value(Json::objectValue);
    }
    
    // 构建checkpoint数据
    Json::Value checkpointJson;
    checkpointJson["cursor"] = checkpoint.cursor;
    checkpointJson["update_time"] = Json::Value(static_cast<int64_t>(time(NULL)));
    
    // 确保checkpoints对象存在
    if (!root.isMember("checkpoints")) {
        root["checkpoints"] = Json::Value(Json::objectValue);
    }
    root["checkpoints"][key] = checkpointJson;
    root["version"] = Json::Value(1);
    
    // 写入文件
    std::ofstream fout(checkpointFile.c_str());
    if (!fout.is_open()) {
        LOG_ERROR(sLogger, ("failed to open journal checkpoint file for write", checkpointFile));
        return false;
    }
    
    fout << root.toStyledString();
    fout.close();
    
    if (fout.bad()) {
        LOG_ERROR(sLogger, ("failed to write journal checkpoint file", checkpointFile));
        return false;
    }
    
    LOG_DEBUG(sLogger, ("journal checkpoint saved", key)("cursor", checkpoint.cursor));
    return true;
}

bool JournalCheckpointManager::loadCheckpointFromDisk(const std::string& key, JournalCheckpoint& checkpoint) {
    std::string checkpointFile = GetAgentDataDir() + "journal_check_point";
    
    std::ifstream infile(checkpointFile.c_str());
    if (!infile.is_open()) {
        LOG_DEBUG(sLogger, ("journal checkpoint file not found", checkpointFile));
        return false;
    }
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string jsonParseErrs;
    std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();
    
    if (!reader->parse(content.data(), content.data() + content.size(), &root, &jsonParseErrs)) {
        LOG_WARNING(sLogger, ("failed to parse journal checkpoint file", checkpointFile)("error", jsonParseErrs));
        return false;
    }
    
    // 查找checkpoint
    if (!root.isMember("checkpoints") || !root["checkpoints"].isMember(key)) {
        LOG_DEBUG(sLogger, ("journal checkpoint not found", key));
        return false;
    }
    
    const Json::Value& checkpointJson = root["checkpoints"][key];
    if (!checkpointJson.isMember("cursor")) {
        LOG_WARNING(sLogger, ("invalid journal checkpoint format", key));
        return false;
    }
    
    // 加载数据
    checkpoint.cursor = checkpointJson["cursor"].asString();
    checkpoint.createTime = std::chrono::steady_clock::now();
    checkpoint.updateTime = std::chrono::steady_clock::now();
    checkpoint.changed = false;
    
    LOG_DEBUG(sLogger, ("journal checkpoint loaded", key)("cursor", checkpoint.cursor));
    return true;
}

bool JournalCheckpointManager::removeCheckpointFromDisk(const std::string& key) {
    std::string checkpointFile = GetAgentDataDir() + "journal_check_point";
    
    // 读取现有文件
    Json::Value root;
    std::ifstream infile(checkpointFile.c_str());
    if (!infile.is_open()) {
        LOG_DEBUG(sLogger, ("journal checkpoint file not found during removal", checkpointFile));
        return true; // 文件不存在，认为删除成功
    }
    
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string jsonParseErrs;
    std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();
    
    if (!reader->parse(content.data(), content.data() + content.size(), &root, &jsonParseErrs)) {
        LOG_WARNING(sLogger, ("failed to parse journal checkpoint file during removal", checkpointFile)("error", jsonParseErrs));
        return false;
    }
    
    // 检查并删除checkpoint
    if (!root.isMember("checkpoints") || !root["checkpoints"].isMember(key)) {
        LOG_DEBUG(sLogger, ("journal checkpoint not found for removal", key));
        return true; // checkpoint不存在，认为删除成功
    }
    
    root["checkpoints"].removeMember(key);
    
    // 如果没有checkpoint了，删除文件
    if (root["checkpoints"].empty()) {
        if (remove(checkpointFile.c_str()) == 0) {
            LOG_DEBUG(sLogger, ("journal checkpoint file removed", checkpointFile));
        }
        return true;
    }
    
    // 写回文件
    std::ofstream fout(checkpointFile.c_str());
    if (!fout.is_open()) {
        LOG_ERROR(sLogger, ("failed to open journal checkpoint file for removal", checkpointFile));
        return false;
    }
    
    fout << root.toStyledString();
    fout.close();
    
    if (fout.bad()) {
        LOG_ERROR(sLogger, ("failed to write journal checkpoint file during removal", checkpointFile));
        return false;
    }
    
    LOG_DEBUG(sLogger, ("journal checkpoint removed", key));
    return true;
}

//=============================================================================
// 2. 核心业务操作层
//=============================================================================

void JournalCheckpointManager::SaveCheckpoint(const std::string& configName, size_t configIndex, const std::string& cursor) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    auto it = mCheckpoints.find(key);
    
    if (it != mCheckpoints.end()) {
        // 更新existing checkpoint
        auto& checkpoint = it->second;
        if (checkpoint->cursor != cursor) {
            checkpoint->cursor = cursor;
            checkpoint->updateTime = std::chrono::steady_clock::now();
            checkpoint->changed = true;
        }
    } else {
        // 创建新checkpoint
        auto checkpoint = std::make_shared<JournalCheckpoint>(cursor);
        checkpoint->changed = true;
        mCheckpoints[key] = checkpoint;
    }
}

std::string JournalCheckpointManager::GetCheckpoint(const std::string& configName, size_t configIndex) const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    auto it = mCheckpoints.find(key);
    
    if (it != mCheckpoints.end()) {
        return it->second->cursor;
    }
    
    return "";  // 如果不存在返回空字符串
}

bool JournalCheckpointManager::HasCheckpoint(const std::string& configName, size_t configIndex) const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    return mCheckpoints.find(key) != mCheckpoints.end();
}

void JournalCheckpointManager::ClearCheckpoint(const std::string& configName, size_t configIndex) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    auto it = mCheckpoints.find(key);
    
    if (it != mCheckpoints.end()) {
        // 从磁盘删除
        removeCheckpointFromDisk(key);
        // 从内存删除
        mCheckpoints.erase(it);
    }
}

bool JournalCheckpointManager::LoadCheckpointFromDisk(const std::string& configName, size_t configIndex) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    
    // 如果内存中已存在，跳过加载
    if (mCheckpoints.find(key) != mCheckpoints.end()) {
        return true;
    }
    
    JournalCheckpoint checkpoint;
    if (loadCheckpointFromDisk(key, checkpoint)) {
        mCheckpoints[key] = std::make_shared<JournalCheckpoint>(std::move(checkpoint));
        return true;
    }
    
    return false;
}

//=============================================================================
// 3. 批量管理层
//=============================================================================

size_t JournalCheckpointManager::FlushAllCheckpoints(bool forceAll) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    size_t flushedCount = 0;
    
    for (auto& pair : mCheckpoints) {
        const std::string& key = pair.first;
        auto& checkpoint = pair.second;
        
        if (forceAll || checkpoint->changed) {
            if (saveCheckpointToDisk(key, *checkpoint)) {
                checkpoint->changed = false;
                flushedCount++;
            }
        }
    }
    
    mLastFlushTime = std::chrono::steady_clock::now();
    
    if (flushedCount > 0) {
        LOG_INFO(sLogger, ("journal checkpoints flushed to disk", "")("count", flushedCount)("force_all", forceAll));
    }
    
    return flushedCount;
}

size_t JournalCheckpointManager::CleanupExpiredCheckpoints(int maxAgeHours) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto maxAge = std::chrono::hours(maxAgeHours);
    
    size_t cleanedCount = 0;
    auto it = mCheckpoints.begin();
    
    while (it != mCheckpoints.end()) {
        const std::string& key = it->first;
        const auto& checkpoint = it->second;
        
        if (now - checkpoint->updateTime > maxAge) {
            // 从磁盘删除
            removeCheckpointFromDisk(key);
            // 从内存删除
            it = mCheckpoints.erase(it);
            cleanedCount++;
        } else {
            ++it;
        }
    }
    
    if (cleanedCount > 0) {
        LOG_INFO(sLogger, ("journal checkpoints cleanup completed", "")("cleaned_count", cleanedCount)("max_age_hours", maxAgeHours));
    }
    
    return cleanedCount;
}

size_t JournalCheckpointManager::ClearConfigCheckpoints(const std::string& configName) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    size_t cleanedCount = 0;
    auto it = mCheckpoints.begin();
    
    std::string configPrefix = configName + "_";
    
    while (it != mCheckpoints.end()) {
        const std::string& key = it->first;
        
        if (key.substr(0, configPrefix.length()) == configPrefix) {
            // 从磁盘删除
            removeCheckpointFromDisk(key);
            // 从内存删除
            it = mCheckpoints.erase(it);
            cleanedCount++;
        } else {
            ++it;
        }
    }
    
    if (cleanedCount > 0) {
        LOG_INFO(sLogger, ("journal config checkpoints cleared", "")("config", configName)("cleared_count", cleanedCount));
    }
    
    return cleanedCount;
}

//=============================================================================
// 4. 状态监控层
//=============================================================================

size_t JournalCheckpointManager::GetCheckpointCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCheckpoints.size();
}

size_t JournalCheckpointManager::GetChangedCheckpointCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    return std::count_if(mCheckpoints.begin(), mCheckpoints.end(),
                        [](const auto& pair) { return pair.second->changed; });
}

} // namespace logtail 