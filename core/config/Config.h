/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <string>
#include <vector>
#include <list>
#include <boost/regex.hpp>
#include <re2/re2.h>
#include "DockerFileConfig.h"
#include "common/EncodingConverter.h"
#include "common/LogstoreFeedbackQueue.h"
#include "common/Flags.h"
#include "common/TimeUtil.h"
#include "aggregator/Aggregator.h"
#include "processor/BaseFilterNode.h"
#include "LogType.h"
#include "IntegrityConfig.h"

namespace logtail {

class LogFileReader;
class EventDispatcher;
class DevInode;
struct LogFilterRule;

enum RegionType { REGION_PUB, REGION_CORP };
enum CheckUpdateStat { NORMAL, UPDATE_CONFIG, UPDATE_BIN };

struct UserInfo {
    std::string aliuid;
    std::string accessKeyId;
    std::string accessKey;
    int32_t updateTime;
    UserInfo(const std::string& aliuidPara,
             const std::string& accessKeyIdPara,
             const std::string& accessKeyPara,
             int32_t updateTimePara) {
        aliuid = aliuidPara;
        accessKeyId = accessKeyIdPara;
        accessKey = accessKeyPara;
        updateTime = updateTimePara;
    }
};

struct SensitiveWordCastOption {
    static const int32_t MD5_OPTION = 0;
    static const int32_t CONST_OPTION = 1;
    int32_t option;
    std::string key;
    std::string constValue;
    bool replaceAll;
    std::shared_ptr<re2::RE2> mRegex; // deleted when config is deleted

    SensitiveWordCastOption() : option(CONST_OPTION), replaceAll(false) {}

    ~SensitiveWordCastOption();
};

class Config {
public:
    static const uint32_t kExactlyOnceMaxConcurrency = 512;

    class AdvancedConfig {
    public:
        AdvancedConfig();

        bool mForceMultiConfig = false; // force collect file and ignore other config
        bool mExtractPartialFields = false;
        bool mPassTagsToPlugin = true; // pass file tags to plugin system.
        std::string mRawLogTag; // if mUploadRawLog is true, use this string as raw log tag
        int32_t mBatchSendInterval;
        BaseFilterNodePtr mFilterExpressionRoot;
        uint32_t mExactlyOnceConcurrency = 0;
        bool mEnableLogPositionMeta = false; // Add inode/offset to log.
        size_t mMaxRotateQueueSize;
        int32_t mCloseUnusedReaderInterval;
        int32_t mSpecifiedYear = -1; // Year deduction.
        uint16_t mSearchCheckpointDirDepth = 0; // Max directory depth when search checkpoint.

