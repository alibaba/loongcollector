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
#include "common/StringTools.h"
#include "common/ArchiveHelper.h"

namespace logtail::apm {

namespace fs = std::filesystem;

const fs::path kSysSharedLibraryPath = "/lib64";
const fs::path kDefaultExecHookDownloadDir = "/opt/.arms/lib/exec-hook";
const char* kDefaultExecHookName = "libexec-hook.so";
const fs::path kDefaultPreloadConfigFile = "/etc/ld.so.preload";
const fs::path kDefaultJavaAgentDir = "/opt/.arms/apm-java-agent";

const fs::path kLatestAgentSubpath = "latest";
const fs::path kCurrentAgentSubpath = "current";

const fs::path kJavaAgentSubpath = "AliyunJavaAgent";
const fs::path kJavaAgentBootstrapFile = "aliyun-java-agent.jar";

// Constants
const std::string kDefaultArmsOssPublicEndpointPattern = "arms-apm-${REGION_ID}.oss-${REGION_ID}.aliyuncs.com";
const std::string kDefaultArmsOssInternalEndpointPattern = "arms-apm-${REGION_ID}.oss-${REGION_ID}-internal.aliyuncs.com";
const std::string kArmsJavaAgentFilename = "AliyunJavaAgent.zip";
const std::string kArmsPythonAgentFilename = "aliyun-python-agent.tar.gz";
const std::string kManifestExt = ".manifest";
const char kHiddenFilePrefix = '.';
const int kDefaultDownloadTimeout = 30; // seconds
const std::string kPlaceHolderRegionId = "${REGION_ID}";

std::regex gPattern(R"(Url: (.+)\nEtag: (.+)\nLastModified: (.+))");

// Generate manifest filename
std::string GetManifestFile(const std::string& filename) {
    return kHiddenFilePrefix + filename + kManifestExt;
}

// Read manifest file
bool ReadManifest(const std::string& dir, const std::string& filename, std::string& lastUrl, std::string& lastEtag, std::string& lastModified) {
    std::string manifestFile = GetManifestFile(filename);
    fs::path manifestPath = fs::path(dir) / manifestFile;
    if (!fs::exists(manifestPath) || !fs::is_regular_file(manifestPath)) {
        return false;
    }

    std::ifstream file(manifestPath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    std::smatch matches;
    if (std::regex_search(content, matches, gPattern) && matches.size() == 4) {
        lastUrl = matches[1].str();
        lastEtag = matches[2].str();
        lastModified = matches[3].str();
        return true;
    }
    return false;
}

// Write manifest file
int DumpManifest(const std::string& dir, const std::string& filename, const std::string& url, const std::string& etag, const std::string& lastModified) {
    std::string manifestFile = GetManifestFile(filename);
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

// Get endpoint
std::string GetArmsOssDefaultEndpoint(const std::string& regionId, bool vpc) {
    if (regionId == "cn-hangzhou-finance") {
        return "arms-apm-cn-hangzhou-finance.oss-cn-hzjbp-b-internal.aliyuncs.com";
    }
    const std::string& pattern = vpc ? kDefaultArmsOssInternalEndpointPattern : kDefaultArmsOssPublicEndpointPattern;
    std::string endpoint = pattern;
    ReplaceString(endpoint, kPlaceHolderRegionId, regionId);
    return endpoint;
}

bool InitSharedLibraryDir() {
    if (!std::filesystem::exists(kDefaultExecHookDownloadDir)) {
        if (!std::filesystem::create_directories(kDefaultExecHookDownloadDir)) {
            LOG_ERROR(sLogger, ("Failed to create exec-hook library dir", ""));
            return false;
        }
    }
    return true;
}

bool InitDir(const fs::path& dir) {
    if (!std::filesystem::exists(dir)) {
        if (!std::filesystem::create_directories(dir)) {
            LOG_ERROR(sLogger, ("Failed to create dir", dir));
            return false;
        }
    }
    return true;
}

bool PackageManager::PrepareExecHook(const std::string& region) {
    // step1. prepare shared lib dir ...
    bool res = InitDir(kDefaultExecHookDownloadDir);
    if (!res) {
        // send alarm
        LOG_WARNING(sLogger, ("failed to init shared library dir", ""));
        return false;
    }

    // step2. download ...
    std::string url = GetArmsOssDefaultEndpoint(region, false) + "/" + kDefaultExecHookName;
    LOG_DEBUG(sLogger, ("url", url));
    bool changed = false;
    bool status = downloadFromOss(url, kDefaultExecHookDownloadDir, kDefaultExecHookName, changed);
    if (!status) {
        LOG_WARNING(sLogger, ("failed to download from oss", kDefaultExecHookName));
        return false;
    }
    
    if (changed || !fs::exists(kSysSharedLibraryPath / kDefaultExecHookName)) {
        // 步骤1: 设置源文件权限为 644 (owner: read/write, group/other: read)
        fs::permissions(kDefaultExecHookDownloadDir / kDefaultExecHookName, 
            fs::perms::owner_read | fs::perms::owner_write |
            fs::perms::group_read | fs::perms::others_read,
            fs::perm_options::replace);

        // 步骤2: 删除目标路径（文件或目录）
        if (fs::exists(kSysSharedLibraryPath / kDefaultExecHookName)) {
            fs::remove(kSysSharedLibraryPath / kDefaultExecHookName);
        }

        // 确保目标目录存在
        if (!fs::exists(kSysSharedLibraryPath)) {
            fs::create_directories(kSysSharedLibraryPath);
        }

        // 步骤3: 复制文件到工作路径
        fs::copy(kDefaultExecHookDownloadDir / kDefaultExecHookName, kSysSharedLibraryPath / kDefaultExecHookName, fs::copy_options::overwrite_existing);
    }
    LOG_INFO(sLogger, ("exec hook is ready", ""));
    return true;
}

// write to /etc/ld.so.preload
bool PackageManager::InstallExecHook(const std::string& region) {
    bool status = PrepareExecHook(region);
    if (!status) {
        LOG_WARNING(sLogger, ("failed to prepare exec hook", ""));
        return false;
    }

    std::string execHookEntry = kSysSharedLibraryPath / kDefaultExecHookName;
    execHookEntry += "\n";

    // step3. write to /etc/ld.so.preload
    if (!std::filesystem::exists(kDefaultPreloadConfigFile)) {
        std::ofstream configFile(kDefaultPreloadConfigFile);
        if (!configFile) {
            LOG_WARNING(sLogger, ("Failed to create ld preload config file", ""));
            return false;
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
                LOG_WARNING(sLogger, ("Failed to update ld preload config file", ""));
                return false;
            }
            outFile << execHookEntry << content;
            outFile.close();
        }
    }

    // TODO @qianlu.kk do we need to refresh ld cache??
    // int result = std::system("ldconfig");
    // if (result != 0) {
    //     LOG_WARNING(sLogger, ("Failed to run ldconfig.", ""));
    //     return false;
    // }

    return true;
}

int ClearDirectory(const fs::path& dirPath) {
    if (!fs::exists(dirPath)) {
        return 0;
    }
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (fs::is_regular_file(entry)) {
                fs::remove(entry.path());
            } else if (fs::is_directory(entry)) {
                fs::remove_all(entry.path());
            }
        }
        return 0;
    } catch (const fs::filesystem_error& e) {
        LOG_WARNING(sLogger, ("Failed to clear directory: ", std::string(e.what())));
        return 1;
    }
}

