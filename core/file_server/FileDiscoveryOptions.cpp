// Copyright 2023 iLogtail Authors
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

#include "file_server/FileDiscoveryOptions.h"

#include <filesystem>

#if defined(__linux__)
#include <fnmatch.h>
#elif defined(_MSC_VER)
#include "common/EncodingConverter.h"
#endif

#include "common/FileSystemUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "common/StringTools.h"

using namespace std;

namespace logtail {


DEFINE_FLAG_INT32(max_file_paths_per_input_config, "", 10);

// basePath must not stop with '/'
inline bool _IsSubPath(const string& basePath, const string& subPath) {
    size_t pathSize = subPath.size();
    size_t basePathSize = basePath.size();
    if (pathSize >= basePathSize && memcmp(subPath.c_str(), basePath.c_str(), basePathSize) == 0) {
        return pathSize == basePathSize || PATH_SEPARATOR[0] == subPath[basePathSize];
    }
    return false;
}

inline bool _IsPathMatched(const string& basePath, const string& path, int maxDepth) {
    size_t pathSize = path.size();
    size_t basePathSize = basePath.size();
    if (pathSize >= basePathSize && memcmp(path.c_str(), basePath.c_str(), basePathSize) == 0) {
        // need check max_depth + preserve depth
        if (pathSize == basePathSize) {
            return true;
        }
        // like /log  --> /log/a/b , maxDepth 2, true
        // like /log  --> /log1/a/b , maxDepth 2, false
        // like /log  --> /log/a/b/c , maxDepth 2, false
        else if (PATH_SEPARATOR[0] == path[basePathSize]) {
            if (maxDepth < 0) {
                return true;
            }
            int depth = 0;
            for (size_t i = basePathSize; i < pathSize; ++i) {
                if (PATH_SEPARATOR[0] == path[i]) {
                    ++depth;
                    if (depth > maxDepth) {
                        return false;
                    }
                }
            }
            return true;
        }
    }
    return false;
}

static bool isNotSubPath(const string& basePath, const string& path) {
    size_t pathSize = path.size();
    size_t basePathSize = basePath.size();
    if (pathSize < basePathSize || memcmp(path.c_str(), basePath.c_str(), basePathSize) != 0) {
        return true;
    }

    // For wildcard Windows path C:\*, mWildcardPaths[0] will be C:\, will
    //   fail on following check because of path[basePathSize].
    // Advaned check for such case if flag enable_root_path_collection is enabled.
    auto checkPos = basePathSize;
#if defined(_MSC_VER)
    if (BOOL_FLAG(enable_root_path_collection)) {
        if (basePathSize >= 2 && basePath[basePathSize - 1] == PATH_SEPARATOR[0] && basePath[basePathSize - 2] == ':') {
            --checkPos;
        }
    }
#endif
    return basePathSize > 1 && pathSize > basePathSize && path[checkPos] != PATH_SEPARATOR[0];
}

void BuildAndSortPathItems(const unordered_map<string, FileDiscoveryConfig>& nameConfigMap,
                           vector<PathItem>& outSortedPaths,
                           vector<PathItem>& outWildcardPaths) {
    outSortedPaths.clear();
    outWildcardPaths.clear();

    for (auto itr = nameConfigMap.begin(); itr != nameConfigMap.end(); ++itr) {
        const FileDiscoveryOptions* config = itr->second.first;
        const auto& pathInfos = config->GetBasePathInfos();

        // 按路径类型分类（使用原始 basePath）
        for (size_t i = 0; i < pathInfos.size(); ++i) {
            const auto& pathInfo = pathInfos[i];
            int32_t depth = CalculatePathDepth(pathInfo.basePath);
            if (pathInfo.hasWildcard()) {
                outWildcardPaths.emplace_back(pathInfo.basePath, depth, itr->second, &pathInfo, i);
            } else {
                outSortedPaths.emplace_back(pathInfo.basePath, depth, itr->second, &pathInfo, i);
            }
        }
    }

    // 对所有精确路径按深度排序（深度降序，保证深路径优先）
    sort(outSortedPaths.begin(), outSortedPaths.end(), [](const PathItem& left, const PathItem& right) {
        if (left.depth != right.depth) {
            return left.depth > right.depth;
        }
        // 深度相同时，按创建时间排序
        return left.config.second->GetCreateTime() < right.config.second->GetCreateTime();
    });
}

bool FileDiscoveryOptions::Init(const Json::Value& config,
                                const CollectionPipelineContext& ctx,
                                const string& pluginType) {
    string errorMsg;

    // FilePaths + MaxDirSearchDepth
    if (!GetMandatoryListParam<string>(config, "FilePaths", mFilePaths, errorMsg)) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           errorMsg,
                           pluginType,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }
    if (mFilePaths.size() == 0) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           "list param FilePaths is empty",
                           pluginType,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    } else if (mFilePaths.size() > static_cast<size_t>(INT32_FLAG(max_file_paths_per_input_config))) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           "list param FilePaths has more than " + ToString(INT32_FLAG(max_file_paths_per_input_config))
                               + " elements",
                           pluginType,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }
    // Support multiple paths now
    for (size_t i = 0; i < mFilePaths.size(); ++i) {
        auto dirAndFile = GetDirAndFileNameFromPath(mFilePaths[i]);
        string basePath = ConvertAndNormalizeNativePath(dirAndFile.first);
        string filePattern = ConvertAndNormalizeNativePath(dirAndFile.second);

        BasePathInfo pathInfo(basePath, filePattern);

        if (pathInfo.basePath.empty() || pathInfo.filePattern.empty()) {
            PARAM_ERROR_RETURN(ctx.GetLogger(),
                               ctx.GetAlarm(),
                               "string param FilePaths[" + ToString(i) + "] is invalid",
                               pluginType,
                               ctx.GetConfigName(),
                               ctx.GetProjectName(),
                               ctx.GetLogstoreName(),
                               ctx.GetRegion());
        }
        size_t len = pathInfo.basePath.size();
        if (len > 2 && pathInfo.basePath[len - 1] == '*' && pathInfo.basePath[len - 2] == '*'
            && pathInfo.basePath[len - 3] == filesystem::path::preferred_separator) {
            if (len == 3) {
                // for parent path like /**, we should maintain root dir, i.e., /
                pathInfo.basePath = pathInfo.basePath.substr(0, len - 2);
            } else {
                pathInfo.basePath = pathInfo.basePath.substr(0, len - 3);
#if defined(_MSC_VER)
                // For Windows, if the result is a drive letter (e.g., "C:"), append backslash
                // to make it a proper root path (e.g., "C:\\")
                if (pathInfo.basePath.size() == 2 && pathInfo.basePath[1] == ':') {
                    pathInfo.basePath += filesystem::path::preferred_separator;
                }
#endif
            }
            // MaxDirSearchDepth is only valid when parent path ends with **
            if (!GetOptionalIntParam(config, "MaxDirSearchDepth", mMaxDirSearchDepth, errorMsg)) {
                PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                                      ctx.GetAlarm(),
                                      errorMsg,
                                      mMaxDirSearchDepth,
                                      pluginType,
                                      ctx.GetConfigName(),
                                      ctx.GetProjectName(),
                                      ctx.GetLogstoreName(),
                                      ctx.GetRegion());
            }
        }
        ParseWildcardPath(pathInfo);
        mBasePathInfos.push_back(std::move(pathInfo));
    }

    // PreservedDirDepth
    if (!GetOptionalIntParam(config, "PreservedDirDepth", mPreservedDirDepth, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mPreservedDirDepth,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    // ExcludeFilePaths
    if (!GetOptionalListParam<string>(config, "ExcludeFilePaths", mExcludeFilePaths, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    } else {
        for (size_t i = 0; i < mExcludeFilePaths.size(); ++i) {
            string excludeFilePath = ConvertAndNormalizeNativePath(mExcludeFilePaths[i]);
            if (!filesystem::path(excludeFilePath).is_absolute()) {
                PARAM_WARNING_IGNORE(ctx.GetLogger(),
                                     ctx.GetAlarm(),
                                     "string param ExcludeFilePaths[" + ToString(i) + "] is not absolute",
                                     pluginType,
                                     ctx.GetConfigName(),
                                     ctx.GetProjectName(),
                                     ctx.GetLogstoreName(),
                                     ctx.GetRegion());
                continue;
            }
            bool isMultipleLevelWildcard = mExcludeFilePaths[i].find("**") != string::npos;
            if (isMultipleLevelWildcard) {
                mMLFilePathBlacklist.push_back(excludeFilePath);
            } else {
                mFilePathBlacklist.push_back(excludeFilePath);
            }
        }
    }

    // ExcludeFiles
    if (!GetOptionalListParam<string>(config, "ExcludeFiles", mExcludeFiles, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    } else {
        for (size_t i = 0; i < mExcludeFiles.size(); ++i) {
            if (mExcludeFiles[i].find(filesystem::path::preferred_separator) != string::npos) {
                PARAM_WARNING_IGNORE(ctx.GetLogger(),
                                     ctx.GetAlarm(),
                                     "string param ExcludeFiles[" + ToString(i) + "] contains path separator",
                                     pluginType,
                                     ctx.GetConfigName(),
                                     ctx.GetProjectName(),
                                     ctx.GetLogstoreName(),
                                     ctx.GetRegion());
                continue;
            }
            mFileNameBlacklist.push_back(ConvertAndNormalizeNativePath(mExcludeFiles[i]));
        }
    }

    // ExcludeDirs
    if (!GetOptionalListParam<string>(config, "ExcludeDirs", mExcludeDirs, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    } else {
        for (size_t i = 0; i < mExcludeDirs.size(); ++i) {
            string excludeDir = ConvertAndNormalizeNativePath(mExcludeDirs[i]);

            if (!filesystem::path(excludeDir).is_absolute()) {
                PARAM_WARNING_IGNORE(ctx.GetLogger(),
                                     ctx.GetAlarm(),
                                     "string param ExcludeDirs[" + ToString(i) + "] is not absolute",
                                     pluginType,
                                     ctx.GetConfigName(),
                                     ctx.GetProjectName(),
                                     ctx.GetLogstoreName(),
                                     ctx.GetRegion());
                continue;
            }
            bool isMultipleLevelWildcard = mExcludeDirs[i].find("**") != string::npos;
            if (isMultipleLevelWildcard) {
                mMLWildcardDirPathBlacklist.push_back(excludeDir);
                continue;
            }
            bool isWildcardPath
                = mExcludeDirs[i].find("*") != string::npos || mExcludeDirs[i].find("?") != string::npos;
            if (isWildcardPath) {
                mWildcardDirPathBlacklist.push_back(excludeDir);
            } else {
                mDirPathBlacklist.push_back(excludeDir);
            }
        }
    }
    if (!mDirPathBlacklist.empty() || !mWildcardDirPathBlacklist.empty() || !mMLWildcardDirPathBlacklist.empty()
        || !mMLFilePathBlacklist.empty() || !mFileNameBlacklist.empty() || !mFilePathBlacklist.empty()) {
        mHasBlacklist = true;
    }

    // AllowingCollectingFilesInRootDir
    if (!GetOptionalBoolParam(
            config, "AllowingCollectingFilesInRootDir", mAllowingCollectingFilesInRootDir, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mAllowingCollectingFilesInRootDir,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    } else if (mAllowingCollectingFilesInRootDir) {
        BOOL_FLAG(enable_root_path_collection) = mAllowingCollectingFilesInRootDir;
    }

    // AllowingIncludedByMultiConfigs
    if (!GetOptionalBoolParam(config, "AllowingIncludedByMultiConfigs", mAllowingIncludedByMultiConfigs, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mAllowingIncludedByMultiConfigs,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }
    return true;
}

pair<string, string> FileDiscoveryOptions::GetDirAndFileNameFromPath(const string& filePath) {
    filesystem::path path(filePath);
    if (path.is_relative()) {
        error_code ec;
        path = filesystem::absolute(path, ec);
    }
    path = path.lexically_normal();
    return make_pair(path.parent_path().string(), path.filename().string());
}

void FileDiscoveryOptions::ParseWildcardPath(BasePathInfo& pathInfo) {
    pathInfo.wildcardPaths.clear();
    pathInfo.constWildcardPaths.clear();
    pathInfo.wildcardDepth = 0;
    if (pathInfo.basePath.size() == 0)
        return;
    size_t posA = pathInfo.basePath.find('*', 0);
    size_t posB = pathInfo.basePath.find('?', 0);
    size_t pos;
    if (posA == string::npos) {
        if (posB == string::npos)
            return;
        else
            pos = posB;
    } else {
        if (posB == string::npos)
            pos = posA;
        else
            pos = min(posA, posB);
    }
    if (pos == 0)
        return;
    pos = pathInfo.basePath.rfind(filesystem::path::preferred_separator, pos);
    if (pos == string::npos)
        return;

        // Check if there is only one path separator, for Windows, the first path
        // separator is next to the first ':'.
#if defined(__linux__)
    if (pos == 0)
#elif defined(_MSC_VER)
    if (pos - 1 == pathInfo.basePath.find(':'))
#endif
    {
        pathInfo.wildcardPaths.push_back(pathInfo.basePath.substr(0, pos + 1));
    } else
        pathInfo.wildcardPaths.push_back(pathInfo.basePath.substr(0, pos));
    while (true) {
        size_t nextPos = pathInfo.basePath.find(filesystem::path::preferred_separator, pos + 1);
        if (nextPos == string::npos)
            break;
        pathInfo.wildcardPaths.push_back(pathInfo.basePath.substr(0, nextPos));
        string dirName = pathInfo.basePath.substr(pos + 1, nextPos - pos - 1);
        if (dirName.find('?') == string::npos && dirName.find('*') == string::npos) {
            pathInfo.constWildcardPaths.push_back(dirName);
        } else {
            pathInfo.constWildcardPaths.push_back("");
        }
        pos = nextPos;
    }
    pathInfo.wildcardPaths.push_back(pathInfo.basePath);
    if (pos < pathInfo.basePath.size()) {
        string dirName = pathInfo.basePath.substr(pos + 1);
        if (dirName.find('?') == string::npos && dirName.find('*') == string::npos) {
            pathInfo.constWildcardPaths.push_back(dirName);
        } else {
            pathInfo.constWildcardPaths.push_back("");
        }
    }

    for (size_t i = 0; i < pathInfo.basePath.size(); ++i) {
        if (filesystem::path::preferred_separator == pathInfo.basePath[i])
            ++pathInfo.wildcardDepth;
    }
}


bool FileDiscoveryOptions::IsFilenameMatched(const std::string& filename, const BasePathInfo& pathInfo) const {
    // Check if filename matches the specific pathInfo's filePattern
    return fnmatch(pathInfo.filePattern.c_str(), filename.c_str(), 0) == 0;
}

bool FileDiscoveryOptions::IsDirectoryInBlacklist(const string& dir) const {
    if (!mHasBlacklist) {
        return false;
    }

    const string nativeDir = NormalizeNativePath(dir);

    for (auto& dp : mDirPathBlacklist) {
        if (_IsSubPath(dp, nativeDir)) {
            return true;
        }
    }
    for (auto& dp : mWildcardDirPathBlacklist) {
        if (0 == fnmatch(dp.c_str(), nativeDir.c_str(), FNM_PATHNAME)) {
            return true;
        }
    }
    for (auto& dp : mMLWildcardDirPathBlacklist) {
        if (0 == fnmatch(dp.c_str(), nativeDir.c_str(), 0)) {
            return true;
        }
    }
    return false;
}

bool FileDiscoveryOptions::IsFilepathInBlacklist(const std::string& filepath) const {
    if (!mHasBlacklist) {
        return false;
    }

    const string nativePath = NormalizeNativePath(filepath);

    for (const auto& fp : mFilePathBlacklist) {
        if (0 == fnmatch(fp.c_str(), nativePath.c_str(), FNM_PATHNAME)) {
            return true;
        }
    }
    for (const auto& fp : mMLFilePathBlacklist) {
        if (0 == fnmatch(fp.c_str(), nativePath.c_str(), 0)) {
            return true;
        }
    }
    return false;
}

bool FileDiscoveryOptions::IsObjectInBlacklist(const string& path, const string& name) const {
    if (!mHasBlacklist) {
        return false;
    }

    if (IsDirectoryInBlacklist(path)) {
        return true;
    }
    if (name.empty()) {
        return false;
    }

    auto const filePath = PathJoin(path, name);
    const string nativePath = NormalizeNativePath(filePath);

    for (auto& fp : mFilePathBlacklist) {
        if (0 == fnmatch(fp.c_str(), nativePath.c_str(), FNM_PATHNAME)) {
            return true;
        }
    }
    for (auto& fp : mMLFilePathBlacklist) {
        if (0 == fnmatch(fp.c_str(), nativePath.c_str(), 0)) {
            return true;
        }
    }

    return false;
}

bool FileDiscoveryOptions::IsFilenameInBlacklist(const string& fileName) const {
    if (!mHasBlacklist) {
        return false;
    }

    for (auto& pattern : mFileNameBlacklist) {
        if (0 == fnmatch(pattern.c_str(), fileName.c_str(), 0)) {
            return true;
        }
    }
    return false;
}

// IsMatch checks if the object is matched with current config.
// @path: absolute path of location in where the object stores in.
// @name: the name of the object. If the object is directory, this parameter will be empty.
bool FileDiscoveryOptions::IsMatch(const string& path, const string& name) const {
    return IsMatch(path, name, nullptr);
}

// IsMatch with match info output
bool FileDiscoveryOptions::IsMatch(const string& path, const string& name, int32_t* outMatchedPathDepth) const {
    // Iterate through all path configurations with index
    for (size_t pathInfoIdx = 0; pathInfoIdx < mBasePathInfos.size(); ++pathInfoIdx) {
        const auto& pathInfo = mBasePathInfos[pathInfoIdx];
        // Check if the file name is matched with this path's pattern
        if (!name.empty()) {
            if (fnmatch(pathInfo.filePattern.c_str(), name.c_str(), 0) != 0)
                continue; // Not matched, try next path
            if (IsFilenameInBlacklist(name)) {
                continue; // In blacklist, try next path
            }
        }

        // File in docker.
        if (mEnableContainerDiscovery) {
            if (pathInfo.wildcardPaths.size() > (size_t)0) {
                ContainerInfo* containerPath = GetContainerPathByLogPath(path);
                if (containerPath == NULL) {
                    continue; // Try next path
                }

                // 找到该 path 对应的 mRealBaseDirs 索引
                size_t realBaseDirIndex = GetRealBaseDirIndex(containerPath, path);
                if (realBaseDirIndex >= containerPath->mRealBaseDirs.size()
                    || containerPath->mRealBaseDirs[realBaseDirIndex].empty()) {
                    continue; // 该路径未映射或索引越界
                }

                const string& realBaseDir = containerPath->mRealBaseDirs[realBaseDirIndex];

                // convert Logtail's real path to config path. eg /host_all/var/lib/xxx/home/admin/logs ->
                // /home/admin/logs
                if (pathInfo.wildcardPaths[0].size() == (size_t)1) {
                    // if wildcardPaths[0] is root path, do not add wildcardPaths[0]
                    if (IsWildcardPathMatch(path.substr(realBaseDir.size()), name, pathInfo)) {
                        if (outMatchedPathDepth)
                            *outMatchedPathDepth = CalculatePathDepth(pathInfo.basePath);
                        return true;
                    }
                } else {
                    string convertPath = pathInfo.wildcardPaths[0] + path.substr(realBaseDir.size());
                    if (IsWildcardPathMatch(convertPath, name, pathInfo)) {
                        if (outMatchedPathDepth)
                            *outMatchedPathDepth = CalculatePathDepth(pathInfo.basePath);
                        return true;
                    }
                }
                continue; // Try next path
            }

            // Normal base path.
            for (size_t i = 0; i < mContainerInfos->size(); ++i) {
                // 只检查当前 pathInfo 对应的 mRealBaseDirs 索引
                if (pathInfoIdx >= (*mContainerInfos)[i].mRealBaseDirs.size()) {
                    continue; // 索引越界，跳过该容器
                }

                const string& containerBasePath = (*mContainerInfos)[i].mRealBaseDirs[pathInfoIdx];
                if (containerBasePath.empty()) {
                    continue; // 跳过空路径
                }

                if (_IsPathMatched(containerBasePath, path, mMaxDirSearchDepth)) {
                    if (!mHasBlacklist) {
                        if (outMatchedPathDepth)
                            *outMatchedPathDepth = CalculatePathDepth(pathInfo.basePath);
                        return true;
                    }

                    // ContainerBasePath contains base path, remove it.
                    auto pathInContainer = pathInfo.basePath + path.substr(containerBasePath.size());
                    LOG_DEBUG(sLogger, ("pathInContainer", pathInContainer)("name", name));
                    if (!IsObjectInBlacklist(pathInContainer, name)) {
                        if (outMatchedPathDepth)
                            *outMatchedPathDepth = CalculatePathDepth(pathInfo.basePath);
                        return true;
                    }
                }
            }
            continue; // Try next path
        }

        // File not in docker: wildcard or non-wildcard.
        if (pathInfo.wildcardPaths.empty()) {
            if (_IsPathMatched(pathInfo.basePath, path, mMaxDirSearchDepth) && !IsObjectInBlacklist(path, name)) {
                if (outMatchedPathDepth)
                    *outMatchedPathDepth = CalculatePathDepth(pathInfo.basePath);
                return true; // Matched!
            }
        } else {
            if (IsWildcardPathMatch(path, name, pathInfo)) {
                if (outMatchedPathDepth)
                    *outMatchedPathDepth = CalculatePathDepth(pathInfo.basePath);
                return true; // Matched!
            }
        }
    }

    return false; // No path matched
}

bool FileDiscoveryOptions::IsWildcardPathMatch(const string& path,
                                               const string& name,
                                               const BasePathInfo& pathInfo) const {
    const string nativePath = NormalizeNativePath(path);
    size_t pos = 0;
    int16_t d = 0;
    int16_t maxWildcardDepth = pathInfo.wildcardDepth + 1;
    while (d < maxWildcardDepth) {
        pos = nativePath.find(PATH_SEPARATOR[0], pos);
        if (pos == string::npos)
            break;
        ++d;
        ++pos;
    }

    if (d < pathInfo.wildcardDepth)
        return false;
    else if (d == pathInfo.wildcardDepth) {
        return fnmatch(pathInfo.basePath.c_str(), nativePath.c_str(), FNM_PATHNAME) == 0
            && !IsObjectInBlacklist(path, name);
    } else if (pos > 0) {
        if (!(fnmatch(pathInfo.basePath.c_str(), nativePath.substr(0, pos - 1).c_str(), FNM_PATHNAME) == 0
              && !IsObjectInBlacklist(path, name))) {
            return false;
        }
    } else
        return false;

    // Only pos > 0 will reach here, which means the level of path is deeper than base,
    // need to check max depth.
    if (mMaxDirSearchDepth < 0)
        return true;
    int depth = 1;
    while (depth < mMaxDirSearchDepth + 1) {
        pos = nativePath.find(PATH_SEPARATOR[0], pos);
        if (pos == string::npos)
            return true;
        ++depth;
        ++pos;
    }
    return false;
}

// XXX: assume path is a subdir under one of the base paths
bool FileDiscoveryOptions::IsTimeout(const string& path) const {
    if (mPreservedDirDepth < 0)
        return false;

    // Check against all base paths
    for (const auto& pathInfo : mBasePathInfos) {
        // we do not check if (path.find(basePath) == 0)
        size_t pos = pathInfo.basePath.size();
        int depthCount = 0;
        while ((pos = path.find(PATH_SEPARATOR[0], pos)) != string::npos) {
            ++depthCount;
            ++pos;
            if (depthCount > mPreservedDirDepth)
                return true;
        }
    }
    return false;
}

bool FileDiscoveryOptions::WithinMaxDepth(const string& path) const {
    // default -1 to compatible with old version
    if (mMaxDirSearchDepth < 0)
        return true;
    if (mEnableContainerDiscovery) {
        // docker file, should check is match
        return IsMatch(path, "");
    }

    // Check against all base paths
    for (const auto& pathInfo : mBasePathInfos) {
        const auto& base = pathInfo.wildcardPaths.empty() ? pathInfo.basePath : pathInfo.wildcardPaths[0];
        if (isNotSubPath(base, path)) {
            continue; // Try next path
        }

        if (pathInfo.wildcardPaths.size() == 0) {
            size_t pos = pathInfo.basePath.size();
            int depthCount = 0;
            while ((pos = path.find(PATH_SEPARATOR, pos)) != string::npos) {
                ++depthCount;
                ++pos;
                if (depthCount > mMaxDirSearchDepth)
                    return false;
            }
            return true; // Within depth for this path
        } else {
            int32_t depth = 0 - pathInfo.wildcardDepth;
            for (size_t i = 0; i < path.size(); ++i) {
                if (path[i] == PATH_SEPARATOR[0])
                    ++depth;
            }
            if (depth < 0) {
                LOG_ERROR(sLogger, ("invalid sub dir", path)("basePath", pathInfo.basePath));
                continue; // Try next path
            } else if (depth > mMaxDirSearchDepth) {
                continue; // Try next path
            } else {
                // Windows doesn't support double *, so we have to check this.
                auto basePath = pathInfo.basePath;
                if (basePath.empty() || basePath.back() != '*')
                    basePath += '*';
                if (fnmatch(basePath.c_str(), path.c_str(), 0) != 0) {
                    LOG_ERROR(sLogger, ("invalid sub dir", path)("basePath", pathInfo.basePath));
                    continue; // Try next path
                }
                return true; // Matched this path
            }
        }
    }
    return false; // No path matched
}

ContainerInfo* FileDiscoveryOptions::GetContainerPathByLogPath(const string& logPath) const {
    if (!mContainerInfos) {
        return NULL;
    }
    // reverse order to find the latest container
    for (int i = mContainerInfos->size() - 1; i >= 0; --i) {
        // 检查该容器的所有真实路径
        for (const auto& realBaseDir : (*mContainerInfos)[i].mRealBaseDirs) {
            if (!realBaseDir.empty() && _IsSubPath(realBaseDir, logPath)) {
                return &(*mContainerInfos)[i];
            }
        }
    }
    return NULL;
}

// 根据实际文件路径，找到它对应的 mRealBaseDirs 索引
size_t FileDiscoveryOptions::GetRealBaseDirIndex(const ContainerInfo* containerInfo, const string& logPath) const {
    if (!containerInfo) {
        return 0;
    }

    // 找到最长匹配的路径索引
    size_t bestMatchIndex = 0;
    size_t longestMatchLength = 0;

    for (size_t i = 0; i < containerInfo->mRealBaseDirs.size(); ++i) {
        const string& realBaseDir = containerInfo->mRealBaseDirs[i];
        if (!realBaseDir.empty() && _IsSubPath(realBaseDir, logPath)) {
            if (realBaseDir.size() > longestMatchLength) {
                longestMatchLength = realBaseDir.size();
                bestMatchIndex = i;
            }
        }
    }

    return bestMatchIndex;
}


bool FileDiscoveryOptions::UpdateRawContainerInfo(const std::shared_ptr<RawContainerInfo>& rawContainerInfo,
                                                  const CollectionPipelineContext* ctx,
                                                  const std::string& checkpointPath) {
    if (!mContainerInfos) {
        return false;
    }

    ContainerInfo containerInfo;
    containerInfo.mRawContainerInfo = rawContainerInfo;

    mContainerDiscovery.GetCustomExternalTags(
        rawContainerInfo->mEnv, rawContainerInfo->mK8sInfo.mLabels, containerInfo.mExtraTags);

    if (!checkpointPath.empty()) {
        LOG_INFO(sLogger,
                 ("set container base dir for checkpoint path", checkpointPath)("container id",
                                                                                containerInfo.mRawContainerInfo->mID));
        containerInfo.mRealBaseDirs.push_back(checkpointPath);
    } else {
        if (!mDeduceAndSetContainerBaseDirFunc(containerInfo, ctx, this)) {
            return false;
        }
    }
    for (size_t i = 0; i < mContainerInfos->size(); ++i) {
        if ((*mContainerInfos)[i].mRawContainerInfo->mID == rawContainerInfo->mID) {
            (*mContainerInfos)[i] = containerInfo;
            return true;
        }
    }
    mContainerInfos->push_back(containerInfo);
    return true;
}

bool FileDiscoveryOptions::DeleteRawContainerInfo(const std::string& containerID) {
    if (!mContainerInfos) {
        return false;
    }

    for (auto iter = mContainerInfos->begin(); iter != mContainerInfos->end(); ++iter) {
        if (iter->mRawContainerInfo && iter->mRawContainerInfo->mID == containerID) {
            LOG_INFO(sLogger, ("delete container", containerID));
            mContainerInfos->erase(iter);
            return true;
        }
    }
    return false;
}
} // namespace logtail