        bool mEnablePreciseTimestamp = false;
        std::string mPreciseTimestampKey;
        TimeStampUnit mPreciseTimestampUnit;
    };

public:
    std::vector<std::string> mWildcardPaths;
    // if const path not empty, we can access this dir directly, and do not need scan parent's dir
    std::vector<std::string> mConstWildcardPaths;
    int16_t mWildcardDepth;
    std::string mBasePath; // base path, not terminated by "*" "**"
    std::string mFilePattern; // file name format
    LogType mLogType;
    std::string mConfigName; // name of log e.g. aliyun_com "##1.0##sls-zc-test$home-log"
    std::string mLogBeginReg; // the log begin line regex
    std::string mProjectName; // project name
    bool mIsPreserve; // true is service dir, false is job dir
    int mPreserveDepth; // for job dir, the depth that will not be watch timeout
    int mMaxDepth; // dirs exceed depth will not be monitored
    std::shared_ptr<std::list<std::string>> mRegs; //regex of log format
    std::shared_ptr<std::list<std::string>> mKeys; //description of each part of regex
    std::string mTimeFormat; // for common_reg_log
    std::string mCategory;
    int32_t mTailLimit; //KB
    std::vector<std::string> mUnAcceptDirPattern; //if not empty, files matching this pattern will not be watched
    //  topic is optional,  only used for common_reg_log and apsara_log , the mTopicFormat is three value type :
    // "default" : for common_reg_log , it's empty, for apsara_log, it's the path without "@hostname" and ".LOG" suffix
    //             if the topic is not set, it will be set to default
    // "none" :  the topic is empty for all the type
    // "reg" : like  (.*) the whole path , .*/(.*) , the last part of the path
    // "group_topic": get value with GetLogtailConfig request
    // "customized://<customized_topic_name>": mTopicFormat will be rewrite to
    //   "customized" and use mCustomizedTopic to save <customized_topic_name>.
    std::string mTopicFormat;
    std::string mGroupTopic;
    std::string mCustomizedTopic;
    FileEncoding mFileEncoding;
    std::shared_ptr<LogFilterRule> mFilterRule;
    bool mLocalStorage;
    int mVersion;
    bool mDiscardNoneUtf8;
    std::string mAliuid;
    std::string mRegion;
    std::string mStreamLogTag;
    bool mDiscardUnmatch;
    std::vector<std::string> mColumnKeys;
    std::string mSeparator;
    char mQuote;
    // for delimiter log, accept logs without enough keys or not
    // eg, keys -> [a, b, c], raw log "xx|yy", log -> [a->xx, b->yy]
    bool mAcceptNoEnoughKeys;
    bool mAutoExtend;
    std::string mTimeKey;
    std::vector<std::string> mShardHashKey;
    bool mTailExisted;
    std::unordered_map<std::string, std::vector<SensitiveWordCastOption>> mSensitiveWordCastOptions;
    bool mUploadRawLog; // true to update raw log to sls
    bool mSimpleLogFlag;
    bool mTimeZoneAdjust;
    int mLogTimeZoneOffsetSecond;
    int32_t mCreateTime; // create time of this config
    int32_t
        mMaxSendBytesPerSecond; // limit for logstore, not just this config. so if we have multi configs with different mMaxSendBytesPerSecond, this logstore's limit will be a random mMaxSendBytesPerSecond
    int32_t mSendRateExpireTime; // send rate expire time, along with mMaxSendBytesPerSecond
    int64_t mLogDelayAlarmBytes; // if <=0, discard it, default 0.
    LogstoreFeedBackKey mLogstoreKey;
    int32_t mPriority; // default is 0(no priority); 1-3, max priority is 1
    int64_t mLogDelaySkipBytes; // if <=0, discard it, default 0.

    std::string mPluginConfig; // plugin config string
    // mDockerContainerPaths is only modified when HoldOn
    std::shared_ptr<std::vector<DockerContainerPath>>
        mDockerContainerPaths; // docker file mapping paths, if mPluginConfig is true, mDockerContainerPaths must not be NULL
    bool mLocalFlag; // this config is loaded from local or remote
    bool mDockerFileFlag; // docker file flag
    bool mPluginProcessFlag; // file config with plugin process
    DATA_MERGE_TYPE mMergeType; // config's merge type, default merge by topic

    // for data integrity
    IntegrityConfigPtr mIntegrityConfig;

    // for line count
    LineCountConfigPtr mLineCountConfig;

    bool mIsFuseMode = false;
    bool mMarkOffsetFlag = false;
    bool mCollectBackwardTillBootTime = false;
    AdvancedConfig mAdvancedConfig;