bool PackageManager::downloadFromOss(const std::string& url, const std::string& dir, const std::string& filename, bool& changed) {
    changed = false;
    std::string lastUrl;
    std::string lastEtag;
    std::string lastModified;
    bool res = ReadManifest(dir, filename, lastUrl, lastEtag, lastModified);
    LOG_DEBUG(sLogger, ("ReadManifest res", res)("dir", dir)("fileName", filename)("url", lastUrl)("etag", lastEtag)("lastModified", lastModified));
    
    // url changed ...
    if (url != lastUrl) {
        // std::string manifestFile = GetManifestFile(filename);
        int ret = ClearDirectory(dir);
        if (ret) {
            LOG_ERROR(sLogger, ("Failed to clean dir: ", dir));
            // do we need send alarm ???
        }
    }

    fs::path savedPath = fs::path(dir) / filename;

    std::unique_ptr<HttpRequest> request;

    FILE* file = fopen(savedPath.c_str(), "wb");
    if (file == nullptr) {
        LOG_WARNING(sLogger, ("cannot open file", savedPath));
        return false;
    }
    HttpResponse response((void*)file,
                     [](void* pf) {
                         FILE* file = static_cast<FILE*>(pf);
                         fclose(file);
                     },
                     [](char* ptr, size_t size, size_t nmemb, void* stream) {
                         return fwrite((void*)ptr, size, nmemb, (FILE*)stream);
                     });

    std::map<std::string, std::string> headers;
    if (lastEtag.size()) {
        headers.insert({"If-None-Match", lastEtag});
    }
    if (lastModified.size()) {
        headers.insert({"If-Modified-Since", lastModified});
    }
    request = std::make_unique<HttpRequest>(
        "GET", true, url, 443, "", "", headers, "", 10, 3, true);
    bool success = SendHttpRequest(std::move(request), response);
    if (!success) {
        return false;
    }

    // get signature and md5
    // or manifest
    // auto headers = res.GetHeader();
    if (response.GetStatusCode() == 304) {
        LOG_DEBUG(sLogger, ("not modified", ""));
        return true;
    }

    if (response.GetStatusCode() == 200) {
        // dump manifest
        std::string etag;
        std::string lastModified;
        auto it = response.GetHeader().find("Etag");
        if (it != response.GetHeader().end()) {
            // not found 
            etag = it->second;
        }
        
        it = response.GetHeader().find("Last-Modified");
        if (it != response.GetHeader().end()) {
            // not found
            lastModified = it->second;
        }
        lastModified = it->second;
        changed = true;
        return DumpManifest(dir, filename, url, etag, lastModified) == 0;
    }

    LOG_WARNING(sLogger, ("failed to download, statusCode", response.GetStatusCode())("filename", filename)("url", url));

    return false;
}

