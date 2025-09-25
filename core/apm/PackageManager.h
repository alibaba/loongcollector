/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <chrono>
#include <filesystem>

#include "apm/Types.h"

namespace logtail::apm {

class PackageManager {
public:
    PackageManager(const PackageManager&) = delete;
    PackageManager(PackageManager&&) = delete;
    PackageManager& operator=(const PackageManager&) = delete;
    PackageManager& operator=(PackageManager&&) = delete;

    explicit PackageManager() = default;

    virtual ~PackageManager() = default;

    void Init() {}

    // 更新exec hook（强制重新下载并覆盖）
    bool UpdateExecHook(const std::string& regionId);
    // 卸载exec hook（删除系统和本地文件）
    bool UninstallExecHook();

    std::string GetApmAgentDownloadUrl(APMLanguage lang, const std::string& region, const std::string& version);

    // download and un-zip to target path ...
    bool PrepareAPMAgent(APMLanguage lang,
                         const std::string& pid,
                         const std::string& region,
                         const std::string& version,
                         std::filesystem::path& outBootstrapPath);

    bool InstallExecHook(const std::string& region);

    bool PrepareExecHook(const std::string& region);

    // 只删除指定appId/agentVersion的包，不影响其他配置
    bool RemoveAPMAgent(APMLanguage lang, const std::string& appId);

private:
    bool download(const std::string& url, const std::string& dir, const std::string& filename, bool& changed);
    bool downloadWithRetry(const std::string& url, const std::string& dir, const std::string& filename, bool& changed);
    bool verifyDownloadedFile(const std::filesystem::path& filePath);
    // bool backupExistingFile(const std::filesystem::path& filePath);
    // bool restoreBackupFile(const std::filesystem::path& filePath);

    // 重试相关配置
    static constexpr int kMaxRetries = 3;
    static constexpr std::chrono::seconds kRetryDelay{5};
    static constexpr int kDefaultDownloadTimeout = 30; // seconds

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PackageManagerUnittest;
#endif
};

} // namespace logtail::apm
