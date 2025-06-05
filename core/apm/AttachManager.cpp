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

#include <filesystem>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>

#include "apm/AttachManager.h"
#include "logger/Logger.h"
#include "common/StringTools.h"

namespace logtail::apm {

namespace fs = std::filesystem;

const fs::path kRuntimeConfigFile = ".arms.rc";
const fs::path kRuntimePidFile = ".arms.pid";

const std::string kPlaceholderLicenseKey = "${licenseKey}";
const std::string kPlaceholderAgentPath = "${path_to_agent_bootstrap}";
const std::string kPlaceholderAppName = "${appName}";

const std::string kRuntimeConfigTemplate = R"(
-javaagent:${path_to_agent_bootstrap}
-Darms.licenseKey=${licenseKey}
-Darms.appName=${appName}
-Darms.agent.env=ECS_AUTO
)";

std::string BuildRuntimeConfig(const std::string& agentJarPath, 
                               const std::string& licenseKey, 
                               const std::string& appName) {
    std::string content = kRuntimeConfigTemplate;
    ReplaceString(content, kPlaceholderAgentPath, agentJarPath);
    ReplaceString(content, kPlaceholderLicenseKey, licenseKey);
    ReplaceString(content, kPlaceholderAppName, appName);
    return content;
}

int AttachManager::prepareRuntimeConfig(const fs::path& cwd, 
                           const std::string& agentJarPath,
                           const std::string& licenseKey,
                           const std::string& appName) {
    auto configPath = cwd / kRuntimeConfigFile;
    
    std::string newContent = BuildRuntimeConfig(agentJarPath, licenseKey, appName);
    
    if (!fs::exists(configPath)) {
        std::ofstream outFile(configPath, std::ios::out | std::ios::trunc);
        if (!outFile) {
            LOG_WARNING(sLogger, ("failed to create config file", configPath));
            return 1;
        }
        outFile << newContent;
        LOG_INFO(sLogger, ("runtime config file created for cwd", cwd)("configPath", configPath));
    } else {
        std::string oldContent;
        std::ifstream inFile(configPath);
        if (!inFile) {
            LOG_WARNING(sLogger, ("Failed to read config file", configPath));
            return 1;
        }
        std::ostringstream buffer;
        buffer << inFile.rdbuf();
        oldContent = buffer.str();
        
        if (newContent != oldContent) {
            std::ofstream outFile(configPath, std::ios::out | std::ios::trunc);
            if (!outFile) {
                LOG_WARNING(sLogger, ("Failed to update config file", configPath));
                return 1;
            }
            outFile << newContent;
            LOG_INFO(sLogger, ("runtime config file updated for cwd", cwd)("configPath", configPath));
        }
    }
    return 0;
}

int RemoveRuntimeConfig(const fs::path& cwd) {
    auto configPath = cwd / kRuntimeConfigFile;
    if (fs::exists(configPath) && !fs::remove(configPath)) {
        LOG_WARNING(sLogger, ("Failed to remove config file", configPath));
        return 1;
    }
    return 0;
}

bool CheckRuntimeConfig(const fs::path& cwd) {
    return fs::exists(cwd / kRuntimeConfigFile);
}

bool CheckProcessInjected(const fs::path& cwd, int pid) {
    auto pidFilePath = cwd / kRuntimePidFile;
    if (!fs::exists(pidFilePath)) {
        return false;
    }

    std::string pidStr = ToString<int>(pid);
    std::ifstream inFile(pidFilePath);
    if (!inFile) {
        LOG_WARNING(sLogger, ("failed to read PID file for pid", pid));
        return false;
    }
    
    std::ostringstream buffer;
    buffer << inFile.rdbuf();
    std::string content = buffer.str();
    
    return content.find(pidStr) != std::string::npos;
}

bool AttachManager::DoAttach(MatchRule& rule, const std::string& agentPath, AttachConfig& config) {
    return prepareRuntimeConfig(rule.mVal, agentPath, config.mLicenseKey, config.mAppName) == 0;
}

}
