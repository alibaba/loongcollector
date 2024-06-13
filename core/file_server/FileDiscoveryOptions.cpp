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
#endif

#include "common/FileSystemUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "common/StringTools.h"

using namespace std;

namespace logtail {

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

bool FileDiscoveryOptions::CompareByPathLength(pair<const FileDiscoveryOptions*, const PipelineContext*> left,
                                               pair<const FileDiscoveryOptions*, const PipelineContext*> right) {
    int32_t leftDepth = 0;
    int32_t rightDepth = 0;
    for (size_t i = 0; i < (left.first->mBasePath).size(); ++i) {
        if (PATH_SEPARATOR[0] == (left.first->mBasePath)[i]) {
            leftDepth++;
        }
    }
    for (size_t i = 0; i < (right.first->mBasePath).size(); ++i) {
        if (PATH_SEPARATOR[0] == (right.first->mBasePath)[i]) {
            rightDepth++;
        }
    }
    return leftDepth > rightDepth;
}

bool FileDiscoveryOptions::CompareByDepthAndCreateTime(
    pair<const FileDiscoveryOptions*, const PipelineContext*> left,
    pair<const FileDiscoveryOptions*, const PipelineContext*> right) {
    int32_t leftDepth = 0;
    int32_t rightDepth = 0;
    for (size_t i = 0; i < (left.first->mBasePath).size(); ++i) {
        if (PATH_SEPARATOR[0] == (left.first->mBasePath)[i]) {
            leftDepth++;
        }
    }
    for (size_t i = 0; i < (right.first->mBasePath).size(); ++i) {
        if (PATH_SEPARATOR[0] == (right.first->mBasePath)[i]) {
            rightDepth++;
        }
    }
    if (leftDepth > rightDepth) {
        return true;
    }
    if (leftDepth == rightDepth) {
        return left.second->GetCreateTime() < right.second->GetCreateTime();
    }
    return false;
}

bool FileDiscoveryOptions::Init(const Json::Value& config, const PipelineContext& ctx, const string& pluginName) {
    string errorMsg;

    // FilePaths + MaxDirSearchDepth
    if (!GetMandatoryListParam<string>(config, "FilePaths", mFilePaths, errorMsg)) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           errorMsg,
                           pluginName,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }
    if (mFilePaths.size() != 1) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           "list param FilePaths has more than 1 element",
                           pluginName,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }
    auto dirAndFile = GetDirAndFileNameFromPath(mFilePaths[0]);
    mBasePath = dirAndFile.first, mFilePattern = dirAndFile.second;
    if (mBasePath.empty() || mFilePattern.empty()) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           "string param FilePaths[0] is invalid",
                           pluginName,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }
#if defined(_MSC_VER)
    mBasePath = EncodingConverter::GetInstance()->FromUTF8ToACP(mBasePath);
    mFilePattern = EncodingConverter::GetInstance()->FromUTF8ToACP(mFilePattern);