std::string PackageManager::GetApmAgentDownloadUrl(APMLanguage lang, const std::string& region, const std::string& version) {
    auto endpoint = GetArmsOssDefaultEndpoint(region, false);

    if (version.size() && version != "latest") {
        endpoint += '/';
        endpoint += version;
    }

    endpoint += '/';
    switch (lang)
    {
    case APMLanguage::kJava:
        endpoint += kArmsJavaAgentFilename;
        break;
    case APMLanguage::kPython:
        endpoint += kArmsPythonAgentFilename;
        break;
    default:
        break;
    }
    return endpoint;

}

bool PackageManager::PrepareAPMAgent(APMLanguage lang, const std::string& appId, const std::string& region, const std::string& version, fs::path& outBootstrapPath) {
    if (lang != APMLanguage::kJava) {
        LOG_WARNING(sLogger, ("language not supported", magic_enum::enum_name(lang)));
        return false;
    }

    bool status = InitDir(kDefaultJavaAgentDir / appId);
    if (!status) {
        LOG_WARNING(sLogger, ("failed to init apm agent dir", ""));
        return false;
    }

    status = InitDir(kDefaultJavaAgentDir / appId / kLatestAgentSubpath);
    if (!status) {
        LOG_WARNING(sLogger, ("failed to init apm agent current dir", ""));
        return false;
    }

    status = InitDir(kDefaultJavaAgentDir / appId / kCurrentAgentSubpath);
    if (!status) {
        LOG_WARNING(sLogger, ("failed to init apm agent current dir", ""));
        return false;
    }

    // download from oss ...
    auto url = GetApmAgentDownloadUrl(lang, region, version);
    bool changed = false;
    status = downloadFromOss(url, kDefaultJavaAgentDir / appId / kLatestAgentSubpath, kArmsJavaAgentFilename, changed);
    if (!status) {
        LOG_WARNING(sLogger, ("failed to download from oss", kArmsJavaAgentFilename));
        return false;
    }

    if (changed || !fs::exists(kDefaultJavaAgentDir / appId / kLatestAgentSubpath / kJavaAgentSubpath/ kJavaAgentBootstrapFile)) {
        // unzip
        ArchiveHelper helper(kDefaultJavaAgentDir / appId / kLatestAgentSubpath / kArmsJavaAgentFilename, kDefaultJavaAgentDir / appId / kLatestAgentSubpath);
        status = helper.Extract();
        if (!status) {
            LOG_WARNING(sLogger, ("failed to unzip agent from oss", kArmsJavaAgentFilename));
            return false;
        }

        // clean current dir 
        int ret = ClearDirectory(kDefaultJavaAgentDir / appId / kCurrentAgentSubpath);
        if (ret) {
            LOG_WARNING(sLogger, ("Failed to clear current dir, ret", ret));
        }

        // copy latest to current dir ...
        try {
            fs::copy(kDefaultJavaAgentDir / appId / kLatestAgentSubpath, kDefaultJavaAgentDir / appId / kCurrentAgentSubpath, fs::copy_options::recursive);
        } catch (const fs::filesystem_error& e) {
            LOG_WARNING(sLogger, ("failed to copy latest dir to current dir, e", e.what()));
            return false;
        }
    }

    outBootstrapPath = kDefaultJavaAgentDir / appId / kCurrentAgentSubpath / kJavaAgentSubpath / kJavaAgentBootstrapFile;

    return true;
}

} // namespace logtail::apm
