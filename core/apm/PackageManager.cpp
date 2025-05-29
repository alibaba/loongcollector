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

#include "apm/PackageManager.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include "common/http/Curl.h"
#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"
#include "logger/Logger.h"
#include "magic_enum.hpp"

namespace logtail::apm {

const char* kDefaultExecHookDir = "/opt/.arms/lib/exec-hook";
const char* kDefaultExecHookName = "libexec-hook.so";
const char* kDefaultSoFullPath = "/opt/.arms/lib/exec-hook/libexec-hook.so";
const char* kDefaultPreloadConfigFile = "/etc/ld.so.preload";

bool initSharedLibraryDir() {
    if (!std::filesystem::exists(kDefaultExecHookDir)) {
        if (!std::filesystem::create_directories(kDefaultExecHookDir)) {
            LOG_ERROR(sLogger, ("Failed to create exec-hook library dir", ""));
            return false;
        }
    }
}

std::string getWorkingPath() {
    return "";
}

// write to /etc/ld.so.preload
bool PackageManager::InstallExecHook() {
    // step1. prepare shared lib dir ...
    bool res = initSharedLibraryDir();
    if (!res) {
        // send alarm
        return false;
    }

    // TODO step2. download ...

    auto workingPath = getWorkingPath();
    auto execHookEntry = workingPath.append("/").append(kDefaultExecHookName);

    // step3. write to /etc/ld.so.preload
    if (!std::filesystem::exists(kDefaultPreloadConfigFile)) {
        std::ofstream configFile(kDefaultPreloadConfigFile);
        if (!configFile) {
            throw std::runtime_error("Failed to create ld preload config file");
        }
        configFile << execHookEntry;
        configFile.close();
    } else {
        std::ifstream inFile(kDefaultPreloadConfigFile);
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        std::string content = buffer.str();
        inFile.close();

        if (content.find(execHookEntry) == std::string::npos) {
            // write into file ...
            std::ofstream outFile(kDefaultPreloadConfigFile);
            if (!outFile) {
                throw std::runtime_error("Failed to update ld preload config file");
            }
            outFile << execHookEntry << content;
            outFile.close();
        }
    }

    return true;
}

bool PackageManager::downloadFromOss(const std::string& url, const std::string& output, bool& changed) {
    std::unique_ptr<HttpRequest> request;

    FILE* file = fopen(output.c_str(), "wb");
    HttpResponse res((void*)file,
                     [](void* pf) {
                         FILE* file = static_cast<FILE*>(pf);
                         fclose(file);
                     },
                     [](char* ptr, size_t size, size_t nmemb, void* stream) {
                         return fwrite((void*)ptr, size, nmemb, (FILE*)stream);
                     });

    request = std::make_unique<HttpRequest>(
        "GET", true, url, 443, "", "", std::map<std::string, std::string>(), "", 10, 3, true);
    bool success = SendHttpRequest(std::move(request), res);
    if (!success) {
        return false;
    }

    // get signature and md5
    // or manifest
    // auto headers = res.GetHeader();

    if (res.GetStatusCode() < 200 || res.GetStatusCode() >= 300) {
        return false;
    }

    return true;
}

bool PackageManager::PrepareAPMAgent(APMLanguage lang, const std::string& version, std::string& agentJarPath) {
    if (lang != APMLanguage::kJava) {
        LOG_WARNING(sLogger, ("language not supported", magic_enum::enum_name(lang)));
        return false;
    }

    // download from oss ...
    return true;
}

} // namespace logtail::apm
