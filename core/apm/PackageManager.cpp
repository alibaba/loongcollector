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
#include <regex>
#include <string>
#include <thread>
#include <tuple>

#include "common/ArchiveHelper.h"
#include "common/StringTools.h"
#include "common/http/Curl.h"
#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"
#include "logger/Logger.h"
#include "magic_enum.hpp"

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
const std::string kDefaultArmsOssInternalEndpointPattern
    = "arms-apm-${REGION_ID}.oss-${REGION_ID}-internal.aliyuncs.com";
const std::string kArmsJavaAgentFilename = "AliyunJavaAgent.zip";
const std::string kArmsPythonAgentFilename = "aliyun-python-agent.tar.gz";
const std::string kManifestExt = ".manifest";
const char kHiddenFilePrefix = '.';
const int kDefaultDownloadTimeout = 30; // seconds
const std::string kPlaceHolderRegionId = "${REGION_ID}";
const int kMaxRetries = 3;
const std::chrono::seconds kRetryDelay(1);

std::regex gPattern(R"(Url: (.+)\nEtag: (.+)\nLastModified: (.+))");

// Generate manifest filename
std::string GetManifestFile(const std::string& filename) {
    return kHiddenFilePrefix + filename + kManifestExt;
}

// Read manifest file
bool ReadManifest(const std::string& dir,
                  const std::string& filename,
                  std::string& lastUrl,
                  std::string& lastEtag,
                  std::string& lastModified) {
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
int DumpManifest(const std::string& dir,
                 const std::string& filename,
                 const std::string& url,
                 const std::string& etag,
                 const std::string& lastModified) {
    std::string manifestFile = GetManifestFile(filename);
    fs::path manifestPath = fs::path(dir) / manifestFile;
    if (fs::exists(manifestPath)) {
        fs::remove(manifestPath);
    }
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
std::string GetApmOssDefaultEndpoint(const std::string& regionId, bool vpc) {
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

bool PackageManager::downloadWithRetry(const std::string& url,
                                       const std::string& dir,
                                       const std::string& filename,
                                       bool& changed) {
    for (int retry = 0; retry < kMaxRetries; ++retry) {
        if (retry > 0) {
            LOG_INFO(sLogger, ("retrying download", retry)("url", url));
            std::this_thread::sleep_for(kRetryDelay);
        }

        if (download(url, dir, filename, changed)) {
            // TODO @qianlu.kk

            // 验证下载的文件
            fs::path filePath = fs::path(dir) / filename;
            if (verifyDownloadedFile(filePath)) {
                return true;
            }
            LOG_WARNING(sLogger, ("downloaded file verification failed", filePath));
        }
    }

    LOG_ERROR(sLogger, ("failed to download after retries", url));
    return false;
}

bool PackageManager::verifyDownloadedFile(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        LOG_WARNING(sLogger, ("file does not exist", filePath));
        return false;
    }

    // 检查文件大小
    auto fileSize = fs::file_size(filePath);
    if (fileSize == 0) {
        LOG_WARNING(sLogger, ("file is empty", filePath));
        return false;
    }

    // TODO: 可以添加更多的验证，比如文件完整性检查

    return true;
}

// bool PackageManager::backupExistingFile(const fs::path& filePath) {
//     if (!fs::exists(filePath)) {
//         return true;
//     }

//     auto backupPath = filePath.string() + ".bak";
//     try {
//         fs::copy_file(filePath, backupPath, fs::copy_options::overwrite_existing);
//         return true;
//     } catch (const fs::filesystem_error& e) {
//         LOG_WARNING(sLogger, ("failed to backup file", e.what()));
//         return false;
//     }
// }

// bool PackageManager::restoreBackupFile(const fs::path& filePath) {
//     auto backupPath = filePath.string() + ".bak";
//     if (!fs::exists(backupPath)) {
//         return true;
//     }

//     try {
//         fs::copy_file(backupPath, filePath, fs::copy_options::overwrite_existing);
//         fs::remove(backupPath);
//         return true;
//     } catch (const fs::filesystem_error& e) {
//         LOG_WARNING(sLogger, ("failed to restore backup file", e.what()));
//         return false;
//     }
// }

bool PackageManager::PrepareExecHook(const std::string& region) {
    // 准备共享库目录
    bool res = InitDir(kDefaultExecHookDownloadDir);
    if (!res) {
        LOG_WARNING(sLogger, ("failed to init shared library dir", ""));
        return false;
    }

    // 备份现有文件
    auto hookPath = kDefaultExecHookDownloadDir / kDefaultExecHookName;
    // if (!backupExistingFile(hookPath)) {
    //     LOG_WARNING(sLogger, ("failed to backup existing exec hook file", ""));
    // }

    // 下载 exec hook
    std::string url = GetApmOssDefaultEndpoint(region, false) + "/" + kDefaultExecHookName;
    LOG_DEBUG(sLogger, ("url", url));
    bool changed = false;
    bool status = downloadWithRetry(url, kDefaultExecHookDownloadDir, kDefaultExecHookName, changed);
    if (!status) {
        LOG_WARNING(sLogger, ("failed to download from oss", kDefaultExecHookName));
        // 恢复备份
        // restoreBackupFile(hookPath);
        return false;
    }

    if (changed || !fs::exists(kSysSharedLibraryPath / kDefaultExecHookName)) {
        // 设置文件权限
        fs::permissions(kDefaultExecHookDownloadDir / kDefaultExecHookName,
                        fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read,
                        fs::perm_options::replace);

        // 删除目标文件
        if (fs::exists(kSysSharedLibraryPath / kDefaultExecHookName)) {
            fs::remove(kSysSharedLibraryPath / kDefaultExecHookName);
        }

        // 确保目标目录存在
        if (!fs::exists(kSysSharedLibraryPath)) {
            fs::create_directories(kSysSharedLibraryPath);
        }

        // 复制文件到系统目录
        try {
            fs::copy(kDefaultExecHookDownloadDir / kDefaultExecHookName,
                     kSysSharedLibraryPath / kDefaultExecHookName,
                     fs::copy_options::overwrite_existing);
        } catch (const fs::filesystem_error& e) {
            LOG_WARNING(sLogger, ("failed to copy exec hook to system dir", e.what()));
            // 恢复备份
            // restoreBackupFile(hookPath);
            return false;
        }
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

bool PackageManager::download(const std::string& url,
                              const std::string& dir,
                              const std::string& filenameOrigin,
                              bool& changed) {
    changed = false;
    std::string lastUrl;
    std::string lastEtag;
    std::string lastModified;
    std::string filename = filenameOrigin + ".download";
    // store ...
    if (fs::exists(fs::path(dir) / filenameOrigin)) {
        bool res = ReadManifest(dir, filenameOrigin, lastUrl, lastEtag, lastModified);
        LOG_DEBUG(sLogger,
                  ("ReadManifest res", res)("dir", dir)("fileName", filename)("url", lastUrl)("etag", lastEtag)(
                      "lastModified", lastModified));
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
    if (url == lastUrl && fs::exists(savedPath)) {
        if (lastEtag.size()) {
            headers.insert({"If-None-Match", lastEtag});
        }
        if (lastModified.size()) {
            headers.insert({"If-Modified-Since", lastModified});
        }
    }

    request = std::make_unique<HttpRequest>("GET", true, url, 443, "", "", headers, "", 10, 3, true);
    bool success = SendHttpRequest(std::move(request), response);
    if (!success) {
        return false;
    }

    // get signature and md5
    // or manifest
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
        changed = true;

        // remove old file
        if (fs::exists(fs::path(dir) / filenameOrigin)) {
            fs::remove(fs::path(dir) / filenameOrigin);
        }

        // download success ...
        fs::rename(fs::path(dir) / filename, fs::path(dir) / filenameOrigin);

        if (DumpManifest(dir, filenameOrigin, url, etag, lastModified)) {
            LOG_WARNING(sLogger, ("failed to dump manifest file", filename));
            return false;
        }
        // do backup ...
        // if (!backupExistingFile(fs::path(dir) / filename)) {
        //     LOG_WARNING(sLogger, ("failed to backup file", fs::path(dir) / filename));
        // }
        return true;
    }


    LOG_WARNING(sLogger,
                ("failed to download, statusCode", response.GetStatusCode())("filename", filename)("url", url));

    return false;
}

std::string
PackageManager::GetApmAgentDownloadUrl(APMLanguage lang, const std::string& region, const std::string& version) {
    auto endpoint = GetApmOssDefaultEndpoint(region, false);

    if (version.size() && version != "latest") {
        endpoint += '/';
        endpoint += version;
    }

    endpoint += '/';
    switch (lang) {
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

bool PackageManager::PrepareAPMAgent(APMLanguage lang,
                                     const std::string& appId,
                                     const std::string& region,
                                     const std::string& version,
                                     fs::path& outBootstrapPath) {
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

    auto agentPath = kDefaultJavaAgentDir / appId / kLatestAgentSubpath / kArmsJavaAgentFilename;
    // if (!backupExistingFile(agentPath)) {
    //     LOG_WARNING(sLogger, ("failed to backup existing agent file", ""));
    // }

    // 下载探针包
    auto url = GetApmAgentDownloadUrl(lang, region, version);
    LOG_INFO(sLogger, ("url", url));
    bool changed = false;
    // TODO filename 中我们可以加上 version，这样相当于有一层缓存了
    status
        = downloadWithRetry(url, kDefaultJavaAgentDir / appId / kLatestAgentSubpath, kArmsJavaAgentFilename, changed);
    if (!status) {
        LOG_WARNING(sLogger, ("failed to download from oss", kArmsJavaAgentFilename));
        // 恢复备份
        // restoreBackupFile(agentPath);
        return false;
    }

    if (changed
        || !fs::exists(kDefaultJavaAgentDir / appId / kLatestAgentSubpath / kJavaAgentSubpath
                       / kJavaAgentBootstrapFile)) {
        // 解压
        ArchiveHelper helper(kDefaultJavaAgentDir / appId / kLatestAgentSubpath / kArmsJavaAgentFilename,
                             kDefaultJavaAgentDir / appId / kLatestAgentSubpath);
        status = helper.Extract();
        if (!status) {
            LOG_WARNING(sLogger, ("failed to unzip agent from oss", kArmsJavaAgentFilename));
            // 恢复备份
            // restoreBackupFile(agentPath);
            return false;
        }

        // 清理当前目录
        int ret = ClearDirectory(kDefaultJavaAgentDir / appId / kCurrentAgentSubpath);
        if (ret) {
            LOG_WARNING(sLogger, ("Failed to clear current dir, ret", ret));
        }

        // 复制最新版本到当前目录
        try {
            fs::copy(kDefaultJavaAgentDir / appId / kLatestAgentSubpath,
                     kDefaultJavaAgentDir / appId / kCurrentAgentSubpath,
                     fs::copy_options::recursive);
        } catch (const fs::filesystem_error& e) {
            LOG_WARNING(sLogger, ("failed to copy latest dir to current dir, e", e.what()));
            // 恢复备份
            // restoreBackupFile(agentPath);
            return false;
        }
    }

    outBootstrapPath
        = kDefaultJavaAgentDir / appId / kCurrentAgentSubpath / kJavaAgentSubpath / kJavaAgentBootstrapFile;
    return true;
}

// TODO @qianlu.kk
bool PackageManager::UpdateExecHook(const std::string& regionId) {
    // 强制重新下载并覆盖
    // 先删除本地和系统文件
    UninstallExecHook();
    // 再重新下载
    return PrepareExecHook(regionId);
}

bool PackageManager::UninstallExecHook() {
    bool success = true;
    // 删除系统目录下的libexec-hook.so
    auto sysPath = kSysSharedLibraryPath / kDefaultExecHookName;
    if (fs::exists(sysPath)) {
        try {
            fs::remove(sysPath);
        } catch (const fs::filesystem_error& e) {
            LOG_WARNING(sLogger, ("failed to remove system exec hook", e.what()));
            success = false;
        }
    }
    // 删除本地下载目录下的libexec-hook.so及manifest
    auto localPath = kDefaultExecHookDownloadDir / kDefaultExecHookName;
    if (fs::exists(localPath)) {
        try {
            fs::remove(localPath);
        } catch (const fs::filesystem_error& e) {
            LOG_WARNING(sLogger, ("failed to remove local exec hook", e.what()));
            success = false;
        }
    }
    auto manifestPath = kDefaultExecHookDownloadDir / GetManifestFile(kDefaultExecHookName);
    if (fs::exists(manifestPath)) {
        try {
            fs::remove(manifestPath);
        } catch (const fs::filesystem_error& e) {
            LOG_WARNING(sLogger, ("failed to remove exec hook manifest", e.what()));
            success = false;
        }
    }

    // 删除
    if (fs::exists(kDefaultPreloadConfigFile)) {
        try {
            std::ifstream inFile(kDefaultPreloadConfigFile);
            std::stringstream buffer;
            buffer << inFile.rdbuf();
            std::string content = buffer.str();
            inFile.close();
            std::string execHookEntry = (kSysSharedLibraryPath / kDefaultExecHookName).string();
            std::istringstream iss(content);
            std::ostringstream oss;
            std::string line;
            while (std::getline(iss, line)) {
                if (line.find(execHookEntry) == std::string::npos) {
                    oss << line << "\n";
                }
            }
            std::ofstream outFile(kDefaultPreloadConfigFile, std::ios::out | std::ios::trunc);
            if (outFile) {
                outFile << oss.str();
                outFile.close();
            } else {
                LOG_WARNING(sLogger, ("failed to update ld.so.preload when uninstall exec hook", ""));
                success = false;
            }
        } catch (const std::exception& e) {
            LOG_WARNING(sLogger, ("failed to clean ld.so.preload", e.what()));
            success = false;
        }
    }
    return success;
}

bool PackageManager::RemoveAPMAgent(APMLanguage lang, const std::string& appId) {
    if (lang != APMLanguage::kJava) {
        LOG_WARNING(sLogger, ("language not supported", magic_enum::enum_name(lang)));
        return false;
    }

    // 只删除指定appId/agentVersion的包目录
    fs::path agentBaseDir = kDefaultJavaAgentDir / appId;
    if (!fs::exists(agentBaseDir)) {
        return true;
    }
    // 删除latest和current子目录
    bool success = true;
    std::vector<fs::path> subDirs = {agentBaseDir / kLatestAgentSubpath, agentBaseDir / kCurrentAgentSubpath};
    for (const auto& subDir : subDirs) {
        if (fs::exists(subDir)) {
            try {
                fs::remove_all(subDir);
            } catch (const fs::filesystem_error& e) {
                LOG_WARNING(sLogger, ("failed to remove agent subdir", subDir)("err", e.what()));
                success = false;
            }
        }
    }
    // 如果appId目录下已无内容，则删除appId目录
    if (fs::exists(agentBaseDir) && fs::is_empty(agentBaseDir)) {
        try {
            fs::remove(agentBaseDir);
        } catch (const fs::filesystem_error& e) {
            LOG_WARNING(sLogger, ("failed to remove agent appId dir", agentBaseDir)("err", e.what()));
            success = false;
        }
    }
    return success;
}

} // namespace logtail::apm
