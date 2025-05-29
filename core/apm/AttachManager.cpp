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

namespace logtail::apm {

namespace fs = std::filesystem;

const std::string kRuntimeConfigFile = ".arms.rc";
const std::string kRuntimePidFile = ".arms.pid";
const std::string kRuntimeConfigTemplate = R"(
-javaagent:${path_to_agent_bootstrap}
-Darms.licenseKey=${licenseKey}
-Darms.appName=${appName}
-Darms.agent.env=ECS_AUTO
)";


std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != std::string::npos) {
        str.replace(startPos, from.length(), to);
        startPos += to.length();
    }
    return str;
}

std::string BuildRuntimeConfig(const std::string& agentJarPath, 
                               const std::string& licenseKey, 
                               const std::string& appName) {
    std::string content = kRuntimeConfigTemplate;
    content = ReplaceAll(content, "${path_to_agent_bootstrap}", agentJarPath);
    content = ReplaceAll(content, "${licenseKey}", licenseKey);
    content = ReplaceAll(content, "${appName}", appName);
    return content;
}

int PrepareRuntimeConfig(const fs::path& cwd, 
                           const std::string& agentJarPath,
                           const std::string& licenseKey,
                           const std::string& appName,
                           int pid) {
    auto configPath = cwd / kRuntimeConfigFile;
    
    std::string newContent = BuildRuntimeConfig(agentJarPath, licenseKey, appName);
    
    if (!fs::exists(configPath)) {
        std::ofstream outFile(configPath, std::ios::out | std::ios::trunc);
        if (!outFile) {
            LOG_WARNING(sLogger, ("failed to create config file", configPath));
            return 1;
        }
        outFile << newContent;
        LOG_INFO(sLogger, ("runtime config file created for pid", pid)("configPath", configPath));
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
            LOG_INFO(sLogger, ("runtime config file updated for pid", pid)("configPath", configPath));
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

    std::string pidStr = std::to_string(pid);
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

bool AttachManager::DoAttach(MatchRule& rule, const std::string& agentPath, AttachConfig& config, int pid) {
    return PrepareRuntimeConfig(rule.mVal, agentPath, config.mLicenseKey, config.mAppName, pid) == 0;
}

}
