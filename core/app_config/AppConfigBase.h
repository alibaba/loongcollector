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

#ifndef _APSARA_LOGTAIL_APP_CONFIG_H
#define _APSARA_LOGTAIL_APP_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <json/json.h>
#include "common/Lock.h"
#include "common/util.h"
#include "log_pb/sls_logs.pb.h"

namespace logtail {

class AppConfigBase {
private:
    mutable SpinLock mAppConfigLock;

    // ilogtail_config.json content for rebuild
    std::string mIlogtailConfigJson;

    // syslog
    std::string mStreamLogAddress;
    uint32_t mStreamLogTcpPort;
    uint32_t mStreamLogPoolSizeInMb;
    uint32_t mStreamLogRcvLenPerCall;
    bool mOpenStreamLog;

    // performance
    float mCpuUsageUpLimit;
#if defined(__linux__) || defined(__APPLE__)
    __attribute__((aligned(64))) int64_t mMemUsageUpLimit;
#elif defined(_MSC_VER)
    int64_t mMemUsageUpLimit;
#endif
    int32_t mProcessThreadCount;
    bool mInputFlowControl;
    bool mResourceAutoScale;
    float mMachineCpuUsageThreshold;
    float mScaledCpuUsageUpLimit;

    // sender
    int32_t mMaxHoldedDataSize;
    int32_t mMaxBufferNum;
    int32_t mBytePerSec;
    int32_t mMaxBytePerSec;
    int32_t mNumOfBufferFile;
    int32_t mLocalFileSize;
    int32_t mSendRequestConcurrency;
    std::string mBufferFilePath;

    // checkpoint
    std::string mCheckPointFilePath;

    // local config
    std::string mMappingConfigPath;

    bool mSendRandomSleep;
    bool mSendFlowControl;

    int32_t mMaxMultiConfigSize;
    bool mAcceptMultiConfigFlag;
    bool mIgnoreDirInodeChanged;

    std::string mUserConfigPath;
    std::string mUserLocalConfigPath;
    std::string mUserLocalConfigDirPath;
    std::string mUserLocalYamlConfigDirPath;
    bool mLogParseAlarmFlag;
    std::string mProcessExecutionDir;
    std::string mWorkingDir;

    bool mContainerMode;
    std::string mContainerMountConfigPath;
    std::string mConfigIP;
    std::string mConfigHostName;
    std::string mAlipayZone;
    int32_t mSystemBootTime = -1;

    // used to get log config instead of mConfigIp if set, eg: "127.0.0.1.fuse",
    std::string mCustomizedConfigIP;

    // config file path to save docker file cmd info
    std::string mDockerFilePathConfig;

    bool mNoInotify;
    // inotify black list, the path should not end with '/'
    std::unordered_multiset<std::string> mInotifyBlackList;

    int32_t mSendDataPort; // port to send data, not for config.
    bool mShennongSocket; // bind shennong domain socket or not.
    // tags load from env, this will been add to all loggroup
    std::vector<sls_logs::LogTag> mEnvTags;

    bool mPurageContainerMode;

    // Monitor thread will check last read event time, if exceeds this,
    // logtail will force quit, 7200s by default.
    int32_t mForceQuitReadTimeout;

    std::string mLogtailSysConfDir; //MUST ends with path separator

    // For such security case: logtail -> proxy server + firewall (domain rule).
    // By default, logtail will construct HTTP request URL by concating host IP with
    //   request path, this policy can be disabled by this parameters.
    bool mEnableHostIPReplace = true;

    // For cases that proxy or firewall might remove some headers in response from SLS.
    // By default, SLS response has an x-log-requestid header, this is used to verify
    //   if the response is from SLS.
    // Introduced because of global acceleration mode, DCDN might return invalid response
    //   in such case, which leads to data lost.
    bool mEnableResponseVerification = true;

    bool mEnableCheckpointSyncWrite = false;

    // SLS server will reject logs with time out of range, which might happen when local
    //   system time is changed.
    // By enabling this feature, logtail will use the offset between server time and
    //   local time to adjust logs' time automatically.
    bool mEnableLogTimeAutoAdjust = false;

