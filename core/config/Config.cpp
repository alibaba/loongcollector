// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Config.h"
#if defined(__linux__)
#include <fnmatch.h>
#endif
#include "common/Constants.h"
#include "common/FileSystemUtil.h"
#include "common/LogtailCommonFlags.h"
#include "reader/LogFileReader.h"
#include "reader/DelimiterLogFileReader.h"
#include "reader/JsonLogFileReader.h"
#include "logger/Logger.h"
#include "processor/LogFilter.h"

DEFINE_FLAG_INT32(logreader_max_rotate_queue_size, "", 20);
DECLARE_FLAG_STRING(raw_log_tag);
DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_INT32(reader_close_unused_file_time);
DECLARE_FLAG_INT32(search_checkpoint_default_dir_depth);

namespace logtail {

// basePath must not stop with '/'
inline bool _IsSubPath(const std::string& basePath, const std::string& subPath) {
    size_t pathSize = subPath.size();
    size_t basePathSize = basePath.size();
    if (pathSize >= basePathSize && memcmp(subPath.c_str(), basePath.c_str(), basePathSize) == 0) {
        return pathSize == basePathSize || PATH_SEPARATOR[0] == subPath[basePathSize];
    }
    return false;
}

inline bool _IsPathMatched(const std::string& basePath, const std::string& path, int maxDepth) {
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

Config::AdvancedConfig::AdvancedConfig()
    : mRawLogTag(STRING_FLAG(raw_log_tag)),
      mBatchSendInterval(INT32_FLAG(batch_send_interval)),
      mMaxRotateQueueSize(INT32_FLAG(logreader_max_rotate_queue_size)),
      mCloseUnusedReaderInterval(INT32_FLAG(reader_close_unused_file_time)),
      mSearchCheckpointDirDepth(static_cast<uint16_t>(INT32_FLAG(search_checkpoint_default_dir_depth))) {
}

Config::Config(const std::string& basePath,
               const std::string& filePattern,
               LogType logType,
               const std::string& logName,
               const std::string& logBeginReg,
               const std::string& projectName,
               bool isPreserve,
               int preserveDepth,
               int maxDepth,
               const std::string& category,
               bool uploadRawLog /*= false*/,
               const std::string& StreamLogTag /*= ""*/,
               bool discardUnmatch /*= true*/,
               std::list<std::string>* regs /*= NULL*/,
               std::list<std::string>* keys /*= NULL*/,
               std::string timeFormat /* = ""*/)
    : mBasePath(basePath),
      mFilePattern(filePattern),
      mLogType(logType),
      mConfigName(logName),
      mLogBeginReg(logBeginReg),
      mProjectName(projectName),
      mIsPreserve(isPreserve),
      mPreserveDepth(preserveDepth),
      mMaxDepth(maxDepth),
      mRegs(regs),
      mKeys(keys),
      mTimeFormat(timeFormat),
      mCategory(category),
      mStreamLogTag(StreamLogTag),
      mDiscardUnmatch(discardUnmatch),
      mUploadRawLog(uploadRawLog) {
#if defined(_MSC_VER)
    mBasePath = EncodingConverter::GetInstance()->FromUTF8ToACP(mBasePath);
    mFilePattern = EncodingConverter::GetInstance()->FromUTF8ToACP(mFilePattern);
#endif

    ParseWildcardPath();
    mTailLimit = 0;
    mTailExisted = false;
    mLogstoreKey = GenerateLogstoreFeedBackKey(GetProjectName(), GetCategory());
    mSimpleLogFlag = false;
    mCreateTime = 0;
    mMaxSendBytesPerSecond = -1;
    mSendRateExpireTime = -1;
    mMergeType = MERGE_BY_TOPIC;
    mTimeZoneAdjust = false;
    mLogTimeZoneOffsetSecond = 0;
    mLogDelayAlarmBytes = 0;
    mPriority = 0;
    mLogDelaySkipBytes = 0;
    mLocalFlag = false;
    mDockerFileFlag = false;
    mPluginProcessFlag = false;
    mAcceptNoEnoughKeys = false;
}

bool Config::IsDirectoryInBlacklist(const std::string& dirPath) const {
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

bool Config::IsObjectInBlacklist(const std::string& path, const std::string& name) const {
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

bool Config::IsFileNameInBlacklist(const std::string& fileName) const {
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
bool Config::IsMatch(const std::string& path, const std::string& name) {
    // Check if the file name is matched or blacklisted.
    if (!name.empty()) {
        if (fnmatch(mFilePattern.c_str(), name.c_str(), 0) != 0)
            return false;
        if (IsFileNameInBlacklist(name)) {
            return false;
        }
    }

    // File in docker.
    if (mDockerFileFlag) {
        if (mWildcardPaths.size() > (size_t)0) {
            DockerContainerPath* containerPath = GetContainerPathByLogPath(path);
            if (containerPath == NULL) {
                return false;
            }
            // convert Logtail's real path to config path. eg /host_all/var/lib/xxx/home/admin/logs -> /home/admin/logs
            if (mWildcardPaths[0].size() == (size_t)1) {
                // if mWildcardPaths[0] is root path, do not add mWildcardPaths[0]
                return IsWildcardPathMatch(path.substr(containerPath->mContainerPath.size()), name);
            } else {
                std::string convertPath = mWildcardPaths[0] + path.substr(containerPath->mContainerPath.size());
                return IsWildcardPathMatch(convertPath, name);
            }
        }

        // Normal base path.
        for (size_t i = 0; i < mDockerContainerPaths->size(); ++i) {
            const std::string& containerBasePath = (*mDockerContainerPaths)[i].mContainerPath;
            if (_IsPathMatched(containerBasePath, path, mMaxDepth)) {
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
        return _IsPathMatched(mBasePath, path, mMaxDepth) && !IsObjectInBlacklist(path, name);
    } else
        return IsWildcardPathMatch(path, name);
}

void Config::ParseWildcardPath() {
    mWildcardPaths.clear();
    mConstWildcardPaths.clear();
    mWildcardDepth = 0;
    if (mBasePath.size() == 0)
        return;
    size_t posA = mBasePath.find('*', 0);
    size_t posB = mBasePath.find('?', 0);
    size_t pos;
    if (posA == std::string::npos) {
        if (posB == std::string::npos)
            return;
        else
            pos = posB;
    } else {
        if (posB == std::string::npos)
            pos = posA;
        else
            pos = std::min(posA, posB);
    }
    if (pos == 0)
        return;
    pos = mBasePath.rfind(PATH_SEPARATOR[0], pos);
    if (pos == std::string::npos)
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
        size_t nextPos = mBasePath.find(PATH_SEPARATOR[0], pos + 1);
        if (nextPos == std::string::npos)
            break;
        mWildcardPaths.push_back(mBasePath.substr(0, nextPos));
        std::string dirName = mBasePath.substr(pos + 1, nextPos - pos - 1);
        LOG_DEBUG(sLogger, ("wildcard paths", mWildcardPaths[mWildcardPaths.size() - 1])("dir name", dirName));
        if (dirName.find('?') == std::string::npos && dirName.find('*') == std::string::npos) {
            mConstWildcardPaths.push_back(dirName);
        } else {
            mConstWildcardPaths.push_back("");
        }
        pos = nextPos;
    }
    mWildcardPaths.push_back(mBasePath);
    if (pos < mBasePath.size()) {
        std::string dirName = mBasePath.substr(pos + 1);
        if (dirName.find('?') == std::string::npos && dirName.find('*') == std::string::npos) {
            mConstWildcardPaths.push_back(dirName);
        } else {
            mConstWildcardPaths.push_back("");
        }
    }

    for (size_t i = 0; i < mBasePath.size(); ++i) {
        if (PATH_SEPARATOR[0] == mBasePath[i])
            ++mWildcardDepth;
    }
}

bool Config::IsWildcardPathMatch(const std::string& path, const std::string& name) {
    size_t pos = 0;
    int16_t d = 0;
    int16_t maxWildcardDepth = mWildcardDepth + 1;
    while (d < maxWildcardDepth) {
        pos = path.find(PATH_SEPARATOR[0], pos);
        if (pos == std::string::npos)
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
    if (mMaxDepth < 0)
        return true;
    int depth = 1;
    while (depth < mMaxDepth + 1) {
        pos = path.find(PATH_SEPARATOR[0], pos);
        if (pos == std::string::npos)
            return true;
        ++depth;
        ++pos;
    }
    return false;
}

// XXX: assume path is a subdir under mBasePath
bool Config::IsTimeout(const std::string& path) {
    if (mIsPreserve || mWildcardPaths.size() > 0)
        return false;

    // we do not check if (path.find(mBasePath) == 0)
    size_t pos = mBasePath.size();
    int depthCount = 0;
    while ((pos = path.find(PATH_SEPARATOR[0], pos)) != std::string::npos) {
        ++depthCount;
        ++pos;
        if (depthCount > mPreserveDepth)
            return true;
    }
    return false;
}

static bool isNotSubPath(const std::string& basePath, const std::string& path) {
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

bool Config::WithinMaxDepth(const std::string& path) {
    // default -1 to compatible with old version
    if (mMaxDepth < 0)
        return true;
    if (mDockerFileFlag) {
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
        while ((pos = path.find(PATH_SEPARATOR, pos)) != std::string::npos) {
            ++depthCount;
            ++pos;
            if (depthCount > mMaxDepth)
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
        } else if (depth > mMaxDepth)
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

bool Config::CompareByPathLength(Config* left, Config* right) {
    int32_t leftDepth = 0;
    int32_t rightDepth = 0;
    for (size_t i = 0; i < (left->mBasePath).size(); ++i) {
        if (PATH_SEPARATOR[0] == (left->mBasePath)[i]) {
            leftDepth++;
        }
    }
    for (size_t i = 0; i < (right->mBasePath).size(); ++i) {
        if (PATH_SEPARATOR[0] == (right->mBasePath)[i]) {
            rightDepth++;
        }
    }
    return leftDepth > rightDepth;
}

bool Config::CompareByDepthAndCreateTime(Config* left, Config* right) {
    int32_t leftDepth = 0;
    int32_t rightDepth = 0;
    for (size_t i = 0; i < (left->mBasePath).size(); ++i) {
        if (PATH_SEPARATOR[0] == (left->mBasePath)[i]) {
            leftDepth++;
        }
    }
    for (size_t i = 0; i < (right->mBasePath).size(); ++i) {
        if (PATH_SEPARATOR[0] == (right->mBasePath)[i]) {
            rightDepth++;
        }
    }
    if (leftDepth > rightDepth) {
        return true;
    }
    if (leftDepth == rightDepth) {
        return left->mCreateTime < right->mCreateTime;
    }
    return false;
}

LogFileReader* Config::CreateLogFileReader(const std::string& dir,
                                           const std::string& file,
                                           const DevInode& devInode,
                                           bool forceFromBeginning) {
#define STRING_DEEP_COPY(str) std::string(str.data(), str.size())

    LogFileReader* reader = NULL;
    if (mLogType == APSARA_LOG) {
        reader = new ApsaraLogFileReader(mProjectName,
                                         mCategory,
                                         dir,
                                         file,
                                         mTailLimit,
                                         STRING_DEEP_COPY(mTopicFormat),
                                         STRING_DEEP_COPY(mGroupTopic),
                                         mFileEncoding,
                                         mDiscardUnmatch,
                                         mDockerFileFlag);
    } else if (mLogType == REGEX_LOG) {
        reader = new CommonRegLogFileReader(mProjectName,
                                            mCategory,
                                            dir,
                                            file,
                                            mTailLimit,
                                            STRING_DEEP_COPY(mTimeFormat),
                                            STRING_DEEP_COPY(mTopicFormat),
                                            STRING_DEEP_COPY(mGroupTopic),
                                            mFileEncoding,
                                            mDiscardUnmatch,
                                            mDockerFileFlag);
        std::list<std::string>::iterator regitr = mRegs->begin();
        std::list<std::string>::iterator keyitr = mKeys->begin();
        CommonRegLogFileReader* commonRegLogFileReader = static_cast<CommonRegLogFileReader*>(reader);
        commonRegLogFileReader->SetTimeKey(mTimeKey);
        for (; regitr != mRegs->end(); ++regitr, ++keyitr) {
            commonRegLogFileReader->AddUserDefinedFormat(*regitr, *keyitr);
        }
    } else if (mLogType == DELIMITER_LOG) {
        reader = new DelimiterLogFileReader(mProjectName,
                                            mCategory,
                                            dir,
                                            file,
                                            mTailLimit,
                                            STRING_DEEP_COPY(mTimeFormat),
                                            STRING_DEEP_COPY(mTopicFormat),
                                            STRING_DEEP_COPY(mGroupTopic),
                                            mFileEncoding,
                                            STRING_DEEP_COPY(mSeparator),
                                            mQuote,
                                            mAutoExtend,
                                            mDiscardUnmatch,
                                            mDockerFileFlag,
                                            mAcceptNoEnoughKeys,
                                            mAdvancedConfig.mExtractPartialFields);
        DelimiterLogFileReader* delimiterLogFileReader = static_cast<DelimiterLogFileReader*>(reader);
        delimiterLogFileReader->SetColumnKeys(mColumnKeys, mTimeKey);
    } else if (mLogType == JSON_LOG) {
        reader = new JsonLogFileReader(mProjectName,
                                       mCategory,
                                       dir,
                                       file,
                                       mTailLimit,
                                       STRING_DEEP_COPY(mTimeFormat),
                                       STRING_DEEP_COPY(mTopicFormat),
                                       STRING_DEEP_COPY(mGroupTopic),
                                       mFileEncoding,
                                       mDiscardUnmatch,
                                       mDockerFileFlag);
        JsonLogFileReader* jsonLogFileReader = static_cast<JsonLogFileReader*>(reader);
        jsonLogFileReader->SetTimeKey(mTimeKey);
    } else {
        LOG_ERROR(sLogger, ("not supported log type", mLogType)("dir", dir)("file", file));
    }

    if (reader != NULL) {
        if ("customized" == mTopicFormat) {
            reader->SetTopicName(STRING_DEEP_COPY(mCustomizedTopic));
        }

        reader->SetFuseMode(mIsFuseMode);
        reader->SetMarkOffsetFlag(mMarkOffsetFlag);
        if (mIsFuseMode) {
            reader->SetFuseTrimedFilename(reader->GetLogPath().substr((STRING_FLAG(fuse_root_dir)).size() + 1));
        }
        if (mLogDelayAlarmBytes > 0) {
            reader->SetDelayAlarmBytes(mLogDelayAlarmBytes);
        }
        reader->SetDelaySkipBytes(mLogDelaySkipBytes);
        reader->SetConfigName(mConfigName);
        reader->SetRegion(mRegion);
        reader->SetLogBeginRegex(STRING_DEEP_COPY(mLogBeginReg));
        if (forceFromBeginning)
            reader->SetReadFromBeginning();
        reader->SetDevInode(devInode);
        if (mAdvancedConfig.mEnableLogPositionMeta) {
            sls_logs::LogTag inodeTag;
            inodeTag.set_key(LOG_RESERVED_KEY_INODE);
            inodeTag.set_value(std::to_string(devInode.inode));
            reader->AddExtraTags(std::vector<sls_logs::LogTag>{inodeTag});
        }
        reader->SetTimeFormat(mTimeFormat);
        reader->SetCloseUnusedInterval(mAdvancedConfig.mCloseUnusedReaderInterval);
        reader->SetLogstoreKey(mLogstoreKey);
        reader->SetPluginFlag(mPluginProcessFlag);
        reader->SetSpecifiedYear(mAdvancedConfig.mSpecifiedYear);
        reader->SetPreciseTimestampConfig(mAdvancedConfig.mEnablePreciseTimestamp,
                                          mAdvancedConfig.mPreciseTimestampKey,
                                          mAdvancedConfig.mPreciseTimestampUnit,
                                          mTimeZoneAdjust,
                                          mLogTimeZoneOffsetSecond);
        if (mDockerFileFlag) {
            DockerContainerPath* containerPath = GetContainerPathByLogPath(dir);
            if (containerPath == NULL) {
                LOG_ERROR(
                    sLogger,
                    ("can not get container path by log path, base path", mBasePath)("host path", dir + "/" + file));
            } else {
                // if config have wildcard path, use mWildcardPaths[0] as base path
                reader->SetDockerPath(mWildcardPaths.size() > 0 ? mWildcardPaths[0] : mBasePath,
                                      containerPath->mContainerPath.size());
                reader->AddExtraTags(containerPath->mContainerTags);
                reader->ResetTopic(mTopicFormat);
            }
        }

#ifndef _MSC_VER // Unnecessary on platforms without symbolic.
        fsutil::PathStat buf;
        if (!fsutil::PathStat::lstat(reader->GetLogPath(), buf)) {
            // should not happen
            reader->SetSymbolicLinkFlag(false);
            LOG_ERROR(sLogger, ("failed to stat file", reader->GetLogPath())("set symbolic link flag to false", ""));
        } else {
            reader->SetSymbolicLinkFlag(buf.IsLink());
        }
#endif

        auto readPolicy = mCollectBackwardTillBootTime ? LogFileReader::BACKWARD_TO_BOOT_TIME
                                                       : LogFileReader::BACKWARD_TO_FIXED_POS;
        reader->InitReader(mTailExisted, readPolicy, mAdvancedConfig.mExactlyOnceConcurrency);
    }

    return reader;
}

bool Config::SetDockerFileFlag(bool supportWildcardPath) {
    if (!supportWildcardPath && !mWildcardPaths.empty()) {
        return false;
    }
    if (!mDockerContainerPaths) {
        mDockerContainerPaths.reset(new std::vector<DockerContainerPath>());
    }
    mDockerFileFlag = true;
    return true;
}

void Config::SetTailLimit(int32_t size) {
    mTailLimit = (size >= 0 && size <= 100 * 1024 * 1024) // 0~100GB
        ? size
        : INT32_FLAG(default_tail_limit_kb);
}

DockerContainerPath* Config::GetContainerPathByLogPath(const std::string& logPath) {
    if (!mDockerContainerPaths) {
        return NULL;
    }
    for (size_t i = 0; i < mDockerContainerPaths->size(); ++i) {
        if (_IsSubPath((*mDockerContainerPaths)[i].mContainerPath, logPath)) {
            return &(*mDockerContainerPaths)[i];
        }
    }
    return NULL;
}

bool Config::IsSameDockerContainerPath(const std::string& paramsJSONStr, bool allFlag) {
    if (!mDockerContainerPaths)
        return true;

    if (!allFlag) {
        DockerContainerPath dockerContainerPath;
        if (!DockerContainerPath::ParseByJSONStr(paramsJSONStr, dockerContainerPath)) {
            LOG_ERROR(sLogger, ("invalid docker container params", "skip this path")("params", paramsJSONStr));
            return true;
        }
        // try update
        for (size_t i = 0; i < mDockerContainerPaths->size(); ++i) {
            if ((*mDockerContainerPaths)[i] == dockerContainerPath) {
                return true;
            }
        }
        return false;
    }

    // check all
    std::unordered_map<std::string, DockerContainerPath> allPathMap;
    if (!DockerContainerPath::ParseAllByJSONStr(paramsJSONStr, allPathMap)) {
        LOG_ERROR(sLogger, ("invalid all docker container params", "skip this path")("params", paramsJSONStr));
        return true;
    }

    // need add
    if (mDockerContainerPaths->size() != allPathMap.size()) {
        return false;
    }

    for (size_t i = 0; i < mDockerContainerPaths->size(); ++i) {
        std::unordered_map<std::string, DockerContainerPath>::iterator iter
            = allPathMap.find((*mDockerContainerPaths)[i].mContainerID);
        // need delete
        if (iter == allPathMap.end()) {
            return false;
        }
        // need update
        if ((*mDockerContainerPaths)[i] != iter->second) {
            return false;
        }
    }
    // same
    return true;
}

bool Config::UpdateDockerContainerPath(const std::string& paramsJSONStr, bool allFlag) {
    if (!mDockerContainerPaths)
        return false;

    if (!allFlag) {
        DockerContainerPath dockerContainerPath;
        if (!DockerContainerPath::ParseByJSONStr(paramsJSONStr, dockerContainerPath)) {
            LOG_ERROR(sLogger, ("invalid docker container params", "skip this path")("params", paramsJSONStr));
            return false;
        }
        // try update
        for (size_t i = 0; i < mDockerContainerPaths->size(); ++i) {
            if ((*mDockerContainerPaths)[i].mContainerID == dockerContainerPath.mContainerID) {
                // update
                (*mDockerContainerPaths)[i] = dockerContainerPath;
                return true;
            }
        }
        // add
        mDockerContainerPaths->push_back(dockerContainerPath);
        return true;
    }

    std::unordered_map<std::string, DockerContainerPath> allPathMap;
    if (!DockerContainerPath::ParseAllByJSONStr(paramsJSONStr, allPathMap)) {
        LOG_ERROR(sLogger, ("invalid all docker container params", "skip this path")("params", paramsJSONStr));
        return false;
    }
    // if update all, clear and reset
    mDockerContainerPaths->clear();
    for (std::unordered_map<std::string, DockerContainerPath>::iterator iter = allPathMap.begin();
         iter != allPathMap.end();
         ++iter) {
        mDockerContainerPaths->push_back(iter->second);
    }
    return true;
}

bool Config::DeleteDockerContainerPath(const std::string& paramsJSONStr) {
    if (!mDockerContainerPaths)
        return false;

    DockerContainerPath dockerContainerPath;
    if (!DockerContainerPath::ParseByJSONStr(paramsJSONStr, dockerContainerPath)) {
        return false;
    }
    for (std::vector<DockerContainerPath>::iterator iter = mDockerContainerPaths->begin();
         iter != mDockerContainerPaths->end();
         ++iter) {
        if (iter->mContainerID == dockerContainerPath.mContainerID) {
            mDockerContainerPaths->erase(iter);
            break;
        }
    }
    return true;
}

} // namespace logtail
