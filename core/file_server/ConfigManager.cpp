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

#include "ConfigManager.h"

#include <curl/curl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cctype>
#if defined(__linux__)
#include <fnmatch.h>
#include <unistd.h>
#endif
#include <limits.h>
#include <re2/re2.h>

#include <fstream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "app_config/AppConfig.h"
#include "checkpoint/CheckPointManager.h"
#include "common/CompressTools.h"
#include "constants/Constants.h"
#include "common/ErrorUtil.h"
#include "common/ExceptionBase.h"
#include "common/FileSystemUtil.h"
#include "common/HashUtil.h"
#include "common/JsonUtil.h"
#include "common/RuntimeUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/version.h"
#include "file_server/EventDispatcher.h"
#include "file_server/event_handler/EventHandler.h"
#include "file_server/FileServer.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogtailAlarm.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineManager.h"

using namespace std;

// 商业版
// DEFINE_FLAG_BOOL(https_verify_peer, "set CURLOPT_SSL_VERIFYPEER, CURLOPT_SSL_VERIFYHOST option for libcurl", true);
// #if defined(__linux__) || defined(__APPLE__)
// DEFINE_FLAG_STRING(https_ca_cert, "set CURLOPT_CAINFO for libcurl", "ca-bundle.crt");
// #elif defined(_MSC_VER)
// DEFINE_FLAG_STRING(https_ca_cert, "set CURLOPT_CAINFO for libcurl", "cacert.pem");
// #endif
// DEFINE_FLAG_INT32(request_access_key_interval, "control the frenquency of GetAccessKey, seconds", 60);
// DEFINE_FLAG_INT32(logtail_sys_conf_update_interval, "control the frenquency of load local machine conf, seconds",
// 60); DEFINE_FLAG_BOOL(logtail_config_update_enable, "", true);

// 废弃
// DEFINE_FLAG_STRING(default_global_topic, "default is empty string", "");
// DEFINE_FLAG_STRING(default_data_integrity_project, "default data integrity project", "data_integrity");
// DEFINE_FLAG_STRING(default_data_integrity_log_store, "default data integrity log store", "data_integrity");
// DEFINE_FLAG_INT32(default_data_integrity_time_pos, "default data integrity time pos", 0);
// DEFINE_FLAG_STRING(default_log_time_reg,
//                    "default log time reg",
//                    "([0-9]{4})-(0[0-9]{1}|1[0-2])-(0[0-9]{1}|[12][0-9]{1}|3[01]) "
//                    "(0[0-9]{1}|1[0-9]{1}|2[0-3]):[0-5][0-9]{1}:([0-5][0-9]{1})");
// DEFINE_FLAG_STRING(default_line_count_project, "default line count project", "line_count");
// DEFINE_FLAG_STRING(default_line_count_log_store, "default line count log store", "line_count");
// DEFINE_FLAG_BOOL(default_fuse_mode, "default fuse mode", false);
// DEFINE_FLAG_BOOL(default_mark_offset_flag, "default mark offset flag", false);
// DEFINE_FLAG_BOOL(default_check_ulogfs_env, "default check ulogfs env", false);
// DEFINE_FLAG_INT32(default_max_depth_from_root, "default max depth from root", 1000);
// DEFINE_FLAG_STRING(fuse_customized_config_name,
//                    "name of specified config for fuse, should not be used by user",
//                    "__FUSE_CUSTOMIZED_CONFIG__");

// 移动
// DEFINE_FLAG_STRING(profile_project_name, "profile project_name for logtail", "");
// DEFINE_FLAG_STRING(ilogtail_docker_file_path_config, "ilogtail docker path config file", "docker_path_config.json");
// DEFINE_FLAG_INT32(default_plugin_log_queue_size, "", 10);

DEFINE_FLAG_INT32(wildcard_max_sub_dir_count, "", 1000);
DEFINE_FLAG_INT32(config_match_max_cache_size, "", 1000000);
DEFINE_FLAG_INT32(multi_config_alarm_interval, "second", 600);

DEFINE_FLAG_STRING(ilogtail_docker_path_version, "ilogtail docker path config file", "0.1.0");
DEFINE_FLAG_INT32(max_docker_config_update_times, "max times docker config update in 3 minutes", 10);
DEFINE_FLAG_INT32(docker_config_update_interval, "interval between docker config updates, seconds", 3);