    virtual void LoadAddrConfig(const Json::Value& confJson) = 0;

    void CheckAndAdjustParameters();
    void MergeJson(Json::Value& mainConfJson, const Json::Value& subConfJson);
    void LoadIncludeConfig(Json::Value& confJson);
    void LoadSyslogConf(const Json::Value& confJson);

    void DumpAllFlagsToMap(std::unordered_map<std::string, std::string>& flagMap);
    void ReadFlagsFromMap(const std::unordered_map<std::string, std::string>& flagMap);
    void ParseJsonToFlags(const Json::Value& confJson);
    void ParseEnvToFlags();

    void LoadResourceConf(const Json::Value& confJson);
    void LoadOtherConf(const Json::Value& confJson);
    void LoadGlobalFuseConf(const Json::Value& confJson);
    void SetIlogtailConfigJson(const std::string& configJson) {
        ScopedSpinLock lock(mAppConfigLock);
        mIlogtailConfigJson = configJson;
    }
    // LoadEnvTags loads env tags from environment.
    // 1. load STRING_FLAG(default_env_tag_keys) to get all env keys
    // 2. load each keys to get env key
    // 3. if no such env key, value will be empty
    // eg.
    // env
    //    ALIYUN_LOG_ENV_TAGS=a|b|c
    //    a=1
    //    b=2
    // tags
    //    a : 1
    //    b : 2
    //    c :
    void LoadEnvTags();

    // LoadEnvResourceLimit loads resource limit from env config.
    // Read values will replace corresponding configs in ilogtail_config.json.
    void LoadEnvResourceLimit();

    // logtail is in purage container mode when STRING_FLAG(ilogtail_user_defined_id_env_name) exist and /logtail_host exist
    void CheckPurageContainerMode();

    static void InitEnvMapping(const std::string& envStr, std::map<std::string, std::string>& envMapping);
    static void SetConfigFlag(const std::string& flagName, const std::string& value);

protected:
    std::string mDefaultRegion;
    std::string mBindInterface;
    //compatible mode, old ilogtail_config.json have no resource usage settings but data_server_address/config_server_address
    bool mIsOldPubRegion;

    EndpointAddressType mConfigServerAddressNetType = EndpointAddressType::INNER;

public:
    AppConfigBase();
    virtual ~AppConfigBase(){};

    void LoadAppConfig(const std::string& ilogtailConfigFile);

    bool NoInotify() const { return mNoInotify; }

    bool IsInInotifyBlackList(const std::string& path) const;

    bool IsLogParseAlarmValid() const { return mLogParseAlarmFlag; }

    bool IsSendRandomSleep() const { return mSendRandomSleep; }

    bool IsSendFlowControl() const { return mSendFlowControl; }

    std::string GetDefaultRegion() const;

    void SetDefaultRegion(const std::string& region);

    uint32_t GetStreamLogTcpPort() const { return mStreamLogTcpPort; }

    const std::string& GetStreamLogAddress() const { return mStreamLogAddress; }

    uint32_t GetStreamLogPoolSizeInMb() const { return mStreamLogPoolSizeInMb; }

    uint32_t GetStreamLogRcvLenPerCall() const { return mStreamLogRcvLenPerCall; }

    bool GetOpenStreamLog() const { return mOpenStreamLog; }

    std::string GetIlogtailConfigJson() {
        ScopedSpinLock lock(mAppConfigLock);
        return mIlogtailConfigJson;
    }

    bool IsAcceptMultiConfig() const { return mAcceptMultiConfigFlag; }

    void SetAcceptMultiConfig(bool flag) { mAcceptMultiConfigFlag = flag; }

    int32_t GetMaxMultiConfigSize() const { return mMaxMultiConfigSize; }

    void SetMaxMultiConfigSize(int32_t maxSize) { mMaxMultiConfigSize = maxSize; }

    const std::string& GetCheckPointFilePath() const { return mCheckPointFilePath; }

    bool IsInputFlowControl() const { return mInputFlowControl; }

