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
#include <regex>
#include <tuple>

#include "common/http/Curl.h"
#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"
#include "logger/Logger.h"
#include "magic_enum.hpp"

namespace logtail::apm {

namespace fs = std::filesystem;

const char* kDefaultExecHookDir = "/opt/.arms/lib/exec-hook";
const char* kDefaultExecHookName = "libexec-hook.so";
const char* kDefaultSoFullPath = "/opt/.arms/lib/exec-hook/libexec-hook.so";
const char* kDefaultPreloadConfigFile = "/etc/ld.so.preload";

// Constants
const std::string kDefaultArmsOssPublicEndpointPattern = "https://arms-apm-${REGION_ID}.oss-${REGION_ID}.aliyuncs.com";
const std::string kDefaultArmsOssInternalEndpointPattern = "https://arms-apm-${REGION_ID}.oss-${REGION_ID}-internal.aliyuncs.com";
const std::string kManifestExt = ".manifest";
const char kHiddenFilePrefix = '.';
const int kDefaultDownloadTimeout = 30; // seconds

std::regex pattern(R"(Url: (.+)\nEtag: (.+)\nLastModified: (.+))");

// Read manifest file
bool ReadManifest(const std::string& dir, const std::string& filename, std::string& lastUrl, std::string& lastEtag, std::string& lastModified) {
    std::string manifestFile = std::string(1, kHiddenFilePrefix) + filename + kManifestExt;
    fs::path manifestPath = fs::path(dir) / manifestFile;
    if (!fs::exists(manifestPath) || !fs::is_regular_file(manifestPath)) {
        return false;
    }

    std::ifstream file(manifestPath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    std::regex pattern(R"(Url: (.+)\nEtag: (.+)\nLastModified: (.+))");
    std::smatch matches;
    if (std::regex_search(content, matches, pattern) && matches.size() == 4) {
        lastUrl = matches[1].str();
        lastEtag = matches[2].str();
        lastModified = matches[3].str();
        return true;
    }
    return false;
}

// Write manifest file
int DumpManifest(const std::string& dir, const std::string& filename, const std::string& url, const std::string& etag, const std::string& lastModified) {
    std::string manifestFile = std::string(1, kHiddenFilePrefix) + filename + kManifestExt;
    fs::path manifestPath = fs::path(dir) / manifestFile;
    LOG_INFO(sLogger, ("dir", dir)("fileName", filename)("url", url)("etag", etag)("lastModified", lastModified));
    std::ofstream out(manifestPath);
    if (!out) {
        LOG_WARNING(sLogger, ("failed to dump manifest to file", manifestPath));
        return -1;
    }
    out << "Url: " << url << "\n";
    out << "Etag: " << etag << "\n";
    out << "LastModified: " << lastModified << "\n";
    out.close();
    
    return 0;
}

// Generate manifest filename
std::string GetManifestFile(const std::string& filename) {
    fs::path p(filename);
    std::string base = p.stem().string();
    return std::string(1, kHiddenFilePrefix) + base + kManifestExt;
}

// Get endpoint
std::string GetArmsOssDefaultEndpoint(const std::string& regionId, bool vpc) {
    if (regionId == "cn-hangzhou-finance") {
        return "https://arms-apm-cn-hangzhou-finance.oss-cn-hzjbp-b-internal.aliyuncs.com";
    }
    const std::string& pattern = vpc ? kDefaultArmsOssInternalEndpointPattern : kDefaultArmsOssPublicEndpointPattern;
    std::string endpoint = pattern;
    size_t pos = 0;
    while ((pos = endpoint.find("${REGION_ID}", pos)) != std::string::npos) {
        endpoint.replace(pos, 11, regionId);
        pos += regionId.size();
    }
    return endpoint;
}

bool InitSharedLibraryDir() {
    if (!std::filesystem::exists(kDefaultExecHookDir)) {
        if (!std::filesystem::create_directories(kDefaultExecHookDir)) {
            LOG_ERROR(sLogger, ("Failed to create exec-hook library dir", ""));
            return false;
        }
    }
}

std::string GetWorkingPath() {
    return "";
}

// write to /etc/ld.so.preload
bool PackageManager::InstallExecHook() {
    // step1. prepare shared lib dir ...
    bool res = InitSharedLibraryDir();
    if (!res) {
        // send alarm
        return false;
    }

    // TODO step2. download ...

    auto workingPath = GetWorkingPath();
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

bool PackageManager::downloadFromOss(const std::string& url, const std::string& dir, const std::string& filename, bool& changed) {
    std::string lastUrl, lastEtag, lastModified;
    bool res = ReadManifest(dir, filename, lastUrl, lastEtag, lastModified);
    LOG_DEBUG(sLogger, ("ReadManifest res", res)("dir", dir)("fileName", filename)("url", lastUrl)("etag", lastEtag)("lastModified", lastModified));

    std::unique_ptr<HttpRequest> request;

    // FILE* file = fopen(output.c_str(), "wb");
    // HttpResponse res((void*)file,
    //                  [](void* pf) {
    //                      FILE* file = static_cast<FILE*>(pf);
    //                      fclose(file);
    //                  },
    //                  [](char* ptr, size_t size, size_t nmemb, void* stream) {
    //                      return fwrite((void*)ptr, size, nmemb, (FILE*)stream);
    //                  });

    // request = std::make_unique<HttpRequest>(
    //     "GET", true, url, 443, "", "", std::map<std::string, std::string>(), "", 10, 3, true);
    // bool success = SendHttpRequest(std::move(request), res);
    // if (!success) {
    //     return false;
    // }

    // // get signature and md5
    // // or manifest
    // // auto headers = res.GetHeader();

    // if (res.GetStatusCode() < 200 || res.GetStatusCode() >= 300) {
    //     return false;
    // }

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