#endif
    size_t len = mBasePath.size();
    if (len > 2 && mBasePath[len - 1] == '*' && mBasePath[len - 2] == '*'
        && mBasePath[len - 3] == filesystem::path::preferred_separator) {
        if (len == 3) {
            // for parent path like /**, we should maintain root dir, i.e., /
            mBasePath = mBasePath.substr(0, len - 2);
        } else {
            mBasePath = mBasePath.substr(0, len - 3);
        }
        // MaxDirSearchDepth is only valid when parent path ends with **
        if (!GetOptionalIntParam(config, "MaxDirSearchDepth", mMaxDirSearchDepth, errorMsg)) {
            PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                                  ctx.GetAlarm(),
                                  errorMsg,
                                  mMaxDirSearchDepth,
                                  pluginName,
                                  ctx.GetConfigName(),
                                  ctx.GetProjectName(),
                                  ctx.GetLogstoreName(),
                                  ctx.GetRegion());
        }
    }
    ParseWildcardPath();

    // PreservedDirDepth
    if (!GetOptionalIntParam(config, "PreservedDirDepth", mPreservedDirDepth, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mPreservedDirDepth,
                              pluginName,
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
                             pluginName,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    } else {
        for (size_t i = 0; i < mExcludeFilePaths.size(); ++i) {
            if (!filesystem::path(mExcludeFilePaths[i]).is_absolute()) {
                PARAM_WARNING_IGNORE(ctx.GetLogger(),
                                     ctx.GetAlarm(),
                                     "string param ExcludeFilePaths[" + ToString(i) + "] is not absolute",
                                     pluginName,
                                     ctx.GetConfigName(),
                                     ctx.GetProjectName(),
                                     ctx.GetLogstoreName(),
                                     ctx.GetRegion());
                continue;
            }
            bool isMultipleLevelWildcard = mExcludeFilePaths[i].find("**") != string::npos;
            if (isMultipleLevelWildcard) {
                mMLFilePathBlacklist.push_back(mExcludeFilePaths[i]);
            } else {
                mFilePathBlacklist.push_back(mExcludeFilePaths[i]);
            }
        }
    }

    // ExcludeFiles
    if (!GetOptionalListParam<string>(config, "ExcludeFiles", mExcludeFiles, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginName,
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
                                     pluginName,
                                     ctx.GetConfigName(),
                                     ctx.GetProjectName(),
                                     ctx.GetLogstoreName(),
                                     ctx.GetRegion());
                continue;
            }
            mFileNameBlacklist.push_back(mExcludeFiles[i]);
        }
    }

    // ExcludeDirs
    if (!GetOptionalListParam<string>(config, "ExcludeDirs", mExcludeDirs, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginName,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    } else {
        for (size_t i = 0; i < mExcludeDirs.size(); ++i) {
            if (!filesystem::path(mExcludeDirs[i]).is_absolute()) {
                PARAM_WARNING_IGNORE(ctx.GetLogger(),
                                     ctx.GetAlarm(),
                                     "string param ExcludeDirs[" + ToString(i) + "] is not absolute",
                                     pluginName,
                                     ctx.GetConfigName(),
                                     ctx.GetProjectName(),
                                     ctx.GetLogstoreName(),
                                     ctx.GetRegion());
                continue;
            }
            bool isMultipleLevelWildcard = mExcludeDirs[i].find("**") != string::npos;
            if (isMultipleLevelWildcard) {
                mMLWildcardDirPathBlacklist.push_back(mExcludeDirs[i]);
                continue;
            }
            bool isWildcardPath
                = mExcludeDirs[i].find("*") != string::npos || mExcludeDirs[i].find("?") != string::npos;
            if (isWildcardPath) {
                mWildcardDirPathBlacklist.push_back(mExcludeDirs[i]);
            } else {
                mDirPathBlacklist.push_back(mExcludeDirs[i]);
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
                              pluginName,
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
                              pluginName,
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

void FileDiscoveryOptions::ParseWildcardPath() {
    mWildcardPaths.clear();
    mConstWildcardPaths.clear();
    mWildcardDepth = 0;
    if (mBasePath.size() == 0)
        return;
    size_t posA = mBasePath.find('*', 0);
    size_t posB = mBasePath.find('?', 0);
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
    pos = mBasePath.rfind(filesystem::path::preferred_separator, pos);
    if (pos == string::npos)
        return;

        // Check if there is only one path separator, for Windows, the first path
        // separator is next to the first ':'.
#if defined(__linux__)
    if (pos == 0)
#elif defined(_MSC_VER)
    if (pos - 1 == mBasePath.find(':'))
#endif
    {
        mWildcardPaths.push_back(mBasePath.substr(0, pos + 1));
    } else
        mWildcardPaths.push_back(mBasePath.substr(0, pos));
    while (true) {
        size_t nextPos = mBasePath.find(filesystem::path::preferred_separator, pos + 1);
        if (nextPos == string::npos)
            break;
        mWildcardPaths.push_back(mBasePath.substr(0, nextPos));
        string dirName = mBasePath.substr(pos + 1, nextPos - pos - 1);
        if (dirName.find('?') == string::npos && dirName.find('*') == string::npos) {
            mConstWildcardPaths.push_back(dirName);
        } else {
            mConstWildcardPaths.push_back("");
        }
        pos = nextPos;
    }
    mWildcardPaths.push_back(mBasePath);
    if (pos < mBasePath.size()) {
        string dirName = mBasePath.substr(pos + 1);
        if (dirName.find('?') == string::npos && dirName.find('*') == string::npos) {
            mConstWildcardPaths.push_back(dirName);
        } else {
            mConstWildcardPaths.push_back("");
        }
    }

    for (size_t i = 0; i < mBasePath.size(); ++i) {
        if (filesystem::path::preferred_separator == mBasePath[i])
            ++mWildcardDepth;
    }
}

bool FileDiscoveryOptions::IsDirectoryInBlacklist(const string& dirPath) const {
    if (!mHasBlacklist) {
        return false;
    }

    for (auto& dp : mDirPathBlacklist) {
        if (_IsSubPath(dp, dirPath)) {
            return true;
        }
    }
    for (auto& dp : mWildcardDirPathBlacklist) {
        if (0 == fnmatch(dp.c_str(), dirPath.c_str(), FNM_PATHNAME)) {
            return true;
        }
    }
    for (auto& dp : mMLWildcardDirPathBlacklist) {
        if (0 == fnmatch(dp.c_str(), dirPath.c_str(), 0)) {
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
    for (auto& fp : mFilePathBlacklist) {
        if (0 == fnmatch(fp.c_str(), filePath.c_str(), FNM_PATHNAME)) {
            return true;
        }
    }
    for (auto& fp : mMLFilePathBlacklist) {
        if (0 == fnmatch(fp.c_str(), filePath.c_str(), 0)) {
            return true;
        }
    }

    return false;
}

bool FileDiscoveryOptions::IsFileNameInBlacklist(const string& fileName) const {
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
    // Check if the file name is matched or blacklisted.
    if (!name.empty()) {
        if (fnmatch(mFilePattern.c_str(), name.c_str(), 0) != 0)
            return false;
        if (IsFileNameInBlacklist(name)) {
            return false;
        }
    }

    // File in docker.
    if (mEnableContainerDiscovery) {
        if (mWildcardPaths.size() > (size_t)0) {
            ContainerInfo* containerPath = GetContainerPathByLogPath(path);
            if (containerPath == NULL) {
                return false;
            }
            // convert Logtail's real path to config path. eg /host_all/var/lib/xxx/home/admin/logs -> /home/admin/logs
            if (mWildcardPaths[0].size() == (size_t)1) {
                // if mWildcardPaths[0] is root path, do not add mWildcardPaths[0]
                return IsWildcardPathMatch(path.substr(containerPath->mRealBaseDir.size()), name);
            } else {
                string convertPath = mWildcardPaths[0] + path.substr(containerPath->mRealBaseDir.size());
                return IsWildcardPathMatch(convertPath, name);
            }
        }

        // Normal base path.
        for (size_t i = 0; i < mContainerInfos->size(); ++i) {
            const string& containerBasePath = (*mContainerInfos)[i].mRealBaseDir;
            if (_IsPathMatched(containerBasePath, path, mMaxDirSearchDepth)) {
                if (!mHasBlacklist) {
                    return true;
                }

                // ContainerBasePath contains base path, remove it.
                auto pathInContainer = mBasePath + path.substr(containerBasePath.size());
                if (!IsObjectInBlacklist(pathInContainer, name))
                    return true;
            }
        }
        return false;
    }

    // File not in docker: wildcard or non-wildcard.
    if (mWildcardPaths.empty()) {
        return _IsPathMatched(mBasePath, path, mMaxDirSearchDepth) && !IsObjectInBlacklist(path, name);
    } else
        return IsWildcardPathMatch(path, name);
}

bool FileDiscoveryOptions::IsWildcardPathMatch(const string& path, const string& name) const {
    size_t pos = 0;
    int16_t d = 0;
    int16_t maxWildcardDepth = mWildcardDepth + 1;
    while (d < maxWildcardDepth) {
        pos = path.find(PATH_SEPARATOR[0], pos);
        if (pos == string::npos)
            break;
        ++d;
        ++pos;
    }

    if (d < mWildcardDepth)
        return false;
    else if (d == mWildcardDepth) {
        return fnmatch(mBasePath.c_str(), path.c_str(), FNM_PATHNAME) == 0 && !IsObjectInBlacklist(path, name);
    } else if (pos > 0) {
        if (!(fnmatch(mBasePath.c_str(), path.substr(0, pos - 1).c_str(), FNM_PATHNAME) == 0
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
        pos = path.find(PATH_SEPARATOR[0], pos);
        if (pos == string::npos)
            return true;
        ++depth;
        ++pos;
    }
    return false;
}

// XXX: assume path is a subdir under mBasePath
bool FileDiscoveryOptions::IsTimeout(const string& path) const {
    if (mPreservedDirDepth < 0 || mWildcardPaths.size() > 0)
        return false;

    // we do not check if (path.find(mBasePath) == 0)
    size_t pos = mBasePath.size();
    int depthCount = 0;
    while ((pos = path.find(PATH_SEPARATOR[0], pos)) != string::npos) {
        ++depthCount;
        ++pos;
        if (depthCount > mPreservedDirDepth)
            return true;
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

    {
        const auto& base = mWildcardPaths.empty() ? mBasePath : mWildcardPaths[0];
        if (isNotSubPath(base, path)) {
            LOG_ERROR(sLogger, ("path is not child of basePath, path", path)("basePath", base));
            return false;
        }
    }

    if (mWildcardPaths.size() == 0) {
        size_t pos = mBasePath.size();
        int depthCount = 0;
        while ((pos = path.find(PATH_SEPARATOR, pos)) != string::npos) {
            ++depthCount;
            ++pos;
            if (depthCount > mMaxDirSearchDepth)
                return false;
        }
    } else {
        int32_t depth = 0 - mWildcardDepth;
        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == PATH_SEPARATOR[0])
                ++depth;
        }
        if (depth < 0) {
            LOG_ERROR(sLogger, ("invalid sub dir", path)("basePath", mBasePath));
            return false;
        } else if (depth > mMaxDirSearchDepth)
            return false;
        else {
            // Windows doesn't support double *, so we have to check this.
            auto basePath = mBasePath;
            if (basePath.empty() || basePath.back() != '*')
                basePath += '*';
            if (fnmatch(basePath.c_str(), path.c_str(), 0) != 0) {
                LOG_ERROR(sLogger, ("invalid sub dir", path)("basePath", mBasePath));
                return false;
            }
        }
    }
    return true;
}

ContainerInfo* FileDiscoveryOptions::GetContainerPathByLogPath(const string& logPath) const {
    if (!mContainerInfos) {
        return NULL;
    }
    for (size_t i = 0; i < mContainerInfos->size(); ++i) {
        if (_IsSubPath((*mContainerInfos)[i].mRealBaseDir, logPath)) {
            return &(*mContainerInfos)[i];
        }
    }
    return NULL;
}

bool FileDiscoveryOptions::IsSameContainerInfo(const Json::Value& paramsJSON, const PipelineContext* ctx) {
    if (!mEnableContainerDiscovery)
        return true;

    if (!paramsJSON.isMember("AllCmd")) {
        ContainerInfo containerInfo;
        string errorMsg;
        if (!ContainerInfo::ParseByJSONObj(paramsJSON, containerInfo, errorMsg)) {
            LOG_ERROR(sLogger, ("invalid container info update param", errorMsg)("action", "ignore current cmd"));
            return true;
        }
        if (!mDeduceAndSetContainerBaseDirFunc(containerInfo, ctx, this)) {
            return true;
        }
        // try update
        for (size_t i = 0; i < mContainerInfos->size(); ++i) {
            if ((*mContainerInfos)[i] == containerInfo) {
                return true;
            }
        }
        return false;
    }

    // check all
    unordered_map<string, ContainerInfo> allPathMap;
    string errorMsg;
    if (!ContainerInfo::ParseAllByJSONObj(paramsJSON["AllCmd"], allPathMap, errorMsg)) {
        LOG_ERROR(sLogger, ("invalid container info update param", errorMsg)("action", "ignore current cmd"));
        return true;
    }

    // need add
    if (mContainerInfos->size() != allPathMap.size()) {
        return false;
    }

    for (size_t i = 0; i < mContainerInfos->size(); ++i) {
        unordered_map<string, ContainerInfo>::iterator iter = allPathMap.find((*mContainerInfos)[i].mID);
        // need delete
        if (iter == allPathMap.end()) {
            return false;
        }
        if (!mDeduceAndSetContainerBaseDirFunc(iter->second, ctx, this)) {
            return true;
        }
        // need update
        if ((*mContainerInfos)[i] != iter->second) {
            return false;
        }
    }
    // same
    return true;
}

bool FileDiscoveryOptions::UpdateContainerInfo(const Json::Value& paramsJSON, const PipelineContext* ctx) {
    if (!mContainerInfos)
        return false;

    if (!paramsJSON.isMember("AllCmd")) {
        ContainerInfo containerInfo;
        string errorMsg;
        if (!ContainerInfo::ParseByJSONObj(paramsJSON, containerInfo, errorMsg)) {
            LOG_ERROR(sLogger, ("invalid container info update param", errorMsg)("action", "ignore current cmd"));
            return false;
        }
        if (!mDeduceAndSetContainerBaseDirFunc(containerInfo, ctx, this)) {
            return false;
        }
        // try update
        for (size_t i = 0; i < mContainerInfos->size(); ++i) {
            if ((*mContainerInfos)[i].mID == containerInfo.mID) {
                // update
                (*mContainerInfos)[i] = containerInfo;
                return true;
            }
        }
        // add
        mContainerInfos->push_back(containerInfo);
        return true;
    }

    unordered_map<string, ContainerInfo> allPathMap;
    string errorMsg;
    if (!ContainerInfo::ParseAllByJSONObj(paramsJSON["AllCmd"], allPathMap, errorMsg)) {
        LOG_ERROR(sLogger,
                  ("invalid all docker container params",
                   "skip this path")("params", paramsJSON.toStyledString())("errorMsg", errorMsg));
        return false;
    }
    // if update all, clear and reset
    mContainerInfos->clear();
    for (unordered_map<string, ContainerInfo>::iterator iter = allPathMap.begin(); iter != allPathMap.end(); ++iter) {
        if (!mDeduceAndSetContainerBaseDirFunc(iter->second, ctx, this)) {
            return false;
        }
        mContainerInfos->push_back(iter->second);
    }
    return true;
}

bool FileDiscoveryOptions::DeleteContainerInfo(const Json::Value& paramsJSON) {
    if (!mContainerInfos)
        return false;

    ContainerInfo containerInfo;
    string errorMsg;
    if (!ContainerInfo::ParseByJSONObj(paramsJSON, containerInfo, errorMsg)) {
        LOG_ERROR(sLogger, ("invalid container info update param", errorMsg)("action", "ignore current cmd"));
        return false;
    }
    for (vector<ContainerInfo>::iterator iter = mContainerInfos->begin(); iter != mContainerInfos->end(); ++iter) {
        if (iter->mID == containerInfo.mID) {
            mContainerInfos->erase(iter);
            break;
        }
    }
    return true;
}

} // namespace logtail