    bool IsResourceAutoScale() const { return mResourceAutoScale; }

    float GetMachineCpuUsageThreshold() const { return mMachineCpuUsageThreshold; }

    float GetScaledCpuUsageUpLimit() const { return mScaledCpuUsageUpLimit; }

    float GetCpuUsageUpLimit() const { return mCpuUsageUpLimit; }

    int64_t GetMemUsageUpLimit() const { return mMemUsageUpLimit; }

    int32_t GetMaxHoldedDataSize() const { return mMaxHoldedDataSize; }

    uint32_t GetMaxBufferNum() const { return mMaxBufferNum; }

    int32_t GetMaxBytePerSec() const { return mMaxBytePerSec; }

    int32_t GetBytePerSec() const { return mBytePerSec; }

    int32_t GetNumOfBufferFile() const { return mNumOfBufferFile; }

    int32_t GetLocalFileSize() const { return mLocalFileSize; }

    const std::string& GetBufferFilePath() const { return mBufferFilePath; }

    int32_t GetSendRequestConcurrency() const { return mSendRequestConcurrency; }

    int32_t GetProcessThreadCount() const { return mProcessThreadCount; }

    const std::string& GetMappingConfigPath() const { return mMappingConfigPath; }

    const std::string& GetUserConfigPath() const { return mUserConfigPath; }

    const std::string& GetLocalUserConfigPath() const { return mUserLocalConfigPath; }

    const std::string& GetLocalUserConfigDirPath() const { return mUserLocalConfigDirPath; }

    const std::string& GetLocalUserYamlConfigDirPath() const { return mUserLocalYamlConfigDirPath; }

    bool IgnoreDirInodeChanged() const { return mIgnoreDirInodeChanged; }

    void SetProcessExecutionDir(const std::string& dir) { mProcessExecutionDir = dir; }

    const std::string& GetProcessExecutionDir() { return mProcessExecutionDir; }

    void SetWorkingDir(const std::string& dir) { mWorkingDir = dir; }

    const std::string& GetWorkingDir() const { return mWorkingDir; }

    bool IsContainerMode() const { return mContainerMode; }

    const std::string& GetContainerMountConfigPath() const { return mContainerMountConfigPath; }

    const std::string& GetConfigIP() const { return mConfigIP; }

    const std::string& GetCustomizedConfigIp() const { return mCustomizedConfigIP; }

    const std::string& GetConfigHostName() const { return mConfigHostName; }

    int32_t GetSystemBootTime() const { return mSystemBootTime; }

    const std::string& GetDockerFilePathConfig() const { return mDockerFilePathConfig; }

    int32_t GetDataServerPort() const { return mSendDataPort; }

    bool ShennongSocketEnabled() const { return mShennongSocket; }

    const std::vector<sls_logs::LogTag>& GetEnvTags() const { return mEnvTags; }

    bool IsPurageContainerMode() const { return mPurageContainerMode; }

    int32_t GetForceQuitReadTimeout() const { return mForceQuitReadTimeout; }

    const std::string& GetAlipayZone() const { return mAlipayZone; }

    // If @dirPath is not accessible, GetProcessExecutionDir will be set.
    void SetLogtailSysConfDir(const std::string& dirPath);

    const std::string& GetLogtailSysConfDir() const { return mLogtailSysConfDir; }

    inline bool IsHostIPReplacePolicyEnabled() const { return mEnableHostIPReplace; }

    inline bool IsResponseVerificationEnabled() const { return mEnableResponseVerification; }

    EndpointAddressType GetConfigServerAddressNetType() const { return mConfigServerAddressNetType; }

    inline bool EnableCheckpointSyncWrite() const { return mEnableCheckpointSyncWrite; }

    inline bool EnableLogTimeAutoAdjust() const { return mEnableLogTimeAutoAdjust; }

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderUnittest;
    friend class ConfigUpdatorUnittest;
    friend class MultiServerConfigUpdatorUnitest;
    friend class UtilUnittest;
    friend class AppConfigUnittest;
#endif
};
} // namespace logtail
#endif