    // Blacklist control.
    bool mHasBlacklist = false;
    // Absolute path of directories to filter, such as /app/log. It will filter
    // subdirectories as well.
    std::vector<std::string> mDirPathBlacklist;
    // Wildcard (*/?) is included, use fnmatch with FNM_PATHNAME to filter, which
    // will also filter subdirectories. For example, /app/* filters subdirectory
    // /app/log but keep /app/text.log, because /app does not match /app/*. And
    // because /app/log is filtered, so any changes under it will be ignored, so
    // both /app/log/sub and /app/log/text.log will be blacklisted.
    std::vector<std::string> mWildcardDirPathBlacklist;
    // Multiple level wildcard (**) is included, use fnmatch with 0 as flags to filter,
    // which will blacklist /path/a/b with pattern /path/**.
    std::vector<std::string> mMLWildcardDirPathBlacklist;
    // Absolute path of files to filter, */? is supported, such as /app/log/100*.log.
    std::vector<std::string> mFilePathBlacklist;
    // Multiple level wildcard (**) is included.
    std::vector<std::string> mMLFilePathBlacklist;
    // File name only, */? is supported too, such as 100*.log. It is similar to
    // mFilePattern, but works in reversed way.
    std::vector<std::string> mFileNameBlacklist;

    Config() {
        mSimpleLogFlag = false;
        mCreateTime = 0;
        mMaxSendBytesPerSecond = -1;
        mSendRateExpireTime = -1;
        mMergeType = MERGE_BY_TOPIC;
        mTimeZoneAdjust = false;
        mLogTimeZoneOffsetSecond = 0;
        mLocalFlag = false;
        mDockerFileFlag = false;
        mPluginProcessFlag = false;
        mPriority = 0;
        mLogDelaySkipBytes = 0;
        mDockerContainerPaths = NULL;
        mAcceptNoEnoughKeys = false;
    }

    Config(const std::string& basePath,
           const std::string& filePattern,
           LogType logType,
           const std::string& logName,
           const std::string& logBeginReg,
           const std::string& projectName,
           bool isPreserve,
           int preserveDepth,
           int maxDepth,
           const std::string& category,
           bool uploadRawLog = false,
           const std::string& StreamLogTag = "",
           bool discardUnmatch = true,
           std::list<std::string>* regs = NULL,
           std::list<std::string>* keys = NULL,
           std::string timeFormat = "");

    bool SetDockerFileFlag(bool supportWildcardPath = false);

    bool IsSameDockerContainerPath(const std::string& paramsJSONStr, bool allFlag);

    bool UpdateDockerContainerPath(const std::string& paramsJSONStr, bool allFlag);
    bool DeleteDockerContainerPath(const std::string& paramsJSONStr);

    DockerContainerPath* GetContainerPathByLogPath(const std::string& logPath);

    bool IsMatch(const std::string& path, const std::string& name);

    void ParseWildcardPath();

    /**whether the dir is watched time out
     *
     * @param path for the watching dir
     */
    bool IsTimeout(const std::string& path);

    bool WithinMaxDepth(const std::string& path);

    /**create new log file reader for a file
     *
     * @param fileName is the newly modified log file
     */
    LogFileReader* CreateLogFileReader(const std::string& dir,
                                       const std::string& file,
                                       const DevInode& devInode,
                                       bool forceFromBeginning = false);

    static bool CompareByPathLength(Config* left, Config* right);
    static bool CompareByDepthAndCreateTime(Config* left, Config* right);

    std::string GetProjectName() const { return mProjectName; }
    std::string GetCategory() const { return mCategory; }

    bool PassingTagsToPlugin() const { return mAdvancedConfig.mPassTagsToPlugin; }

    // IsDirectoryInBlacklist checks if the directory with @dirPath is in blacklist.
    bool IsDirectoryInBlacklist(const std::string& dirPath) const;

    void SetTailLimit(int32_t size);

private:
    bool IsWildcardPathMatch(const std::string& path, const std::string& name = "");

    // IsObjectInBlacklist checks if the object is in blacklist.
    // @path: absolute path, must be real path in local file system, so for docker file,
    //   the caller must do some converting.
    bool IsObjectInBlacklist(const std::string& path, const std::string& name) const;

    // IsFileNameInBlacklist checks if the file name is in blacklist.
    bool IsFileNameInBlacklist(const std::string& fileName) const;

#if defined(APSARA_UNIT_TEST_MAIN)
    friend class ConfigUpdatorUnittest;
#endif
};

} // namespace logtail