namespace logtail {

// 
ParseConfResult ParseConfig(const std::string& configName, Json::Value& jsonRoot) {
    // Get full path, if it is a relative path, prepend process execution dir.
    std::string fullPath = configName;
    if (IsRelativePath(fullPath)) {
        fullPath = GetProcessExecutionDir() + configName;
    }

    ifstream is;
    is.open(fullPath.c_str());
    if (!is.good()) {
        return CONFIG_NOT_EXIST;
    }
    std::string buffer((std::istreambuf_iterator<char>(is)), (std::istreambuf_iterator<char>()));
    if (!IsValidJson(buffer.c_str(), buffer.length())) {
        return CONFIG_INVALID_FORMAT;
    }

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> jsonReader(builder.newCharReader());
    std::string jsonParseErrs;
    if (!jsonReader->parse(buffer.data(), buffer.data() + buffer.size(), &jsonRoot, &jsonParseErrs)) {
        LOG_WARNING(sLogger, ("ConfigName", configName)("ParseConfig error", jsonParseErrs));
        return CONFIG_INVALID_FORMAT;
    }
    return CONFIG_OK;
}

// dir which is not timeout will be registerd recursively
// if checkTimeout, will not register the dir which is timeout
// if not checkTimeout, will register the dir which is timeout and add it to the timeout list
bool ConfigManager::RegisterHandlersRecursively(const std::string& path,
                                                const FileDiscoveryConfig& config,
                                                bool checkTimeout) {
    if (AppConfig::GetInstance()->IsHostPathMatchBlacklist(path)) {
        LOG_INFO(sLogger, ("ignore path matching host path blacklist", path));
        return false;
    }
    bool result = false;
    if (checkTimeout && config.first->IsTimeout(path))
        return result;

    if (!config.first->IsDirectoryInBlacklist(path))
        result = EventDispatcher::GetInstance()->RegisterEventHandler(path.c_str(), config, mSharedHandler);

    if (!result)
        return result;

    fsutil::Dir dir(path);
    if (!dir.Open()) {
        auto err = GetErrno();
        LogtailAlarm::GetInstance()->SendAlarm(LOGDIR_PERMINSSION_ALARM,
                                               string("Failed to open dir : ") + path + ";\terrno : " + ToString(err),
                                               config.second->GetProjectName(),
                                               config.second->GetLogstoreName(),
                                               config.second->GetRegion());

        LOG_ERROR(sLogger, ("Open dir fail", path.c_str())("errno", ErrnoToString(err)));
        return false;
    }
    fsutil::Entry ent;
    while ((ent = dir.ReadNext())) {
        string item = PathJoin(path, ent.Name());
        if (ent.IsDir()) {
            if (false == RegisterHandlersRecursively(item, config, checkTimeout)) {
                result = false;
            }
        }
    }
    return result;
}

ConfigManager::ConfigManager() {
    // mEnvFlag = false;
    // mStartTime = time(NULL);
    // mRemoveConfigFlag = false;
    // mHaveMappingPathConfig = false;
    // mMappingPathsChanged = false;
    mSharedHandler = NULL;
    // mThreadIsRunning = true;
    // mUpdateStat = NORMAL;
    // mRegionType = REGION_CORP;

    // mLogtailSysConfUpdateTime = 0;
    // mUserDefinedId.clear();
    // mUserDefinedIdSet.clear();
    // mAliuidSet.clear();

    // SetDefaultPubAccessKeyId(STRING_FLAG(default_access_key_id));
    // SetDefaultPubAccessKey(STRING_FLAG(default_access_key));
    // SetDefaultPubAliuid("");
    // SetUserAK(STRING_FLAG(logtail_profile_aliuid),
    //           STRING_FLAG(logtail_profile_access_key_id),
    //           STRING_FLAG(logtail_profile_access_key));
    srand(time(NULL));
    // CorrectionLogtailSysConfDir(); // first create dir then rewrite system-uuid file in GetSystemUUID
    // use a thread to get uuid, work around for CalculateDmiUUID hang
    // mUUID = CalculateDmiUUID();
    // mInstanceId = CalculateRandomUUID() + "_" + LogFileProfiler::mIpAddr + "_" + ToString(time(NULL));
    // ReloadMappingConfig();
}

ConfigManager::~ConfigManager() {
    // unordered_map<string, Config*>::iterator itr = mNameConfigMap.begin();
    // for (; itr != mNameConfigMap.end(); ++itr) {
    //     delete itr->second;
    // }

    unordered_map<std::string, EventHandler*>::iterator itr1 = mDirEventHandlerMap.begin();
    for (; itr1 != mDirEventHandlerMap.end(); ++itr1) {
        if (itr1->second != mSharedHandler)
            delete itr1->second;
    }
    mDirEventHandlerMap.clear();
    delete mSharedHandler;
    // mThreadIsRunning = false;
    // try {
    //     if (mUUIDthreadPtr.get() != NULL)
    //         mUUIDthreadPtr->GetValue(100);
    // } catch (...) {
    // }
}

void ConfigManager::RemoveHandler(const string& dir, bool delete_flag) {
    unordered_map<string, EventHandler*>::iterator itr = mDirEventHandlerMap.find(dir);
    if (itr != mDirEventHandlerMap.end()) {
        if (itr->second != mSharedHandler && delete_flag) {
            delete itr->second;
        }
        mDirEventHandlerMap.erase(itr);
    }
}

// this functions should only be called when register base dir
bool ConfigManager::RegisterHandlers() {
    if (mSharedHandler == NULL) {
        mSharedHandler = new NormalEventHandler();
    }
    vector<FileDiscoveryConfig> sortedConfigs;
    vector<FileDiscoveryConfig> wildcardConfigs;
    auto nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    for (auto itr = nameConfigMap.begin(); itr != nameConfigMap.end(); ++itr) {
        if (itr->second.first->GetWildcardPaths().empty())
            sortedConfigs.push_back(itr->second);
        else
            wildcardConfigs.push_back(itr->second);
    }
    sort(sortedConfigs.begin(), sortedConfigs.end(), FileDiscoveryOptions::CompareByPathLength);
    bool result = true;
    for (auto itr = sortedConfigs.begin(); itr != sortedConfigs.end(); ++itr) {
        const FileDiscoveryOptions* config = itr->first;
        if (!config->IsContainerDiscoveryEnabled()) {
            result &= RegisterHandlers(config->GetBasePath(), *itr);
        } else {
            for (size_t i = 0; i < config->GetContainerInfo()->size(); ++i) {
                result &= RegisterHandlers((*config->GetContainerInfo())[i].mRealBaseDir, *itr);
            }
        }
    }
    for (auto itr = wildcardConfigs.begin(); itr != wildcardConfigs.end(); ++itr) {
        const FileDiscoveryOptions* config = itr->first;
        if (!config->IsContainerDiscoveryEnabled()) {
            RegisterWildcardPath(*itr, config->GetWildcardPaths()[0], 0);
        } else {
            for (size_t i = 0; i < config->GetContainerInfo()->size(); ++i) {
                RegisterWildcardPath(*itr, (*config->GetContainerInfo())[i].mRealBaseDir, 0);
            }
        }
    }
    return result;
}

void ConfigManager::RegisterWildcardPath(const FileDiscoveryConfig& config, const std::string& path, int32_t depth) {
    if (AppConfig::GetInstance()->IsHostPathMatchBlacklist(path)) {
        LOG_INFO(sLogger, ("ignore path matching host path blacklist", path));
        return;
    }
    bool finish;
    if ((depth + 1) == ((int)config.first->GetWildcardPaths().size() - 1))
        finish = true;
    else if ((depth + 1) < ((int)config.first->GetWildcardPaths().size() - 1))
        finish = false;
    else
        return;

    // const path
    if (!config.first->GetConstWildcardPaths()[depth].empty()) {
        // stat directly
        string item = PathJoin(path, config.first->GetConstWildcardPaths()[depth]);
        fsutil::PathStat baseDirStat;
        if (!fsutil::PathStat::stat(item, baseDirStat)) {
            LOG_DEBUG(sLogger,
                      ("get wildcard dir info error: ",
                       item)(config.second->GetProjectName(),
                             config.second->GetLogstoreName())("error", ErrnoToString(GetErrno())));
            return;
        }
        if (!baseDirStat.IsDir())
            return;
        if (finish) {
            DirRegisterStatus registerStatus = EventDispatcher::GetInstance()->IsDirRegistered(item);
            if (registerStatus == GET_REGISTER_STATUS_ERROR) {
                return;
            }
            if (EventDispatcher::GetInstance()->RegisterEventHandler(item.c_str(), config, mSharedHandler)) {
                RegisterDescendants(
                    item, config, config.first->mMaxDirSearchDepth < 0 ? 100 : config.first->mMaxDirSearchDepth);
            }
        } else {
            RegisterWildcardPath(config, item, depth + 1);
        }
        return;
    }

    fsutil::Dir dir(path);
    if (!dir.Open()) {
        auto err = GetErrno();
        LogtailAlarm::GetInstance()->SendAlarm(LOGDIR_PERMINSSION_ALARM,
                                               string("Failed to open dir : ") + path + ";\terrno : " + ToString(err),
                                               config.second->GetProjectName(),
                                               config.second->GetLogstoreName(),
                                               config.second->GetRegion());
        LOG_WARNING(sLogger, ("Open dir fail", path.c_str())("errno", err));
        return;
    }
    fsutil::Entry ent;
    int32_t dirCount = 0;
    while ((ent = dir.ReadNext())) {
        if (dirCount >= INT32_FLAG(wildcard_max_sub_dir_count)) {
            LOG_WARNING(sLogger,
                        ("too many sub directoried for path", path)("dirCount", dirCount)("basePath",
                                                                                          config.first->GetBasePath()));
            LogtailAlarm::GetInstance()->SendAlarm(STAT_LIMIT_ALARM,
                                                   string("too many sub directoried for path:" + path
                                                          + " dirCount: " + ToString(dirCount) + " basePath"
                                                          + config.first->GetBasePath()),
                                                   config.second->GetProjectName(),
                                                   config.second->GetLogstoreName(),
                                                   config.second->GetRegion());
            break;
        }
        if (!ent.IsDir())
            continue;

        ++dirCount;
        string item = PathJoin(path, ent.Name());

        // we should check match and then check finsh
        size_t dirIndex = config.first->GetWildcardPaths()[depth].size() + 1;
#if defined(_MSC_VER)
        // Backward compatibility: the inner condition never happen.
        if (!BOOL_FLAG(enable_root_path_collection)) {
            if (dirIndex == (size_t)2) {
                dirIndex = 1;
            }
        }
        // For wildcard Windows path C:\*, mWildcardPaths[0] will be C:\,
        //   so dirIndex should be adjusted by minus 1.
        else if (0 == depth) {
            const auto& firstWildcardPath = config.first->GetWildcardPaths()[0];
            const auto pathSize = firstWildcardPath.size();
            if (pathSize >= 2 && firstWildcardPath[pathSize - 1] == PATH_SEPARATOR[0]
                && firstWildcardPath[pathSize - 2] == ':') {
                --dirIndex;
            }
        }
#else
        // if mWildcardPaths[depth] is '/', we should set dirIndex = 1
        if (dirIndex == (size_t)2) {
            dirIndex = 1;
        }
#endif

        if (fnmatch(&(config.first->GetWildcardPaths()[depth + 1].at(dirIndex)), ent.Name().c_str(), FNM_PATHNAME)
            == 0) {
            if (finish) {
                DirRegisterStatus registerStatus = EventDispatcher::GetInstance()->IsDirRegistered(item);
                if (registerStatus == GET_REGISTER_STATUS_ERROR) {
                    return;
                }
                if (EventDispatcher::GetInstance()->RegisterEventHandler(item.c_str(), config, mSharedHandler)) {
                    RegisterDescendants(
                        item, config, config.first->mMaxDirSearchDepth < 0 ? 100 : config.first->mMaxDirSearchDepth);
                }
            } else {
                RegisterWildcardPath(config, item, depth + 1);
            }
        }
    }
}

// this functions should only be called when register base dir
bool ConfigManager::RegisterHandlers(const string& basePath, const FileDiscoveryConfig& config) {
    bool result = true;
    // static set<string> notExistDirs;
    // static int32_t lastErrorLogTime = time(NULL);
    // if (notExistDirs.size() > 0 && (time(NULL) - lastErrorLogTime > 3600)) {
    //     string dirs;
    //     for (set<string>::iterator iter = notExistDirs.begin(); iter != notExistDirs.end(); ++iter) {
    //         dirs.append(*iter);
    //         dirs.append(" ");
    //     }
    //     lastErrorLogTime = time(NULL);
    //     notExistDirs.clear();
    //     LOG_WARNING(sLogger, ("logPath in config not exist", dirs));
    // }
    if (!CheckExistance(basePath)) {
        // if (!(config->mLogType == APSARA_LOG && basePath.find("/tmp") == 0)
        //     && basePath.find(LOG_LOCAL_DEFINED_PATH_PREFIX) != 0)
        //     notExistDirs.insert(basePath);
        if (EventDispatcher::GetInstance()->IsRegistered(basePath.c_str())) {
            EventDispatcher::GetInstance()->UnregisterAllDir(basePath);
            LOG_DEBUG(sLogger, ("logPath in config not exist, unregister existed monitor", basePath));
        }
        return result;
    }
    DirRegisterStatus registerStatus = EventDispatcher::GetInstance()->IsDirRegistered(basePath);
    if (registerStatus == GET_REGISTER_STATUS_ERROR)
        return result;
    // dir in config is valid by default, do not call pathValidator
    result = EventDispatcher::GetInstance()->RegisterEventHandler(basePath.c_str(), config, mSharedHandler);
    // if we come into a failure, do not try to register others, there must be something wrong!
    if (!result)
        return result;

    if (config.first->mPreservedDirDepth < 0)
        result = RegisterDescendants(
            basePath, config, config.first->mMaxDirSearchDepth < 0 ? 100 : config.first->mMaxDirSearchDepth);
    else {
        // preserve_depth register
        int depth = config.first->mPreservedDirDepth;
        result = RegisterHandlersWithinDepth(basePath, config, depth);
    }
    return result;
}

bool ConfigManager::RegisterDirectory(const std::string& source, const std::string& object) {
    // TODO��A potential bug: FindBestMatch will test @object with filePattern, which has very
    // low possibility to match a sub directory name, so here will return false in most cases.
    // e.g.: source: /path/to/monitor, file pattern: *.log, object: subdir.
    // Match(subdir, *.log) = false.
    FileDiscoveryConfig config = FindBestMatch(source, object);
    if (config.first && !config.first->IsDirectoryInBlacklist(source))
        return EventDispatcher::GetInstance()->RegisterEventHandler(source.c_str(), config, mSharedHandler);
    return false;
}

bool ConfigManager::RegisterHandlersWithinDepth(const std::string& path, const FileDiscoveryConfig& config, int depth) {
    if (AppConfig::GetInstance()->IsHostPathMatchBlacklist(path)) {
        LOG_INFO(sLogger, ("ignore path matching host path blacklist", path));
        return false;
    }
    if (depth <= 0) {
        DirCheckPointPtr dirCheckPoint;
        if (CheckPointManager::Instance()->GetDirCheckPoint(path, dirCheckPoint) == false)
            return true;
        // path had dircheckpoint means it was watched before, so it is valid
        const set<string>& subdir = dirCheckPoint.get()->mSubDir;
        for (set<string>::iterator it = subdir.begin(); it != subdir.end(); it++) {
            if (EventDispatcher::GetInstance()->RegisterEventHandler((*it).c_str(), config, mSharedHandler))
                RegisterHandlersWithinDepth(*it, config, depth - 1);
        }
        return true;
    }
    bool result = true;

    fsutil::Dir dir(path);
    if (!dir.Open()) {
        int err = GetErrno();
        LogtailAlarm::GetInstance()->SendAlarm(LOGDIR_PERMINSSION_ALARM,
                                               string("Failed to open dir : ") + path + ";\terrno : " + ToString(err),
                                               config.second->GetProjectName(),
                                               config.second->GetLogstoreName(),
                                               config.second->GetRegion());

        LOG_ERROR(sLogger, ("Open dir error: ", path.c_str())("errno", err));
        return false;
    }
    fsutil::Entry ent;
    while ((ent = dir.ReadNext())) {
        string item = PathJoin(path, ent.Name());
        if (ent.IsDir() && !config.first->IsDirectoryInBlacklist(item)) {
            if (!(EventDispatcher::GetInstance()->RegisterEventHandler(item.c_str(), config, mSharedHandler))) {
                // break;// fail early, do not try to register others
                result = false;
            } else // sub dir will not be registered if parent dir fails
                RegisterHandlersWithinDepth(item, config, depth - 1);
        }
    }

    return result;
}

// path not terminated by '/', path already registered
bool ConfigManager::RegisterDescendants(const string& path, const FileDiscoveryConfig& config, int withinDepth) {
    if (AppConfig::GetInstance()->IsHostPathMatchBlacklist(path)) {
        LOG_INFO(sLogger, ("ignore path matching host path blacklist", path));
        return false;
    }
    if (withinDepth <= 0) {
        return true;
    }

    fsutil::Dir dir(path);
    if (!dir.Open()) {
        auto err = GetErrno();
        LogtailAlarm::GetInstance()->SendAlarm(LOGDIR_PERMINSSION_ALARM,
                                               string("Failed to open dir : ") + path + ";\terrno : " + ToString(err),
                                               config.second->GetProjectName(),
                                               config.second->GetLogstoreName(),
                                               config.second->GetRegion());
        LOG_ERROR(sLogger, ("Open dir error: ", path.c_str())("errno", err));
        return false;
    }
    fsutil::Entry ent;
    bool result = true;
    while ((ent = dir.ReadNext())) {
        string item = PathJoin(path, ent.Name());
        if (ent.IsDir() && !config.first->IsDirectoryInBlacklist(item)) {
            result = EventDispatcher::GetInstance()->RegisterEventHandler(item.c_str(), config, mSharedHandler);
            if (result)
                RegisterDescendants(item, config, withinDepth - 1);
        }
    }
    return result;
}

void ConfigManager::ClearConfigMatchCache() {
    static const int32_t FORCE_CLEAR_INTERVAL = 6 * 3600;
    static int32_t s_lastClearTime = (int32_t)time(NULL) - rand() % 600;
    static int32_t s_lastClearAllTime = (int32_t)time(NULL) - rand() % 600;
    int32_t curTime = (int32_t)time(NULL);

    ScopedSpinLock cachedLock(mCacheFileConfigMapLock);
    if (curTime - s_lastClearTime > FORCE_CLEAR_INTERVAL
        || mCacheFileConfigMap.size() > (size_t)INT32_FLAG(config_match_max_cache_size)) {
        s_lastClearTime = curTime;
        mCacheFileConfigMap.clear();
    }

    ScopedSpinLock allCachedLock(mCacheFileAllConfigMapLock);
    if (curTime - s_lastClearAllTime > FORCE_CLEAR_INTERVAL
        || mCacheFileAllConfigMap.size() > (size_t)INT32_FLAG(config_match_max_cache_size)) {
        s_lastClearAllTime = curTime;
        mCacheFileAllConfigMap.clear();
    }
}

FileDiscoveryConfig ConfigManager::FindBestMatch(const string& path, const string& name) {
    string cachedFileKey(path);
    cachedFileKey.push_back('<');
    cachedFileKey.append(name);
    bool acceptMultiConfig = AppConfig::GetInstance()->IsAcceptMultiConfig();
    {
        ScopedSpinLock cachedLock(mCacheFileConfigMapLock);
        auto iter = mCacheFileConfigMap.find(cachedFileKey);
        if (iter != mCacheFileConfigMap.end()) {
            // if need report alarm, do not return, just continue to find all match and send alarm
            if (acceptMultiConfig || iter->second.second == 0
                || time(NULL) - iter->second.second < INT32_FLAG(multi_config_alarm_interval)) {
                return iter->second.first;
            }
        }
    }
    const auto& nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    auto itr = nameConfigMap.begin();
    FileDiscoveryConfig prevMatch(nullptr, nullptr);
    size_t prevLen = 0;
    size_t curLen = 0;
    uint32_t nameRepeat = 0;
    string logNameList;
    vector<FileDiscoveryConfig> multiConfigs;
    for (; itr != nameConfigMap.end(); ++itr) {
        const FileDiscoveryOptions* config = itr->second.first;
        // // exclude __FUSE_CONFIG__
        // if (itr->first == STRING_FLAG(fuse_customized_config_name)) {
        //     continue;
        // }

        bool match = config->IsMatch(path, name);
        if (match) {
            // if force multi config, do not send alarm
            if (!name.empty() && !config->mAllowingIncludedByMultiConfigs) {
                nameRepeat++;
                logNameList.append("logstore:");
                logNameList.append(itr->second.second->GetLogstoreName());
                logNameList.append(",config:");
                logNameList.append(itr->second.second->GetConfigName());
                logNameList.append(" ");
                multiConfigs.push_back(itr->second);
            }

            // note: best config is the one which length is longest and create time is nearest
            curLen = config->GetBasePath().size();
            if (prevLen < curLen) {
                prevMatch = itr->second;
                prevLen = curLen;
            } else if (prevLen == curLen && prevMatch.first) {
                if (prevMatch.second->GetCreateTime() > itr->second.second->GetCreateTime()) {
                    prevMatch = itr->second;
                    prevLen = curLen;
                }
            }
        }
    }

    // file include in multi config, find config for path will not trigger this alarm
    // do not send alarm to config with mForceMultiConfig
    if (nameRepeat > 1 && !name.empty() && !acceptMultiConfig) {
        LOG_ERROR(sLogger,
                  ("file", path + '/' + name)("include in multi config",
                                              logNameList)("best", prevMatch.second->GetConfigName()));
        for (auto iter = multiConfigs.begin(); iter != multiConfigs.end(); ++iter) {
            LogtailAlarm::GetInstance()->SendAlarm(
                MULTI_CONFIG_MATCH_ALARM,
                path + '/' + name + " include in multi config, best and oldest match: "
                    + prevMatch.second->GetProjectName() + ',' + prevMatch.second->GetLogstoreName() + ','
                    + prevMatch.second->GetConfigName() + ", allmatch: " + logNameList,
                (*iter).second->GetProjectName(),
                (*iter).second->GetLogstoreName(),
                (*iter).second->GetRegion());
        }
    }
    {
        ScopedSpinLock cachedLock(mCacheFileConfigMapLock);
        // use operator [], force update time
        mCacheFileConfigMap[cachedFileKey]
            = std::make_pair(prevMatch, nameRepeat > 1 && !name.empty() ? (int32_t)time(NULL) : (int32_t)0);
    }
    return prevMatch;
}


int32_t ConfigManager::FindAllMatch(vector<FileDiscoveryConfig>& allConfig,
                                    const std::string& path,
                                    const std::string& name /*= ""*/) {
    static AppConfig* appConfig = AppConfig::GetInstance();
    string cachedFileKey(path);
    cachedFileKey.push_back('<');
    cachedFileKey.append(name);
    const int32_t maxMultiConfigSize = appConfig->GetMaxMultiConfigSize();
    {
        ScopedSpinLock cachedLock(mCacheFileAllConfigMapLock);
        auto iter = mCacheFileAllConfigMap.find(cachedFileKey);
        if (iter != mCacheFileAllConfigMap.end()) {
            if (iter->second.second == 0
                || time(NULL) - iter->second.second < INT32_FLAG(multi_config_alarm_interval)) {
                allConfig = iter->second.first;
                return (int32_t)allConfig.size();
            }
        }
    }
    bool alarmFlag = false;
    auto nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    auto itr = nameConfigMap.begin();
    for (; itr != nameConfigMap.end(); ++itr) {
        const FileDiscoveryOptions* config = itr->second.first;
        // // exclude __FUSE_CONFIG__
        // if (itr->first == STRING_FLAG(fuse_customized_config_name)) {
        //     continue;
        // }

        bool match = config->IsMatch(path, name);
        if (match) {
            allConfig.push_back(itr->second);
        }
    }

    if (!name.empty() && allConfig.size() > static_cast<size_t>(maxMultiConfigSize)) {
        // only report log file alarm
        alarmFlag = true;
        sort(allConfig.begin(), allConfig.end(), FileDiscoveryOptions::CompareByDepthAndCreateTime);
        SendAllMatchAlarm(path, name, allConfig, maxMultiConfigSize);
        allConfig.resize(maxMultiConfigSize);
    }
    {
        ScopedSpinLock cachedLock(mCacheFileAllConfigMapLock);
        // use operator [], force update time
        mCacheFileAllConfigMap[cachedFileKey] = std::make_pair(allConfig, alarmFlag ? (int32_t)time(NULL) : (int32_t)0);
    }
    return (int32_t)allConfig.size();
}

int32_t ConfigManager::FindMatchWithForceFlag(std::vector<FileDiscoveryConfig>& allConfig,
                                              const string& path,
                                              const string& name) {
    const bool acceptMultiConfig = AppConfig::GetInstance()->IsAcceptMultiConfig();
    string cachedFileKey(path);
    cachedFileKey.push_back('<');
    cachedFileKey.append(name);
    {
        ScopedSpinLock cachedLock(mCacheFileAllConfigMapLock);
        auto iter = mCacheFileAllConfigMap.find(cachedFileKey);
        if (iter != mCacheFileAllConfigMap.end()) {
            if (iter->second.second == 0
                || time(NULL) - iter->second.second < INT32_FLAG(multi_config_alarm_interval)) {
                allConfig = iter->second.first;
                return (int32_t)allConfig.size();
            }
        }
    }
    auto nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    auto itr = nameConfigMap.begin();
    FileDiscoveryConfig prevMatch = make_pair(nullptr, nullptr);
    size_t prevLen = 0;
    size_t curLen = 0;
    uint32_t nameRepeat = 0;
    string logNameList;
    vector<FileDiscoveryConfig> multiConfigs;
    for (; itr != nameConfigMap.end(); ++itr) {
        FileDiscoveryConfig config = itr->second;
        // // exclude __FUSE_CONFIG__
        // if (itr->first == STRING_FLAG(fuse_customized_config_name)) {
        //     continue;
        // }

        bool match = config.first->IsMatch(path, name);
        if (match) {
            // if force multi config, do not send alarm
            if (!name.empty() && !config.first->mAllowingIncludedByMultiConfigs) {
                nameRepeat++;
                logNameList.append("logstore:");
                logNameList.append(config.second->GetLogstoreName());
                logNameList.append(",config:");
                logNameList.append(config.second->GetConfigName());
                logNameList.append(" ");
                multiConfigs.push_back(config);
            }
            if (!config.first->mAllowingIncludedByMultiConfigs) {
                // if not ForceMultiConfig, find best match in normal cofigs
                // note: best config is the one which length is longest and create time is nearest
                curLen = config.first->GetBasePath().size();
                if (prevLen < curLen) {
                    prevMatch = config;
                    prevLen = curLen;
                } else if (prevLen == curLen && prevMatch.first) {
                    if (prevMatch.second->GetCreateTime() > config.second->GetCreateTime()) {
                        prevMatch = config;
                        prevLen = curLen;
                    }
                }
            } else {
                // save ForceMultiConfig
                allConfig.push_back(config);
            }
        }
    }

    bool alarmFlag = false;
    // file include in multi config, find config for path will not trigger this alarm
    // do not send alarm to config with mForceMultiConfig
    if (nameRepeat > 1 && !acceptMultiConfig && prevMatch.first) {
        alarmFlag = true;
        LOG_ERROR(sLogger,
                  ("file", path + '/' + name)("include in multi config",
                                              logNameList)("best", prevMatch.second->GetConfigName()));
        for (auto iter = multiConfigs.begin(); iter != multiConfigs.end(); ++iter) {
            LogtailAlarm::GetInstance()->SendAlarm(
                MULTI_CONFIG_MATCH_ALARM,
                path + '/' + name + " include in multi config, best and oldest match: "
                    + prevMatch.second->GetProjectName() + ',' + prevMatch.second->GetLogstoreName() + ','
                    + prevMatch.second->GetConfigName() + ", allmatch: " + logNameList,
                (*iter).second->GetProjectName(),
                (*iter).second->GetLogstoreName(),
                (*iter).second->GetRegion());
        }
    }
    if (prevMatch.first) {
        allConfig.push_back(prevMatch);
    }
    {
        ScopedSpinLock cachedLock(mCacheFileAllConfigMapLock);
        // use operator [], force update time
        mCacheFileAllConfigMap[cachedFileKey] = std::make_pair(allConfig, alarmFlag ? (int32_t)time(NULL) : (int32_t)0);
    }
    return (int32_t)allConfig.size();
}

void ConfigManager::SendAllMatchAlarm(const string& path,
                                      const string& name,
                                      vector<FileDiscoveryConfig>& allConfig,
                                      int32_t maxMultiConfigSize) {
    string allConfigNames;
    for (auto iter = allConfig.begin(); iter != allConfig.end(); ++iter) {
        allConfigNames.append("[")
            .append((*iter).second->GetProjectName())
            .append(" : ")
            .append((*iter).second->GetLogstoreName())
            .append(" : ")
            .append((*iter).second->GetConfigName())
            .append("]");
    }
    LOG_ERROR(sLogger,
              ("file", path + '/' + name)("include in too many configs", allConfig.size())(
                  "max multi config size", maxMultiConfigSize)("allconfigs", allConfigNames));
    for (auto iter = allConfig.begin(); iter != allConfig.end(); ++iter)
        LogtailAlarm::GetInstance()->SendAlarm(
            TOO_MANY_CONFIG_ALARM,
            path + '/' + name + " include in too many configs:" + ToString(allConfig.size())
                + ", max multi config size : " + ToString(maxMultiConfigSize) + ", allmatch: " + allConfigNames,
            (*iter).second->GetProjectName(),
            (*iter).second->GetLogstoreName(),
            (*iter).second->GetRegion());
}

void ConfigManager::AddHandlerToDelete(EventHandler* handler) {
    mHandlersToDelete.push_back(handler);
}

void ConfigManager::DeleteHandlers() {
    for (size_t i = 0; i < mHandlersToDelete.size(); ++i) {
        if (mHandlersToDelete[i] != mSharedHandler && mHandlersToDelete[i] != NULL)
            delete mHandlersToDelete[i];
    }
    mHandlersToDelete.clear();
}

// GetRelatedConfigs calculates related configs of @path.
// Two kind of relations:
// 1. No wildcard path: the base path of Config is the prefix of @path and within depth.
// 2. Wildcard path: @path matches and within depth.
void ConfigManager::GetRelatedConfigs(const std::string& path, std::vector<FileDiscoveryConfig>& configs) {
    const auto& nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    for (auto iter = nameConfigMap.begin(); iter != nameConfigMap.end(); ++iter) {
        if (iter->second.first->IsMatch(path, "")) {
            configs.push_back(iter->second);
        }
    }
}

bool ConfigManager::UpdateContainerPath(ConfigContainerInfoUpdateCmd* cmd) {
    mContainerInfoCmdLock.lock();
    mContainerInfoCmdVec.push_back(cmd);
    mContainerInfoCmdLock.unlock();
    return true;
}

bool ConfigManager::DoUpdateContainerPaths() {
    mContainerInfoCmdLock.lock();
    std::vector<ConfigContainerInfoUpdateCmd*> tmpPathCmdVec = mContainerInfoCmdVec;
    mContainerInfoCmdVec.clear();
    mContainerInfoCmdLock.unlock();
    LOG_INFO(sLogger, ("update container path", tmpPathCmdVec.size()));
    for (size_t i = 0; i < tmpPathCmdVec.size(); ++i) {
        FileDiscoveryConfig config = FileServer::GetInstance()->GetFileDiscoveryConfig(tmpPathCmdVec[i]->mConfigName);
        if (!config.first) {
            LOG_ERROR(sLogger,
                      ("invalid container path update cmd",
                       tmpPathCmdVec[i]->mConfigName)("params", tmpPathCmdVec[i]->mJsonParams.toStyledString()));
            continue;
        }
        if (tmpPathCmdVec[i]->mDeleteFlag) {
            if (config.first->DeleteContainerInfo(tmpPathCmdVec[i]->mJsonParams)) {
                LOG_DEBUG(sLogger,
                          ("container path delete cmd success",
                           tmpPathCmdVec[i]->mConfigName)("params", tmpPathCmdVec[i]->mJsonParams.toStyledString()));
            } else {
                LOG_ERROR(sLogger,
                          ("container path delete cmd fail",
                           tmpPathCmdVec[i]->mConfigName)("params", tmpPathCmdVec[i]->mJsonParams.toStyledString()));
            }
        } else {
            if (config.first->UpdateContainerInfo(tmpPathCmdVec[i]->mJsonParams, config.second)) {
                LOG_DEBUG(sLogger,
                          ("container path update cmd success",
                           tmpPathCmdVec[i]->mConfigName)("params", tmpPathCmdVec[i]->mJsonParams.toStyledString()));
            } else {
                LOG_ERROR(sLogger,
                          ("container path update cmd fail",
                           tmpPathCmdVec[i]->mConfigName)("params", tmpPathCmdVec[i]->mJsonParams.toStyledString()));
            }
        }
        delete tmpPathCmdVec[i];
    }
    return true;
}

bool ConfigManager::IsUpdateContainerPaths() {
    mContainerInfoCmdLock.lock();
    bool rst = false;
    for (size_t i = 0; i < mContainerInfoCmdVec.size(); ++i) {
        ConfigContainerInfoUpdateCmd* pCmd = mContainerInfoCmdVec[i];
        if (pCmd->mDeleteFlag) {
            rst = true;
            break;
        }
        FileDiscoveryConfig pConfig = FileServer::GetInstance()->GetFileDiscoveryConfig(pCmd->mConfigName);
        if (!pConfig.first) {
            continue;
        }
        if (!pConfig.first->IsSameContainerInfo(pCmd->mJsonParams, pConfig.second)) {
            rst = true;
            break;
        }
    }
    if (mContainerInfoCmdVec.size() > 0) {
        LOG_INFO(sLogger, ("check container path update flag", rst)("size", mContainerInfoCmdVec.size()));
    }
    if (rst == false) {
        for (size_t i = 0; i < mContainerInfoCmdVec.size(); ++i) {
            delete mContainerInfoCmdVec[i];
        }
        mContainerInfoCmdVec.clear();
    }
    mContainerInfoCmdLock.unlock();

    /********** qps limit : only update docker config INT32_FLAG(max_docker_config_update_times) times in 3 minutes
     * ********/
    static int32_t s_lastUpdateTime = 0;
    static int32_t s_lastUpdateCount = 0;
    if (!rst) {
        return rst;
    }
    int32_t nowTime = time(NULL);
    // not in 3 minutes window
    if (nowTime / 180 != s_lastUpdateTime / 180) {
        s_lastUpdateCount = 1;
        s_lastUpdateTime = nowTime;
        return rst;
    }
    // this window's update times < INT32_FLAG(max_docker_config_update_times)
    // min interval : 10 seconds
    // For case with frequent container update (eg. K8s short job), adjust this parameter.
    if (s_lastUpdateCount < INT32_FLAG(max_docker_config_update_times)
        && nowTime - s_lastUpdateTime >= INT32_FLAG(docker_config_update_interval)) {
        ++s_lastUpdateCount;
        s_lastUpdateTime = nowTime;
        return rst;
    }
    // return false if s_lastUpdateCount >= INT32_FLAG(max_docker_config_update_times) and last update time is in same
    // window
    return false;
    /************************************************************************************************************************/
}

bool ConfigManager::UpdateContainerStopped(ConfigContainerInfoUpdateCmd* cmd) {
    PTScopedLock lock(mDockerContainerStoppedCmdLock);
    mDockerContainerStoppedCmdVec.push_back(cmd);
    return true;
}

void ConfigManager::GetContainerStoppedEvents(std::vector<Event*>& eventVec) {
    std::vector<ConfigContainerInfoUpdateCmd*> cmdVec;
    {
        PTScopedLock lock(mDockerContainerStoppedCmdLock);
        cmdVec.swap(mDockerContainerStoppedCmdVec);
    }
    for (auto& cmd : cmdVec) {
        // find config and container's path, then emit stopped event
        FileDiscoveryConfig config = FileServer::GetInstance()->GetFileDiscoveryConfig(cmd->mConfigName);
        if (!config.first) {
            continue;
        }
        ContainerInfo containerInfo;
        std::string errorMsg;
        if (!ContainerInfo::ParseByJSONObj(cmd->mJsonParams, containerInfo, errorMsg)) {
            LOG_ERROR(sLogger, ("invalid container info update param", errorMsg)("action", "ignore current cmd"));
            continue;
        }
        std::vector<ContainerInfo>::iterator iter = config.first->GetContainerInfo()->begin();
        std::vector<ContainerInfo>::iterator iend = config.first->GetContainerInfo()->end();
        for (; iter != iend; ++iter) {
            if (iter->mID == containerInfo.mID) {
                break;
            }
        }
        if (iter == iend) {
            continue;
        }
        Event* pStoppedEvent = new Event(iter->mRealBaseDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, -1, 0);
        LOG_DEBUG(
            sLogger,
            ("GetContainerStoppedEvent Type", pStoppedEvent->GetType())("Source", pStoppedEvent->GetSource())(
                "Object", pStoppedEvent->GetObject())("Config", pStoppedEvent->GetConfigName())(
                "IsDir", pStoppedEvent->IsDir())("IsCreate", pStoppedEvent->IsCreate())("IsModify",
                                                                                        pStoppedEvent->IsModify())(
                "IsDeleted", pStoppedEvent->IsDeleted())("IsMoveFrom", pStoppedEvent->IsMoveFrom())(
                "IsContainerStopped", pStoppedEvent->IsContainerStopped())("IsMoveTo", pStoppedEvent->IsMoveTo()));
        eventVec.push_back(pStoppedEvent);
    }
    for (auto cmd : cmdVec) {
        delete cmd;
    }
}

void ConfigManager::SaveDockerConfig() {
    string dockerPathConfigName = AppConfig::GetInstance()->GetDockerFilePathConfig();
    Json::Value dockerPathValueRoot;
    dockerPathValueRoot["version"] = Json::Value(STRING_FLAG(ilogtail_docker_path_version));
    Json::Value dockerPathValueDetail;
    mContainerInfoCmdLock.lock();
    const auto& nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    for (auto it = nameConfigMap.begin(); it != nameConfigMap.end(); ++it) {
        if (it->second.first->GetContainerInfo()) {
            std::vector<ContainerInfo>& containerPathVec = *(it->second.first->GetContainerInfo());
            for (size_t i = 0; i < containerPathVec.size(); ++i) {
                Json::Value dockerPathValue;
                dockerPathValue["config_name"] = Json::Value(it->first);
                dockerPathValue["container_id"] = Json::Value(containerPathVec[i].mID);
                containerPathVec[i].mJson["Path"] = Json::Value(containerPathVec[i].mRealBaseDir);
                dockerPathValue["params"] = Json::Value(containerPathVec[i].mJson.toStyledString());
                dockerPathValueDetail.append(dockerPathValue);
            }
        }
    }
    mContainerInfoCmdLock.unlock();
    dockerPathValueRoot["detail"] = dockerPathValueDetail;
    string dockerInfo = dockerPathValueRoot.toStyledString();
    OverwriteFile(dockerPathConfigName, dockerInfo);
    LOG_INFO(sLogger, ("dump docker path info", dockerPathConfigName));
}

void ConfigManager::LoadDockerConfig() {
    string dockerPathConfigName = AppConfig::GetInstance()->GetDockerFilePathConfig();
    Json::Value dockerPathValueRoot;
    ParseConfResult rst = ParseConfig(dockerPathConfigName, dockerPathValueRoot);
    if (rst == CONFIG_INVALID_FORMAT) {
        LOG_ERROR(sLogger, ("invalid docker config json", rst)("file path", dockerPathConfigName));
    }
    if (rst != CONFIG_OK) {
        return;
    }
    if (!dockerPathValueRoot.isMember("detail")) {
        return;
    }
    Json::Value dockerPathValueDetail = dockerPathValueRoot["detail"];
    if (!dockerPathValueDetail.isArray()) {
        return;
    }
    std::vector<ConfigContainerInfoUpdateCmd*> localPaths;
    for (Json::Value::iterator iter = dockerPathValueDetail.begin(); iter != dockerPathValueDetail.end(); ++iter) {
        const Json::Value& dockerPathItem = *iter;
        string configName = dockerPathItem.isMember("config_name") && dockerPathItem["config_name"].isString()
            ? dockerPathItem["config_name"].asString()
            : string();
        string containerID = dockerPathItem.isMember("container_id") && dockerPathItem["container_id"].isString()
            ? dockerPathItem["container_id"].asString()
            : string();
        string params = dockerPathItem.isMember("params") && dockerPathItem["params"].isString()
            ? dockerPathItem["params"].asString()
            : string();
        LOG_INFO(sLogger, ("load docker path, config", configName)("container id", containerID)("params", params));
        if (configName.empty() || containerID.empty() || params.empty()) {
            continue;
        }

        // cmd 解析json
        Json::Value jsonParams;
        std::string errorMsg;
        if (params.size() < 5UL || !ParseJsonTable(params, jsonParams, errorMsg)) {
            LOG_ERROR(sLogger, ("invalid docker container params", params)("errorMsg", errorMsg));
            continue;
        }
        ConfigContainerInfoUpdateCmd* cmd = new ConfigContainerInfoUpdateCmd(configName, false, jsonParams);
        localPaths.push_back(cmd);
    }
    mContainerInfoCmdLock.lock();
    localPaths.insert(localPaths.end(), mContainerInfoCmdVec.begin(), mContainerInfoCmdVec.end());
    mContainerInfoCmdVec = localPaths;
    mContainerInfoCmdLock.unlock();

    DoUpdateContainerPaths();
}

void ConfigManager::ClearFilePipelineMatchCache() {
    ScopedSpinLock lock(mCacheFileConfigMapLock);
    mCacheFileConfigMap.clear();
    ScopedSpinLock allLock(mCacheFileAllConfigMapLock);
    mCacheFileAllConfigMap.clear();
}

#ifdef APSARA_UNIT_TEST_MAIN
void ConfigManager::CleanEnviroments() {
    for (std::unordered_map<std::string, EventHandler*>::iterator iter = mDirEventHandlerMap.begin();
         iter != mDirEventHandlerMap.end();
         ++iter) {
        if (iter->second != mSharedHandler && iter->second)
            delete iter->second;
    }
    mDirEventHandlerMap.clear();
}
#endif

} // namespace logtail
