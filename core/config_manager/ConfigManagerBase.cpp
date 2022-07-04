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
#include <cctype>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(__linux__)
#include <unistd.h>
#include <fnmatch.h>
#endif
#include <limits.h>
#include <unordered_map>
#include <unordered_set>
#include <re2/re2.h>
#include <set>
#include <vector>
#include <fstream>
#include "common/util.h"
#include "common/JsonUtil.h"
#include "common/HashUtil.h"
#include "common/RuntimeUtil.h"
#include "common/FileSystemUtil.h"
#include "common/Constants.h"
#include "common/ExceptionBase.h"
#include "common/CompressTools.h"
#include "common/ErrorUtil.h"
#include "common/TimeUtil.h"
#include "common/StringTools.h"
#include "common/GlobalPara.h"
#include "common/version.h"
#include "config/UserLogConfigParser.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogFileProfiler.h"
#include "profiler/LogIntegrity.h"
#include "profiler/LogLineCount.h"
#include "app_config/AppConfig.h"
#include "config_manager/ConfigYamlToJson.h"
#include "checkpoint/CheckPointManager.h"
#include "event_handler/EventHandler.h"
#include "controller/EventDispatcher.h"
#include "sender/Sender.h"
#include "processor/LogProcess.h"
#include "processor/LogFilter.h"

using namespace std;
using namespace logtail;

DEFINE_FLAG_BOOL(https_verify_peer, "set CURLOPT_SSL_VERIFYPEER, CURLOPT_SSL_VERIFYHOST option for libcurl", true);
#if defined(__linux__) || defined(__APPLE__)
DEFINE_FLAG_STRING(https_ca_cert, "set CURLOPT_CAINFO for libcurl", "ca-bundle.crt");
#elif defined(_MSC_VER)
DEFINE_FLAG_STRING(https_ca_cert, "set CURLOPT_CAINFO for libcurl", "cacert.pem");
#endif
DEFINE_FLAG_STRING(default_global_topic, "default is empty string", "");
DEFINE_FLAG_STRING(profile_project_name, "profile project_name for logtail", "");
DEFINE_FLAG_INT32(request_access_key_interval, "control the frenquency of GetAccessKey, seconds", 60);
DEFINE_FLAG_INT32(logtail_sys_conf_update_interval, "control the frenquency of load local machine conf, seconds", 60);
DEFINE_FLAG_INT32(wildcard_max_sub_dir_count, "", 1000);
DEFINE_FLAG_INT32(config_match_max_cache_size, "", 1000000);
DEFINE_FLAG_INT32(multi_config_alarm_interval, "second", 600);
DECLARE_FLAG_INT32(delay_bytes_upperlimit);
DECLARE_FLAG_BOOL(global_network_success);
DECLARE_FLAG_STRING(ilogtail_user_defined_id_env_name);
DECLARE_FLAG_STRING(ilogtail_aliuid_env_name);
DEFINE_FLAG_STRING(ilogtail_docker_file_path_config, "ilogtail docker path config file", "docker_path_config.json");
DEFINE_FLAG_STRING(ilogtail_docker_path_version, "ilogtail docker path config file", "0.1.0");
DEFINE_FLAG_INT32(max_docker_config_update_times, "max times docker config update in 3 minutes", 10);
DEFINE_FLAG_INT32(docker_config_update_interval, "interval between docker config updates, seconds", 3);
DEFINE_FLAG_STRING(default_data_integrity_project, "default data integrity project", "data_integrity");
DEFINE_FLAG_STRING(default_data_integrity_log_store, "default data integrity log store", "data_integrity");
DEFINE_FLAG_INT32(default_data_integrity_time_pos, "default data integrity time pos", 0);
DEFINE_FLAG_STRING(default_log_time_reg,
                   "default log time reg",
                   "([0-9]{4})-(0[0-9]{1}|1[0-2])-(0[0-9]{1}|[12][0-9]{1}|3[01]) "
                   "(0[0-9]{1}|1[0-9]{1}|2[0-3]):[0-5][0-9]{1}:([0-5][0-9]{1})");
DEFINE_FLAG_STRING(default_line_count_project, "default line count project", "line_count");
DEFINE_FLAG_STRING(default_line_count_log_store, "default line count log store", "line_count");
DEFINE_FLAG_BOOL(default_fuse_mode, "default fuse mode", false);
DEFINE_FLAG_BOOL(default_mark_offset_flag, "default mark offset flag", false);
DEFINE_FLAG_BOOL(default_check_ulogfs_env, "default check ulogfs env", false);
DEFINE_FLAG_INT32(default_max_depth_from_root, "default max depth from root", 1000);
DEFINE_FLAG_INT32(default_plugin_log_queue_size, "", 10);
DEFINE_FLAG_STRING(fuse_customized_config_name,
                   "name of specified config for fuse, should not be used by user",
                   "__FUSE_CUSTOMIZED_CONFIG__");
DEFINE_FLAG_BOOL(logtail_config_update_enable, "", true);

DECLARE_FLAG_BOOL(rapid_retry_update_config);
DECLARE_FLAG_BOOL(default_global_fuse_mode);
DECLARE_FLAG_BOOL(default_global_mark_offset_flag);
DECLARE_FLAG_BOOL(enable_collection_mark);
DECLARE_FLAG_BOOL(enable_env_ref_in_config);

namespace logtail {

static bool ReadAliuidsFile(std::vector<std::string>& aliuids) {
    std::string dirName = AppConfig::GetInstance()->GetLogtailSysConfDir() + STRING_FLAG(logtail_sys_conf_users_dir);
    fsutil::Dir dir(dirName);
    if (!dir.Open()) {
        auto err = GetErrno();
        if (fsutil::Dir::IsENOENT(err)) {
            aliuids.clear();
            return true;
        }
        LOG_ERROR(sLogger, ("Open dir failed", dirName)("errno", ErrnoToString(err)));
        return false;
    }
    fsutil::Entry ent;
    while (ent = dir.ReadNext(false)) {
        aliuids.push_back(ent.Name());
    }
    return true;
}

const std::string LOG_LOCAL_DEFINED_PATH_PREFIX = "__local_defined_path__";

ParseConfResult ParseConfig(const std::string& configName, Json::Value& jsonRoot) {
    // Get full path, if it is a relative path, prepend process execution dir.
    std::string fullPath = configName;
    if (IsRelativePath(fullPath)) {
        fullPath = GetProcessExecutionDir() + configName;
    }

    ifstream is;
    is.open(fullPath.c_str());
    if (!is.good())
        return CONFIG_NOT_EXIST;

    Chmod(fullPath.c_str(), 0644);
    std::string buffer((std::istreambuf_iterator<char>(is)), (std::istreambuf_iterator<char>()));
    if (!IsValidJson(buffer.c_str(), buffer.length())) {
        return CONFIG_INVALID_FORMAT;
    }

    Json::Reader jsonReader;
    bool parseOk = jsonReader.parse(buffer, jsonRoot);
    if (parseOk == false)
        return CONFIG_INVALID_FORMAT;

    return CONFIG_OK;
}

ParseConfResult ParseConfig(const std::string& configName, YAML::Node& yamlRoot) {
    // Get full path, if it is a relative path, prepend process execution dir.
    std::string fullPath = configName;
    if (IsRelativePath(fullPath)) {
        fullPath = GetProcessExecutionDir() + configName;
    }

    try {
        yamlRoot = YAML::LoadFile(fullPath);
    } catch (const YAML::BadFile& e) {
        return CONFIG_NOT_EXIST;
    } catch (const YAML::ParserException& e) {
        LOG_WARNING(sLogger, ("parse yaml failed", e.what()));
        return CONFIG_INVALID_FORMAT;
    } catch (...) {
        return CONFIG_INVALID_FORMAT;
    }
    return CONFIG_OK;
}

// CheckLogType validate @logTypeStr and convert it to @logType if it's valid.
bool ConfigManagerBase::CheckLogType(const string& logTypeStr, LogType& logType) {
    if (logTypeStr == "common_reg_log")
        logType = REGEX_LOG;
    else if (logTypeStr == "apsara_log")
        logType = APSARA_LOG;
    else if (logTypeStr == "streamlog")
        logType = STREAM_LOG;
    else if (logTypeStr == "delimiter_log")
        logType = DELIMITER_LOG;
    else if (logTypeStr == "json_log")
        logType = JSON_LOG;
    else if (logTypeStr == "plugin")
        logType = PLUGIN_LOG;
    else {
        LOG_ERROR(sLogger, ("not supported log type", logTypeStr));
        return false;
    }
    return true;
}

// LoadGlobalConfig reads config from @jsonRoot, and set to LogtailGlobalPara::Instance().
bool ConfigManagerBase::LoadGlobalConfig(const Json::Value& jsonRoot) {
    LOG_INFO(sLogger, ("load global config", jsonRoot.toStyledString()));
    static LogtailGlobalPara* sGlobalPara = LogtailGlobalPara::Instance();
    try {
        if (jsonRoot.isMember("global_topic")) {
            sGlobalPara->SetTopic(GetStringValue(jsonRoot, "global_topic", ""));
        } else {
            sGlobalPara->SetTopic(STRING_FLAG(default_global_topic));
        }
    } catch (const ExceptionBase& e) {
        LOG_ERROR(sLogger, ("The logtail global topic is invalid", e.GetExceptionMessage()));
        LogtailAlarm::GetInstance()->SendAlarm(GLOBAL_CONFIG_ALARM,
                                               string("The global_topic value is invalid") + e.GetExceptionMessage());
    } catch (...) {
        LOG_ERROR(sLogger, ("The logtail global topic is invalid", "unkown reason"));
        LogtailAlarm::GetInstance()->SendAlarm(GLOBAL_CONFIG_ALARM, string("The global_topic value is invalid"));
    }
    return true;
}

void ConfigManagerBase::MappingPluginConfig(const Json::Value& configValue, Config* config, Json::Value& pluginJson) {
    Json::Value inputs;
    Json::Value dockerFile;
    Json::Value detail;
    dockerFile["type"] = Json::Value("metric_docker_file");
    // if wildcard path, use config->mWildcardPaths[0] as path,  maxDepth += config->mWildcardPaths.size() - 1
    if (config->mWildcardPaths.size() > (size_t)0) {
        detail["LogPath"] = Json::Value(config->mWildcardPaths[0]);
        detail["MaxDepth"] = Json::Value(int(config->mWildcardPaths.size()) - int(1) + int(config->mMaxDepth));
    } else {
        detail["LogPath"] = Json::Value(config->mBasePath);
        detail["MaxDepth"] = Json::Value(config->mMaxDepth);
    }
    if (configValue.isMember("docker_include_label") && configValue["docker_include_label"].isObject()) {
        detail["IncludeLabel"] = configValue["docker_include_label"];
    }
    if (configValue.isMember("docker_exclude_label") && configValue["docker_exclude_label"].isObject()) {
        detail["ExcludeLabel"] = configValue["docker_exclude_label"];
    }
    if (configValue.isMember("docker_include_env") && configValue["docker_include_env"].isObject()) {
        detail["IncludeEnv"] = configValue["docker_include_env"];
    }
    if (configValue.isMember("docker_exclude_env") && configValue["docker_exclude_env"].isObject()) {
        detail["ExcludeEnv"] = configValue["docker_exclude_env"];
    }
    // parse k8s flags
    if (configValue.isMember("advanced") && configValue["advanced"].isObject()
        && configValue["advanced"].isMember("k8s") && configValue["advanced"]["k8s"].isObject()) {
        // k8s_filter is deprecated,using k8s instead.
        const Json::Value& k8sVal = configValue["advanced"]["k8s"];
        auto setDetail = [&](const std::string& key, Json::ValueType valueType) {
            if (k8sVal.isMember(key)) {
                if (k8sVal[key].type() == valueType) {
                    detail[key] = k8sVal[key];
                } else {
                    LOG_ERROR(sLogger,
                              ("Parse Config advanced.k8s Error", key)("support type", valueType)("parse type",
                                                                                                  k8sVal[key].type()));
                }
            }
        };
        setDetail("K8sNamespaceRegex", Json::ValueType::stringValue);
        setDetail("K8sPodRegex", Json::ValueType::stringValue);
        setDetail("K8sContainerRegex", Json::ValueType::stringValue);

        setDetail("IncludeEnv", Json::ValueType::objectValue);
        setDetail("ExcludeEnv", Json::ValueType::objectValue);

        setDetail("IncludeLabel", Json::ValueType::objectValue);
        setDetail("ExcludeLabel", Json::ValueType::objectValue);

        setDetail("IncludeContainerLabel", Json::ValueType::objectValue);
        setDetail("ExcludeContainerLabel", Json::ValueType::objectValue);
        setDetail("IncludeK8sLabel", Json::ValueType::objectValue);
        setDetail("ExcludeK8sLabel", Json::ValueType::objectValue);
        setDetail("ExternalEnvTag", Json::ValueType::objectValue);
        setDetail("ExternalK8sLabelTag", Json::ValueType::objectValue);
    }
    dockerFile["detail"] = detail;
    inputs.append(dockerFile);

    // Overwite inputs and some fields in global.
    pluginJson["inputs"] = inputs;
    const std::string kGlobalFiledName = "global";
    Json::Value global;
    if (pluginJson.isMember(kGlobalFiledName) && pluginJson[kGlobalFiledName].isObject()) {
        global = pluginJson[kGlobalFiledName];
    }
    SetNotFoundJsonMember(global, "DefaultLogQueueSize", INT32_FLAG(default_plugin_log_queue_size));
    SetNotFoundJsonMember(global, "AlwaysOnline", true);
    pluginJson[kGlobalFiledName] = global;
    config->mPluginConfig = pluginJson.toStyledString();
    LOG_INFO(sLogger, ("docker file config", config->mPluginConfig));
}

// LoadSingleUserConfig constructs new Config object according to @value, and insert it into
// mNameConfigMap with name @logName.
void ConfigManagerBase::LoadSingleUserConfig(const std::string& logName, const Json::Value& rawValue, bool localFlag) {
    Config* config = NULL;
    string projectName, category, errorMessage;
    LOG_DEBUG(sLogger, ("message", "load single user config")("json", rawValue.toStyledString()));
    const Json::Value* valuePtr = &rawValue;
    Json::Value replacedValue = rawValue;
    if (BOOL_FLAG(enable_env_ref_in_config)) {
        // replace environment variable reference in config string
        ReplaceEnvVarRefInConf(replacedValue);
        valuePtr = &replacedValue;
        LOG_DEBUG(
            sLogger,
            ("message", "user config after environment variable replacement")("json", replacedValue.toStyledString()));
    }
    const Json::Value& value = *valuePtr;
    try {
        if (value["enable"].asBool()) {
            int version = GetIntValue(value, USER_CONFIG_VERSION, 0);
            if (version == -1) {
                return;
            }
            projectName = GetStringValue(value, "project_name", "");
            category = GetStringValue(value, "category", "");
            string logTypeStr = GetStringValue(value, "log_type", "plugin");
            auto region = GetStringValue(value, "region", AppConfig::GetInstance()->GetDefaultRegion());
            LogType logType;
            if (!CheckLogType(logTypeStr, logType)) {
                throw ExceptionBase(std::string("The logType is invalid : ") + logTypeStr);
            }
            bool discardUnmatch = GetBoolValue(value, "discard_unmatch", true);

            // this field is for ant
            // all configuration are included in "customized" field
            bool dataIntegritySwitch = false;
            string dataIntegrityProjectName, dataIntegrityLogstore, logTimeReg;
            int32_t timePos = 0;
            bool lineCountSwitch = false;
            string lineCountProjectName, lineCountLogstore;
            bool isFuseMode = BOOL_FLAG(default_global_fuse_mode);
            bool markOffsetFlag = BOOL_FLAG(default_global_mark_offset_flag);
            bool collectBackwardTillBootTime = false;
            if (value.isMember("customized_fields") && value["customized_fields"].isObject()) {
                // parse data integrity and line count fields
                const Json::Value& customizedFieldsValue = value["customized_fields"];
                if (customizedFieldsValue.isMember("data_integrity")
                    && customizedFieldsValue["data_integrity"].isObject()) {
                    const Json::Value& dataIntegrityValue = customizedFieldsValue["data_integrity"];
                    dataIntegritySwitch = GetBoolValue(dataIntegrityValue, "switch", false);
                    if (dataIntegritySwitch) {
                        dataIntegrityProjectName = GetStringValue(
                            dataIntegrityValue, "project_name", STRING_FLAG(default_data_integrity_project));
                        dataIntegrityLogstore = GetStringValue(
                            dataIntegrityValue, "logstore", STRING_FLAG(default_data_integrity_log_store));
                        logTimeReg
                            = GetStringValue(dataIntegrityValue, "log_time_reg", STRING_FLAG(default_log_time_reg));
                        timePos
                            = GetIntValue(dataIntegrityValue, "time_pos", INT32_FLAG(default_data_integrity_time_pos));
                    }
                }
                if (customizedFieldsValue.isMember("line_count") && customizedFieldsValue["line_count"].isObject()) {
                    const Json::Value& lineCountValue = customizedFieldsValue["line_count"];
                    lineCountSwitch = GetBoolValue(lineCountValue, "switch", false);
                    if (lineCountSwitch) {
                        lineCountProjectName
                            = GetStringValue(lineCountValue, "project_name", STRING_FLAG(default_line_count_project));
                        lineCountLogstore
                            = GetStringValue(lineCountValue, "logstore", STRING_FLAG(default_line_count_log_store));
                    }
                }

                if (customizedFieldsValue.isMember("check_ulogfs_env")
                    && customizedFieldsValue["check_ulogfs_env"].isBool()) {
                    bool checkUlogfsEnv
                        = GetBoolValue(customizedFieldsValue, "check_ulogfs_env", BOOL_FLAG(default_check_ulogfs_env));
                    bool hasCollectionMarkFileFlag
                        = BOOL_FLAG(enable_collection_mark) ? GetCollectionMarkFileExistFlag() : false;
                    if (checkUlogfsEnv && !hasCollectionMarkFileFlag) {
                        // only set in pod's app container
                        const char* ulogfsEnabledEnv = getenv("ULOGFS_ENABLED");
                        if (ulogfsEnabledEnv) {
                            if (strcmp(ulogfsEnabledEnv, "true") == 0) {
                                LOG_WARNING(sLogger,
                                            ("load conifg", logName)("skip config", category)("project", projectName)(
                                                "check_ulogfs_env", "true")("env of ULOGFS_ENABLED", "true"));
                                // if ULOGFS_ENABLED in env is true and check_ulogfs_env in config json is true
                                // it means this logtail instance should skip this fuse config, we should no load it
                                return;
                            }
                        }
                    }
                }

                isFuseMode
                    = isFuseMode && GetBoolValue(customizedFieldsValue, "fuse_mode", BOOL_FLAG(default_fuse_mode));
                markOffsetFlag = markOffsetFlag && GetBoolValue(customizedFieldsValue, "mark_offset", false);
                collectBackwardTillBootTime
                    = GetBoolValue(customizedFieldsValue, "collect_backward_till_boot_time", false);
            }

            string pluginConfig;
            bool flusher_exists = false;
            if (value.isMember("plugin")) {
                if (value["plugin"].isObject()) {
                    pluginConfig = value["plugin"].toStyledString();
                    if (value["plugin"].isMember("flushers")) {
                        flusher_exists = true;
                    }
                } else if (value["plugin"].isString()) {
                    pluginConfig = value["plugin"].asString();
                    if (pluginConfig.find("\"flushers\"")) {
                        flusher_exists = true;
                    }
                }
            }
            if ((projectName == "" || category == "") && !flusher_exists) {
                throw ExceptionBase(std::string("Neither project/logstore or flusher exists"));
            }
            // prevent invalid input like "{}" "null" ...
            if (pluginConfig.size() < 20) {
                pluginConfig.clear();
            }

            if (logType == PLUGIN_LOG) {
                config = new Config(
                    "", "", logType, logName, "", projectName, false, 0, 0, category, false, "", discardUnmatch);
                if (pluginConfig.empty()) {
                    throw ExceptionBase(std::string("The plugin log type is invalid"));
                }
                LOG_DEBUG(sLogger, ("load plugin config ", logName)("config", pluginConfig));
                config->mPluginConfig = pluginConfig;

            } else if (logType == STREAM_LOG) {
                config = new Config("",
                                    "",
                                    logType,
                                    logName,
                                    "",
                                    projectName,
                                    false,
                                    0,
                                    0,
                                    category,
                                    false,
                                    GetStringValue(value, "tag"),
                                    discardUnmatch);
            } else {
                bool isPreserve = GetBoolValue(value, "preserve", true);
                int preserveDepth = 0;
                if (!isPreserve) {
                    preserveDepth = GetIntValue(value, "preserve_depth");
                }
                int maxDepth = -1;
                if (value.isMember("max_depth"))
                    maxDepth = GetIntValue(value, "max_depth");
                string logPath = GetStringValue(value, "log_path");
                if (logPath.find(LOG_LOCAL_DEFINED_PATH_PREFIX) == 0) {
                    mHaveMappingPathConfig = true;
                    string tmpLogPath = GetMappingPath(logPath);
                    if (!tmpLogPath.empty())
                        logPath = tmpLogPath;
                }
                // one may still make mistakes, teminate logPath by '/'
                size_t size = logPath.size();
                if (size > 0 && PATH_SEPARATOR[0] == logPath[size - 1])
                    logPath = logPath.substr(0, size - 1);

                string logBeingReg = GetStringValue(value, "log_begin_reg", "");
                if (logBeingReg != "" && CheckRegFormat(logBeingReg) == false) {
                    throw ExceptionBase("The log begin line is not value regex : " + logBeingReg);
                }
                string filePattern = GetStringValue(value, "file_pattern");
                // raw log flag
                bool rawLogFlag = false;
                if (value.isMember("raw_log") && value["raw_log"].isBool()) {
                    rawLogFlag = value["raw_log"].asBool();
                }

                config = new Config(logPath,
                                    filePattern,
                                    logType,
                                    logName,
                                    logBeingReg,
                                    projectName,
                                    isPreserve,
                                    preserveDepth,
                                    maxDepth,
                                    category,
                                    rawLogFlag,
                                    "",
                                    discardUnmatch);

                // normal log file config can have plugin too
                Json::Value pluginConfigJSON;
                if (!pluginConfig.empty()) {
                    // check processors
                    Json::Reader jsonReader;
                    if (jsonReader.parse(pluginConfig, pluginConfigJSON)) {
                        // set process flag when config have processors
                        if (pluginConfigJSON.isMember("processors")
                            && (pluginConfigJSON["processors"].isObject()
                                || pluginConfigJSON["processors"].isArray())) {
                            config->mPluginProcessFlag = true;
                            config->mPluginConfig = pluginConfig;
                        }
                    }
                }

                if (value.isMember("docker_file") && value["docker_file"].isBool() && value["docker_file"].asBool()) {
                    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
                        // docker file is not supported in Logtail's container mode
                        if (AppConfig::GetInstance()->IsContainerMode()) {
                            throw ExceptionBase(
                                std::string("docker file is not supported in Logtail's container mode "));
                        }
                        // load saved container path
                        auto iter = mAllDockerContainerPathMap.find(logName);
                        if (iter != mAllDockerContainerPathMap.end()) {
                            config->mDockerContainerPaths = iter->second;
                            mAllDockerContainerPathMap.erase(iter);
                        }
                        if (!config->SetDockerFileFlag(true)) {
                            // should not happen
                            throw ExceptionBase(std::string("docker file do not support wildcard path"));
                        }
                        MappingPluginConfig(value, config, pluginConfigJSON);
                    } else {
                        LOG_WARNING(sLogger,
                                    ("config is docker_file mode, but logtail is not a purage container",
                                     "the flag is ignored")("project", projectName)("logstore", category));
                        LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                               string("config is docker_file mode, but logtail is not "
                                                                      "a purage container, the flag is ignored"),
                                                               projectName,
                                                               category,
                                                               region);
                    }
                }

                if (AppConfig::GetInstance()->IsContainerMode()) {
                    // mapping config's path to real filePath
                    // use docker file flag
                    if (!config->SetDockerFileFlag(true)) {
                        // should not happen
                        throw ExceptionBase(std::string("docker file do not support wildcard path"));
                    }
                    string realPath;
                    string basePath
                        = config->mWildcardPaths.size() > (size_t)0 ? config->mWildcardPaths[0] : config->mBasePath;
                    mDockerMountPathsLock.lock();
                    bool findRst = mDockerMountPaths.FindBestMountPath(basePath, realPath);
                    mDockerMountPathsLock.unlock();
                    if (!findRst) {
                        throw ExceptionBase(std::string("invalid mount path, basePath : ") + basePath);
                    }

                    // add containerPath
                    DockerContainerPath containerPath;
                    containerPath.mContainerPath = realPath;
                    config->mDockerContainerPaths->push_back(containerPath);
                }

                config->mTopicFormat = GetStringValue(value, "topic_format", "default");
                if (!config->mTopicFormat.empty()) {
                    // Constant prefix to indicate use customized topic name,
                    // it might be an invalid regex, so compare directly.
                    const std::string customizedPrefix = "customized://";
                    if (StartWith(config->mTopicFormat, customizedPrefix)) {
                        config->mCustomizedTopic = config->mTopicFormat.substr(customizedPrefix.length());
                        config->mTopicFormat = "customized";
                    } else if (CheckTopicRegFormat(config->mTimeFormat) == false) {
                        throw ExceptionBase("The topic format is not valid regex");
                    }
                }
                string fileEncoding = GetStringValue(value, "file_encoding", "");
                if (ToLowerCaseString(fileEncoding) == "gbk")
                    config->mFileEncoding = ENCODING_GBK;
                else
                    config->mFileEncoding = ENCODING_UTF8;
                if (value.isMember("filter_keys") && value.isMember("filter_regs")) {
                    config->mFilterRule.reset(GetFilterFule(value["filter_keys"], value["filter_regs"]));
                }
                if (value.isMember("dir_pattern_black_list")) {
                    config->mUnAcceptDirPattern = GetStringVector(value["dir_pattern_black_list"]);
                }

                if (value.isMember("merge_type") && value["merge_type"].isString()) {
                    string mergeType = value["merge_type"].asString();

                    if (mergeType == "logstore") {
                        config->mMergeType = MERGE_BY_LOGSTORE;
                        LOG_INFO(sLogger, ("set config merge type to MERGE_BY_LOGSTORE", config->mConfigName));
                    } else {
                        config->mMergeType = MERGE_BY_TOPIC;
                    }
                }

                if (value.isMember("tz_adjust") && value["tz_adjust"].isBool() && value.isMember("log_tz")
                    && value["log_tz"].isString()) {
                    string logTZ = value["log_tz"].asString();
                    int logTZSecond = 0;
                    bool adjustFlag = value["tz_adjust"].asBool();
                    if (adjustFlag && !ParseTimeZoneOffsetSecond(logTZ, logTZSecond)) {
                        LOG_ERROR(sLogger, ("invalid log time zone set", logTZ));
                        config->mTimeZoneAdjust = false;
                    } else {
                        config->mTimeZoneAdjust = adjustFlag;
                        config->mLogTimeZoneOffsetSecond = logTZSecond;
                        if (adjustFlag) {
                            LOG_INFO(
                                sLogger,
                                ("set log timezone adjust, project", config->mProjectName)(
                                    "logstore", config->mCategory)("time zone", logTZ)("offset seconds", logTZSecond));
                        }
                    }
                }

                // create time
                int32_t createTime = 0;
                if (value.isMember("create_time") && value["create_time"].isInt()) {
                    createTime = value["create_time"].asInt();
                }
                config->mCreateTime = createTime;

                if (value.isMember("max_send_rate") && value["max_send_rate"].isInt()
                    && value.isMember("send_rate_expire") && value["send_rate_expire"].isInt()) {
                    int32_t maxSendBytesPerSecond = value["max_send_rate"].asInt();
                    int32_t expireTime = value["send_rate_expire"].asInt();
                    config->mMaxSendBytesPerSecond = maxSendBytesPerSecond;
                    config->mSendRateExpireTime = expireTime;
                    if (maxSendBytesPerSecond >= 0) {
                        LOG_INFO(sLogger,
                                 ("set logstore flow control, project", config->mProjectName)(
                                     "logstore", config->mCategory)("max send byteps", config->mMaxSendBytesPerSecond)(
                                     "expire", expireTime - (int32_t)time(NULL)));
                    }
                    Sender::Instance()->SetLogstoreFlowControl(config->mLogstoreKey, maxSendBytesPerSecond, expireTime);
                }
                config->mPriority = 0;
                if (value.isMember("priority") && value["priority"].isInt()) {
                    int32_t priority = value["priority"].asInt();
                    if (priority < 0 || priority > MAX_CONFIG_PRIORITY_LEVEL) {
                        LOG_ERROR(
                            sLogger,
                            ("invalid config priority, project", config->mProjectName)("logstore", config->mCategory));
                    } else {
                        config->mPriority = priority;
                        if (priority > 0) {
                            LogProcess::GetInstance()->SetPriorityWithHoldOn(config->mLogstoreKey, priority);
                            LOG_INFO(sLogger,
                                     ("set logstore priority, project",
                                      config->mProjectName)("logstore", config->mCategory)("priority", priority));
                        }
                    }
                }
                if (config->mPriority == 0) {
                    // if mPriority is 0, try to delete high level queue
                    LogProcess::GetInstance()->DeletePriorityWithHoldOn(config->mLogstoreKey);
                }

                if (value.isMember("sensitive_keys") && value["sensitive_keys"].isArray()) {
                    GetSensitiveKeys(value["sensitive_keys"], config);
                }

                if (value.isMember("delay_alarm_bytes") && value["delay_alarm_bytes"].isInt()) {
                    config->mLogDelayAlarmBytes = value["delay_alarm_bytes"].asInt64();
                }
                if (config->mLogDelayAlarmBytes <= 0) {
                    config->mLogDelayAlarmBytes = INT32_FLAG(delay_bytes_upperlimit);
                }

                if (value.isMember("delay_skip_bytes") && value["delay_skip_bytes"].isInt()) {
                    config->mLogDelaySkipBytes = value["delay_skip_bytes"].asInt64();
                }


                if (logType == REGEX_LOG) {
                    config->mTimeFormat = GetStringValue(value, "timeformat", "");
                    GetRegexAndKeys(value, config);
                    if (config->mRegs && config->mKeys && config->mRegs->size() == (size_t)1
                        && config->mKeys->size() == (size_t)1) {
                        if ((config->mLogBeginReg.empty() || config->mLogBeginReg == ".*")
                            && *(config->mKeys->begin()) == DEFAULT_CONTENT_KEY
                            && *(config->mRegs->begin()) == DEFAULT_REG) {
                            LOG_DEBUG(sLogger,
                                      ("config is simple mode", config->GetProjectName())(config->GetCategory(),
                                                                                          config->mLogBeginReg));
                            config->mSimpleLogFlag = true;
                        }
                    }
                }

                config->mTimeKey = GetStringValue(value, "time_key", "");

                config->mTailExisted = GetBoolValue(value, "tail_existed", false);
                int32_t tailLimit = GetIntValue(value, "tail_limit", INT32_FLAG(default_tail_limit_kb));
                config->SetTailLimit(tailLimit);

                UserLogConfigParser::ParseAdvancedConfig(value, *config);
            }
            if (logType == DELIMITER_LOG) {
                config->mTimeFormat = GetStringValue(value, "timeformat", "");
                string separatorStr = GetStringValue(value, "delimiter_separator");
                if (separatorStr == "\\t")
                    separatorStr = '\t';
                if (separatorStr.size() == 1) {
                    config->mSeparator = separatorStr;
                    string quoteStr = GetStringValue(value, "delimiter_quote", "\"");
                    if (quoteStr.size() == 1)
                        config->mQuote = quoteStr[0];
                    else
                        throw ExceptionBase("quote for Delimiter Log only support single char(like \")");
                } else if (separatorStr.size() > 1)
                    config->mSeparator = separatorStr;
                else
                    throw ExceptionBase("separator for Delimiter Log should not be empty");

                const Json::Value& columnKeys = value["column_keys"];
                for (uint32_t cIdx = 0; cIdx < columnKeys.size(); ++cIdx)
                    config->mColumnKeys.push_back(columnKeys[cIdx].asString());

                config->mTimeKey = GetStringValue(value, "time_key", "");
                config->mAutoExtend = GetBoolValue(value, "auto_extend", true);
                config->mAcceptNoEnoughKeys = GetBoolValue(value, "accept_no_enough_keys", false);
            } else if (logType == JSON_LOG) {
                config->mTimeFormat = GetStringValue(value, "timeformat", "");
                config->mTimeKey = GetStringValue(value, "time_key", "");
            }

            if (value.isMember("merge_type") && value["merge_type"].isString()) {
                string mergeType = value["merge_type"].asString();

                if (mergeType == "logstore") {
                    config->mMergeType = MERGE_BY_LOGSTORE;
                    LOG_INFO(sLogger, ("set config merge type to MERGE_BY_LOGSTORE", config->mConfigName));
                } else {
                    config->mMergeType = MERGE_BY_TOPIC;
                }
            }

            if (value.isMember("tz_adjust") && value["tz_adjust"].isBool() && value.isMember("log_tz")
                && value["log_tz"].isString()) {
                string logTZ = value["log_tz"].asString();
                int logTZSecond = 0;
                bool adjustFlag = value["tz_adjust"].asBool();
                if (adjustFlag && !ParseTimeZoneOffsetSecond(logTZ, logTZSecond)) {
                    LOG_ERROR(sLogger, ("invalid log time zone set", logTZ));
                    config->mTimeZoneAdjust = false;
                } else {
                    config->mTimeZoneAdjust = adjustFlag;
                    config->mLogTimeZoneOffsetSecond = logTZSecond;
                    if (adjustFlag) {
                        LOG_INFO(sLogger,
                                 ("set log timezone adjust, project", config->mProjectName)(
                                     "logstore", config->mCategory)("time zone", logTZ)("offset seconds", logTZSecond));
                    }
                }
            }

            // create time
            int32_t createTime = 0;
            if (value.isMember("create_time") && value["create_time"].isInt()) {
                createTime = value["create_time"].asInt();
            }
            config->mCreateTime = createTime;

            if (value.isMember("max_send_rate") && value["max_send_rate"].isInt() && value.isMember("send_rate_expire")
                && value["send_rate_expire"].isInt()) {
                int32_t maxSendBytesPerSecond = value["max_send_rate"].asInt();
                int32_t expireTime = value["send_rate_expire"].asInt();
                config->mMaxSendBytesPerSecond = maxSendBytesPerSecond;
                config->mSendRateExpireTime = expireTime;
                LOG_INFO(
                    sLogger,
                    ("set logstore flow control, project", config->mProjectName)("logstore", config->mCategory)(
                        "max send byteps", config->mMaxSendBytesPerSecond)("expire", expireTime - (int32_t)time(NULL)));
                Sender::Instance()->SetLogstoreFlowControl(config->mLogstoreKey, maxSendBytesPerSecond, expireTime);
            }

            if (value.isMember("sensitive_keys") && value["sensitive_keys"].isArray()) {
                GetSensitiveKeys(value["sensitive_keys"], config);
            }

            config->mGroupTopic = GetStringValue(value, USER_CONFIG_GROUPTOPIC, "");
            config->mLocalStorage = true; // Must be true, parameter local_storage is desperated.
            config->mVersion = version;

            config->mDiscardNoneUtf8 = GetBoolValue(value, "discard_none_utf8", false);

            config->mAliuid = GetStringValue(value, "aliuid", "");
            if (AppConfig::GetInstance()->IsDataServerPrivateCloud())
                config->mRegion = STRING_FLAG(default_region_name);
            else {
                config->mRegion = region;
                std::string defaultEndpoint = TrimString(GetStringValue(value, "defaultEndpoint", ""), ' ', ' ');
                if (defaultEndpoint.size() > 0)
                    Sender::Instance()->AddEndpointEntry(config->mRegion,
                                                         CheckAddress(defaultEndpoint, defaultEndpoint));
                if (value.isMember("endpoint_list")) {
                    const Json::Value& epArray = value["endpoint_list"];
                    if (epArray.isArray()) {
                        for (uint32_t epIdx = 0; epIdx < epArray.size(); ++epIdx) {
                            if (!(epArray[epIdx].isString()))
                                continue;
                            string ep = TrimString(epArray[epIdx].asString(), ' ', ' ');
                            if (ep.size() > 0)
                                Sender::Instance()->AddEndpointEntry(config->mRegion, CheckAddress(ep, ep));
                        }
                    }
                }
            }

            config->mShardHashKey.clear();
            if (value.isMember("shard_hash_key")) {
                const Json::Value& shardHashKey = value["shard_hash_key"];
                if (shardHashKey.isArray()) {
                    for (uint32_t sIdx = 0; sIdx < shardHashKey.size(); ++sIdx)
                        config->mShardHashKey.push_back(shardHashKey[sIdx].asString());
                }
            }
            config->mLocalFlag = localFlag;

            config->mIntegrityConfig.reset(new IntegrityConfig(config->mAliuid,
                                                               dataIntegritySwitch,
                                                               dataIntegrityProjectName,
                                                               dataIntegrityLogstore,
                                                               logTimeReg,
                                                               config->mTimeFormat,
                                                               timePos));
            // if integrity switch is off, erase corresponding item in integrity map
            if (!dataIntegritySwitch) {
                LogIntegrity::GetInstance()->EraseItemInMap(config->mRegion, config->mProjectName, config->mCategory);
            }

            config->mLineCountConfig.reset(
                new LineCountConfig(config->mAliuid, lineCountSwitch, lineCountProjectName, lineCountLogstore));
            // if line count switch is off, erase corresponding item in line count map
            if (!lineCountSwitch) {
                LogLineCount::GetInstance()->EraseItemInMap(config->mRegion, config->mProjectName, config->mCategory);
            }

            // set fuse mode
            config->mIsFuseMode = isFuseMode;
            // if fuse mode is true, then we should mark offset
            config->mMarkOffsetFlag = isFuseMode || markOffsetFlag;
            mHaveFuseConfigFlag = mHaveFuseConfigFlag || isFuseMode;
            config->mCollectBackwardTillBootTime = collectBackwardTillBootTime;

            // time format should not be blank here
            if ((collectBackwardTillBootTime || dataIntegritySwitch) && config->mTimeFormat.empty()) {
                LOG_ERROR(sLogger,
                          ("time format should not be blank if collect backward or open fata integrity function", ""));
            }

            auto configIter = mNameConfigMap.find(logName);
            if (mNameConfigMap.end() != configIter) {
                LOG_ERROR(sLogger,
                          ("duplicated config name, last will be deleted",
                           logName)("last", configIter->second->mBasePath)("new", config->mBasePath));
                delete configIter->second;
                configIter->second = config;
            } else {
                mNameConfigMap[logName] = config;
            }
            InsertProject(config->mProjectName);
            InsertRegion(config->mRegion);
        }
    } catch (const ExceptionBase& e) {
        LOG_ERROR(sLogger, ("failed to parse config logname", logName)("Reason", e.GetExceptionMessage()));
        errorMessage = logName + string(" is invalid, reason:") + e.GetExceptionMessage();
    } catch (const exception& e) {
        LOG_ERROR(sLogger, ("failed to parse config logname", logName)("Reason", e.what()));
        errorMessage = logName + string(" is invalid, reason:") + e.what();
    } catch (...) {
        LOG_ERROR(sLogger, ("failed to parse config logname", logName)("Reason", "Unknown"));
        errorMessage = logName + " is invalid, reason:unknow";
    }
    if (!errorMessage.empty()) {
        if (config)
            delete config;
        if (!projectName.empty())
            LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM, errorMessage, projectName, category);
        else
            LogtailAlarm::GetInstance()->SendAlarm(USER_CONFIG_ALARM, errorMessage);
    }
}

void ConfigManagerBase::GetRegexAndKeys(const Json::Value& value, Config* configPtr) {
    configPtr->mRegs.reset(new list<string>());
    configPtr->mKeys.reset(new list<string>());
    if (value.isMember("regex") == false || value.isMember("keys") == false) {
        throw ExceptionBase("The [regex] or [keys] field is not set");
    }

    const Json::Value& regs = value["regex"];
    const Json::Value& keys = value["keys"];
    if (regs.size() == 0 || regs.size() != keys.size()) {
        throw ExceptionBase(std::string("The regex and keys size is invalid, regs size : ") + ToString(regs.size())
                            + string(" keys size : ") + ToString(keys.size()));
    }
    for (uint32_t i = 0; i < regs.size(); i++) {
        if (CheckRegFormat(regs[i].asString()) == false) {
            throw ExceptionBase(std::string("The regex is invalid") + regs[i].asString());
        }
        configPtr->mRegs->push_back(regs[i].asString());
        configPtr->mKeys->push_back(keys[i].asString().empty() ? DEFAULT_CONTENT_KEY : keys[i].asString());
    }
}

bool ConfigManagerBase::CheckRegFormat(const std::string& regStr) {
    try {
        boost::regex reg(regStr);
    } catch (...) {
        return false;
    }
    return true;
}

// GetStringVector converts @value to vector<string>.
// **Will throw exception if @value is not array<string>**.
vector<string> ConfigManagerBase::GetStringVector(const Json::Value& value) {
    if (value.isArray() == false)
        throw ExceptionBase(std::string("value is not string" + value.toStyledString()));
    vector<string> result;
    for (unsigned int i = 0; i < value.size(); ++i) {
        if (value[i].isString())
            result.push_back(value[i].asString());
        else
            throw ExceptionBase(std::string("value ") + value.toStyledString() + ", pos:" + ToString(i)
                                + " not valid string value");
    }
    return result;
}

// GetFilterFule constructs LogFilterRule according to @filterKeys and @filterRegs.
// **Will throw exception if @filterKeys.size() != @filterRegs.size() or failed**.
LogFilterRule* ConfigManagerBase::GetFilterFule(const Json::Value& filterKeys, const Json::Value& filterRegs) {
    if (filterKeys.size() != filterRegs.size()) {
        throw ExceptionBase(std::string("The filterKey size is ") + ToString(filterKeys.size())
                            + std::string(", while the filterRegs size is ") + ToString(filterRegs.size()));
    }

    LogFilterRule* rulePtr = new LogFilterRule();
    for (uint32_t i = 0; i < filterKeys.size(); i++) {
        try {
            rulePtr->FilterKeys.push_back(filterKeys[i].asString());
            rulePtr->FilterRegs.push_back(boost::regex(filterRegs[i].asString()));
        } catch (const exception& e) {
            LOG_WARNING(sLogger, ("The filter is invalid", e.what()));
            delete rulePtr;
            throw;
        } catch (...) {
            LOG_WARNING(sLogger, ("The filter is invalid reason unknow", ""));
            delete rulePtr;
            throw;
        }
    }
    return rulePtr;
}

bool ConfigManagerBase::LoadAllConfig() {
    bool rst = true;
    rst &= LoadJsonConfig(GetConfigJson());
    LOG_DEBUG(sLogger, ("load remote server config", rst)("now config count", mNameConfigMap.size()));
    rst &= LoadJsonConfig(GetLocalConfigJson());
    LOG_DEBUG(sLogger, ("load local config", rst)("now config count", mNameConfigMap.size()));
    for (auto iter = mLocalConfigDirMap.begin(); iter != mLocalConfigDirMap.end(); ++iter) {
        rst &= LoadJsonConfig(iter->second);
        LOG_DEBUG(
            sLogger,
            ("load local user_config.d config", rst)("file", iter->first)("now config count", mNameConfigMap.size()));
    }
    for (auto iter = mLocalYamlConfigDirMap.begin(); iter != mLocalYamlConfigDirMap.end(); ++iter) {
        Json::Value userLocalJsonConfig;
        std::string fileName = iter->first;
        if (ConfigYamlToJson::GetInstance()->GenerateLocalJsonConfig(fileName, iter->second, userLocalJsonConfig)) {
            rst &= LoadJsonConfig(userLocalJsonConfig);
            LOG_INFO(sLogger,
                     ("load local user_yaml_config.d config", rst)("file", fileName)("now config count",
                                                                                     mNameConfigMap.size()));
        }
    }
    return rst;
}

// LoadConfig constructs Config objects accroding to @jsonRoot.
// This function is called by EventDispatcher's Dispatch thread (main thread).
// It will iterate all log configs in @jsonRoot[USER_CONFIG_NODE], then call
// LoadSingleUserConfig to deal them.
bool ConfigManagerBase::LoadJsonConfig(const Json::Value& jsonRoot, bool localFlag) {
    try {
        mHaveMappingPathConfig = false;
        // USER_CONFIG_NODE is optional
        if (jsonRoot.isMember(USER_CONFIG_NODE) == false)
            return true;
        const Json::Value& metrics = jsonRoot[USER_CONFIG_NODE];
        Json::Value::Members logNames = metrics.getMemberNames();
        for (size_t index = 0; index < logNames.size(); index++) {
            const string& logName = logNames[index];
            const Json::Value& metric = metrics[logName];
            LoadSingleUserConfig(logName, metric, localFlag);
        }
        // generate customized config for fuse
        if (HaveFuseConfig()) {
            CreateCustomizedFuseConfig();
        }
    } catch (...) {
        LOG_ERROR(sLogger, ("config parse error", ""));
        LogtailAlarm::GetInstance()->SendAlarm(USER_CONFIG_ALARM, string("the user config is invalid"));
    }

    return true;
}

// dir which is not timeout will be registerd recursively
// if checkTimeout, will not register the dir which is timeout
// if not checkTimeout, will register the dir which is timeout and add it to the timeout list
bool ConfigManagerBase::RegisterHandlersRecursively(const std::string& path, Config* config, bool checkTimeout) {
    bool result = false;
    if (checkTimeout && config->IsTimeout(path))
        return result;

    if (MatchDirPattern(config, path))
        result = EventDispatcher::GetInstance()->RegisterEventHandler(path.c_str(), config, mSharedHandler);

    if (!result)
        return result;

    fsutil::Dir dir(path);
    if (!dir.Open()) {
        auto err = GetErrno();
        LogtailAlarm::GetInstance()->SendAlarm(LOGDIR_PERMINSSION_ALARM,
                                               string("Failed to open dir : ") + path + ";\terrno : " + ToString(err),
                                               config->GetProjectName(),
                                               config->GetCategory(),
                                               config->mRegion);

        LOG_ERROR(sLogger, ("Open dir fail", path.c_str())("errno", ErrnoToString(err)));
        return false;
    }
    fsutil::Entry ent;
    while (ent = dir.ReadNext()) {
        string item = PathJoin(path, ent.Name());
        if (ent.IsDir()) {
            if (false == RegisterHandlersRecursively(item, config, checkTimeout)) {
                result = false;
            }
        }
    }
    return result;
}

ConfigManagerBase::ConfigManagerBase() {
    mEnvFlag = false;
    mStartTime = time(NULL);
    mRemoveConfigFlag = false;
    mHaveMappingPathConfig = false;
    mMappingPathsChanged = false;
    mSharedHandler = NULL;
    mThreadIsRunning = true;
    mUpdateStat = NORMAL;
    mRegionType = REGION_CORP;

    mLogtailSysConfUpdateTime = 0;
    mUserDefinedId.clear();
    mUserDefinedIdSet.clear();
    mAliuidSet.clear();

    SetDefaultPubAccessKeyId(STRING_FLAG(default_access_key_id));
    SetDefaultPubAccessKey(STRING_FLAG(default_access_key));
    SetDefaultPubAliuid("");
    SetUserAK(STRING_FLAG(logtail_profile_aliuid),
              STRING_FLAG(logtail_profile_access_key_id),
              STRING_FLAG(logtail_profile_access_key));
    srand(time(NULL));
    CorrectionLogtailSysConfDir(); //first create dir then rewrite system-uuid file in GetSystemUUID
    // use a thread to get uuid, work around for CalculateDmiUUID hang
    //mUUID = CalculateDmiUUID();
    mInstanceId = CalculateRandomUUID() + "_" + LogFileProfiler::mIpAddr + "_" + ToString(time(NULL));
    ReloadMappingConfig();
}

ConfigManagerBase::~ConfigManagerBase() {
    unordered_map<string, Config*>::iterator itr = mNameConfigMap.begin();
    for (; itr != mNameConfigMap.end(); ++itr) {
        delete itr->second;
    }

    unordered_map<std::string, EventHandler*>::iterator itr1 = mDirEventHandlerMap.begin();
    for (; itr1 != mDirEventHandlerMap.end(); ++itr1) {
        if (itr1->second != mSharedHandler)
            delete itr1->second;
    }
    mDirEventHandlerMap.clear();
    delete mSharedHandler;
    mThreadIsRunning = false;
    try {
        if (mUUIDthreadPtr.get() != NULL)
            mUUIDthreadPtr->GetValue(100);
    } catch (...) {
    }
}

void ConfigManagerBase::RemoveHandler(const string& dir, bool delete_flag) {
    unordered_map<string, EventHandler*>::iterator itr = mDirEventHandlerMap.find(dir);
    if (itr != mDirEventHandlerMap.end()) {
        if (itr->second != mSharedHandler && delete_flag) {
            delete itr->second;
        }
        mDirEventHandlerMap.erase(itr);
    }
}

// this functions should only be called when register base dir
bool ConfigManagerBase::RegisterHandlers() {
    if (mSharedHandler == NULL) {
        mSharedHandler = new NormalEventHandler();
    }
    vector<Config*> sortedConfigs;
    vector<Config*> wildcardConfigs;
    for (unordered_map<string, Config*>::iterator itr = mNameConfigMap.begin(); itr != mNameConfigMap.end(); ++itr) {
        if (itr->second->mLogType == STREAM_LOG || itr->second->mLogType == PLUGIN_LOG)
            continue;
        if (itr->second->mWildcardPaths.size() == 0)
            sortedConfigs.push_back(itr->second);
        else
            wildcardConfigs.push_back(itr->second);
    }
    sort(sortedConfigs.begin(), sortedConfigs.end(), Config::CompareByPathLength);
    bool result = true;
    for (vector<Config*>::iterator itr = sortedConfigs.begin(); itr != sortedConfigs.end(); ++itr) {
        Config* config = *itr;
        if (!config->mDockerFileFlag) {
            result &= RegisterHandlers(config->mBasePath, config);
        } else {
            for (size_t i = 0; i < config->mDockerContainerPaths->size(); ++i) {
                result &= RegisterHandlers((*config->mDockerContainerPaths)[i].mContainerPath, config);
            }
        }
    }
    for (vector<Config*>::iterator itr = wildcardConfigs.begin(); itr != wildcardConfigs.end(); ++itr) {
        Config* config = *itr;
        if (!config->mDockerFileFlag) {
            RegisterWildcardPath(config, config->mWildcardPaths[0], 0);
        } else {
            for (size_t i = 0; i < config->mDockerContainerPaths->size(); ++i) {
                RegisterWildcardPath(config, (*config->mDockerContainerPaths)[i].mContainerPath, 0);
            }
        }
    }
    return result;
}

void ConfigManagerBase::RegisterWildcardPath(Config* config, const string& path, int32_t depth) {
    bool finish;
    if ((depth + 1) == ((int)config->mWildcardPaths.size() - 1))
        finish = true;
    else if ((depth + 1) < ((int)config->mWildcardPaths.size() - 1))
        finish = false;
    else
        return;

    // const path
    if (!config->mConstWildcardPaths[depth].empty()) {
        // stat directly
        string item = PathJoin(path, config->mConstWildcardPaths[depth]);
        fsutil::PathStat baseDirStat;
        if (!fsutil::PathStat::stat(item, baseDirStat)) {
            LOG_DEBUG(sLogger,
                      ("get wildcard dir info error: ", item)(config->mProjectName,
                                                              config->mCategory)("error", ErrnoToString(GetErrno())));
            return;
        }
        if (!baseDirStat.IsDir())
            return;
        if (finish) {
            DirRegisterStatus registerStatus = EventDispatcher::GetInstance()->IsDirRegistered(item);
            if (registerStatus == PATH_INODE_NOT_REGISTERED) {
                if (EventDispatcher::GetInstance()->RegisterEventHandler(item.c_str(), config, mSharedHandler))
                    RegisterDescendants(item, config, config->mMaxDepth < 0 ? 100 : config->mMaxDepth);
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
                                               config->GetProjectName(),
                                               config->GetCategory(),
                                               config->mRegion);
        LOG_WARNING(sLogger, ("Open dir fail", path.c_str())("errno", err));
        return;
    }
    fsutil::Entry ent;
    int32_t dirCount = 0;
    while (ent = dir.ReadNext()) {
        if (dirCount >= INT32_FLAG(wildcard_max_sub_dir_count)) {
            LOG_WARNING(
                sLogger,
                ("too many sub directoried for path", path)("dirCount", dirCount)("basePath", config->mBasePath));
            break;
        }
        if (!ent.IsDir())
            continue;

        ++dirCount;
        string item = PathJoin(path, ent.Name());

        // we should check match and then check finsh
        size_t dirIndex = config->mWildcardPaths[depth].size() + 1;
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
            const auto& firstWildcardPath = config->mWildcardPaths[0];
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

        if (fnmatch(&(config->mWildcardPaths[depth + 1].at(dirIndex)), ent.Name().c_str(), FNM_PATHNAME) == 0) {
            if (finish) {
                DirRegisterStatus registerStatus = EventDispatcher::GetInstance()->IsDirRegistered(item);
                if (registerStatus == PATH_INODE_NOT_REGISTERED) {
                    if (EventDispatcher::GetInstance()->RegisterEventHandler(item.c_str(), config, mSharedHandler))
                        RegisterDescendants(item, config, config->mMaxDepth < 0 ? 100 : config->mMaxDepth);
                }
            } else {
                RegisterWildcardPath(config, item, depth + 1);
            }
        }
    }
}

// this functions should only be called when register base dir
bool ConfigManagerBase::RegisterHandlers(const string& basePath, Config* config) {
    bool result = true;
    static set<string> notExistDirs;
    static int32_t lastErrorLogTime = time(NULL);
    if (notExistDirs.size() > 0 && (time(NULL) - lastErrorLogTime > 3600)) {
        string dirs;
        for (set<string>::iterator iter = notExistDirs.begin(); iter != notExistDirs.end(); ++iter) {
            dirs.append(*iter);
            dirs.append(" ");
        }
        lastErrorLogTime = time(NULL);
        notExistDirs.clear();
        LOG_WARNING(sLogger, ("logPath in config not exist", dirs));
    }
    if (!CheckExistance(basePath)) {
        if (!(config->mLogType == APSARA_LOG && basePath.find("/tmp") == 0)
            && basePath.find(LOG_LOCAL_DEFINED_PATH_PREFIX) != 0)
            notExistDirs.insert(basePath);
        if (EventDispatcher::GetInstance()->IsRegistered(basePath.c_str())) {
            EventDispatcher::GetInstance()->UnregisterAllDir(basePath);
            LOG_DEBUG(sLogger, ("logPath in config not exist, unregister existed monitor", basePath));
        }
        return result;
    }
    DirRegisterStatus registerStatus = EventDispatcher::GetInstance()->IsDirRegistered(basePath);
    if (registerStatus == PATH_INODE_REGISTERED || registerStatus == GET_REGISTER_STATUS_ERROR)
        return result;
    // dir in config is valid by default, do not call pathValidator
    result = EventDispatcher::GetInstance()->RegisterEventHandler(basePath.c_str(), config, mSharedHandler);
    // if we come into a failure, do not try to register others, there must be something wrong!
    if (!result)
        return result;

    if (config->mIsPreserve)
        result = RegisterDescendants(basePath, config, config->mMaxDepth < 0 ? 100 : config->mMaxDepth);
    else {
        // preserve_depth register
        int depth = config->mPreserveDepth;
        result = RegisterHandlersWithinDepth(basePath, config, depth);
    }
    return result;
}

bool ConfigManagerBase::MatchDirPattern(const Config* config, const string& dir) {
    for (size_t i = 0; i < config->mUnAcceptDirPattern.size(); ++i) {
        if (fnmatch(config->mUnAcceptDirPattern[i].c_str(), dir.c_str(), 0) == 0) {
            return false;
        }
    }
    if (config->IsDirectoryInBlacklist(dir))
        return false;
    return true;
}

bool ConfigManagerBase::RegisterDirectory(const std::string& source, const std::string& object) {
    // TODO��A potential bug: FindBestMatch will test @object with filePattern, which has very
    // low possibility to match a sub directory name, so here will return false in most cases.
    // e.g.: source: /path/to/monitor, file pattern: *.log, object: subdir.
    // Match(subdir, *.log) = false.
    Config* config = FindBestMatch(source, object);
    if (config != NULL && MatchDirPattern(config, source))
        return EventDispatcher::GetInstance()->RegisterEventHandler(source.c_str(), config, mSharedHandler);
    return false;
}

bool ConfigManagerBase::RegisterHandlersWithinDepth(const std::string& path, Config* config, int depth) {
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
                                               config->GetProjectName(),
                                               config->GetCategory(),
                                               config->mRegion);

        LOG_ERROR(sLogger, ("Open dir error: ", path.c_str())("errno", err));
        return false;
    }
    fsutil::Entry ent;
    while (ent = dir.ReadNext()) {
        string item = PathJoin(path, ent.Name());
        if (ent.IsDir() && MatchDirPattern(config, item)) {
            if (!(EventDispatcher::GetInstance()->RegisterEventHandler(item.c_str(), config, mSharedHandler))) {
                // break;// fail early, do not try to register others
                result = false;
            } else //sub dir will not be registered if parent dir fails
                RegisterHandlersWithinDepth(item, config, depth - 1);
        }
    }

    return result;
}

// path not terminated by '/', path already registered
bool ConfigManagerBase::RegisterDescendants(const string& path, Config* config, int withinDepth) {
    if (withinDepth <= 0) {
        return true;
    }

    fsutil::Dir dir(path);
    if (!dir.Open()) {
        auto err = GetErrno();
        LogtailAlarm::GetInstance()->SendAlarm(LOGDIR_PERMINSSION_ALARM,
                                               string("Failed to open dir : ") + path + ";\terrno : " + ToString(err),
                                               config->GetProjectName(),
                                               config->GetCategory(),
                                               config->mRegion);
        LOG_ERROR(sLogger, ("Open dir error: ", path.c_str())("errno", err));
        return false;
    }
    fsutil::Entry ent;
    bool result = true;
    while (ent = dir.ReadNext()) {
        string item = PathJoin(path, ent.Name());
        if (ent.IsDir() && MatchDirPattern(config, item)) {
            result = EventDispatcher::GetInstance()->RegisterEventHandler(item.c_str(), config, mSharedHandler);
            if (result)
                RegisterDescendants(item, config, withinDepth - 1);
        }
    }
    return result;
}

Config* ConfigManagerBase::FindStreamLogTagMatch(const std::string& tag) {
    for (unordered_map<string, Config*>::iterator it = mNameConfigMap.begin(); it != mNameConfigMap.end(); ++it) {
        if (it->second->mLogType == STREAM_LOG && it->second->mStreamLogTag == tag) {
            return it->second;
        }
    }
    return NULL;
}

void ConfigManagerBase::ClearConfigMatchCache() {
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

Config* ConfigManagerBase::FindBestMatch(const string& path, const string& name) {
    string cachedFileKey(path);
    cachedFileKey.push_back('<');
    cachedFileKey.append(name);
    bool acceptMultiConfig = AppConfig::GetInstance()->IsAcceptMultiConfig();
    {
        ScopedSpinLock cachedLock(mCacheFileConfigMapLock);
        std::unordered_map<string, std::pair<Config*, int32_t> >::iterator iter
            = mCacheFileConfigMap.find(cachedFileKey);
        if (iter != mCacheFileConfigMap.end()) {
            // if need report alarm, do not return, just continue to find all match and send alarm
            if (acceptMultiConfig || iter->second.second == 0
                || time(NULL) - iter->second.second < INT32_FLAG(multi_config_alarm_interval)) {
                return iter->second.first;
            }
        }
    }
    unordered_map<string, Config*>::iterator itr = mNameConfigMap.begin();
    Config* prevMatch = NULL;
    size_t prevLen = 0;
    int32_t preCreateTime = INT_MAX;
    size_t curLen = 0;
    uint32_t nameRepeat = 0;
    string logNameList;
    vector<Config*> multiConfigs;
    for (; itr != mNameConfigMap.end(); ++itr) {
        Config* config = itr->second;
        if (config->mLogType != STREAM_LOG && config->mLogType != PLUGIN_LOG) {
            // exclude __FUSE_CONFIG__
            if (itr->first == STRING_FLAG(fuse_customized_config_name)) {
                continue;
            }

            bool match = config->IsMatch(path, name);
            if (match) {
                // if force multi config, do not send alarm
                if (!name.empty() && !config->mAdvancedConfig.mForceMultiConfig) {
                    nameRepeat++;
                    logNameList.append("logstore:");
                    logNameList.append(config->mCategory);
                    logNameList.append(",config:");
                    logNameList.append(config->mConfigName);
                    logNameList.append(" ");
                    multiConfigs.push_back(config);
                }

                // note: best config is the one which length is longest and create time is nearest
                curLen = config->mBasePath.size();
                if (prevLen < curLen) {
                    prevMatch = config;
                    preCreateTime = config->mCreateTime;
                    prevLen = curLen;
                } else if (prevLen == curLen && prevMatch != NULL) {
                    if (prevMatch->mCreateTime > config->mCreateTime) {
                        prevMatch = config;
                        preCreateTime = config->mCreateTime;
                        prevLen = curLen;
                    }
                }
            }
        }
    }

    // file include in multi config, find config for path will not trigger this alarm
    // do not send alarm to config with mForceMultiConfig
    if (nameRepeat > 1 && !name.empty() && !acceptMultiConfig) {
        LOG_ERROR(sLogger,
                  ("file", path + '/' + name)("include in multi config", logNameList)("best", prevMatch->mConfigName));
        for (vector<Config*>::iterator iter = multiConfigs.begin(); iter != multiConfigs.end(); ++iter) {
            LogtailAlarm::GetInstance()->SendAlarm(
                MULTI_CONFIG_MATCH_ALARM,
                path + '/' + name + " include in multi config, best and oldest match: " + prevMatch->GetProjectName()
                    + ',' + prevMatch->GetCategory() + ',' + prevMatch->mConfigName + ", allmatch: " + logNameList,
                (*iter)->GetProjectName(),
                (*iter)->GetCategory(),
                (*iter)->mRegion);
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


int32_t
ConfigManagerBase::FindAllMatch(vector<Config*>& allConfig, const std::string& path, const std::string& name /*= ""*/) {
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
    unordered_map<string, Config*>::iterator itr = mNameConfigMap.begin();
    for (; itr != mNameConfigMap.end(); ++itr) {
        Config* config = itr->second;
        if (config->mLogType != STREAM_LOG && config->mLogType != PLUGIN_LOG) {
            // exclude __FUSE_CONFIG__
            if (itr->first == STRING_FLAG(fuse_customized_config_name)) {
                continue;
            }

            bool match = config->IsMatch(path, name);
            if (match) {
                allConfig.push_back(itr->second);
            }
        }
    }

    if (!name.empty() && allConfig.size() > static_cast<size_t>(maxMultiConfigSize)) {
        // only report log file alarm
        alarmFlag = true;
        sort(allConfig.begin(), allConfig.end(), Config::CompareByDepthAndCreateTime);
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

int32_t
ConfigManagerBase::FindMatchWithForceFlag(std::vector<Config*>& allConfig, const string& path, const string& name) {
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
    auto itr = mNameConfigMap.begin();
    Config* prevMatch = NULL;
    size_t prevLen = 0;
    int32_t preCreateTime = INT_MAX;
    size_t curLen = 0;
    uint32_t nameRepeat = 0;
    string logNameList;
    vector<Config*> multiConfigs;
    for (; itr != mNameConfigMap.end(); ++itr) {
        Config* config = itr->second;
        if (config->mLogType != STREAM_LOG && config->mLogType != PLUGIN_LOG) {
            // exclude __FUSE_CONFIG__
            if (itr->first == STRING_FLAG(fuse_customized_config_name)) {
                continue;
            }

            bool match = config->IsMatch(path, name);
            if (match) {
                // if force multi config, do not send alarm
                if (!name.empty() && !config->mAdvancedConfig.mForceMultiConfig) {
                    nameRepeat++;
                    logNameList.append("logstore:");
                    logNameList.append(config->mCategory);
                    logNameList.append(",config:");
                    logNameList.append(config->mConfigName);
                    logNameList.append(" ");
                    multiConfigs.push_back(config);
                }
                if (!config->mAdvancedConfig.mForceMultiConfig) {
                    // if not ForceMultiConfig, find best match in normal cofigs
                    // note: best config is the one which length is longest and create time is nearest
                    curLen = config->mBasePath.size();
                    if (prevLen < curLen) {
                        prevMatch = config;
                        preCreateTime = config->mCreateTime;
                        prevLen = curLen;
                    } else if (prevLen == curLen && prevMatch != NULL) {
                        if (prevMatch->mCreateTime > config->mCreateTime) {
                            prevMatch = config;
                            preCreateTime = config->mCreateTime;
                            prevLen = curLen;
                        }
                    }
                } else {
                    // save ForceMultiConfig
                    allConfig.push_back(config);
                }
            }
        }
    }

    bool alarmFlag = false;
    // file include in multi config, find config for path will not trigger this alarm
    // do not send alarm to config with mForceMultiConfig
    if (nameRepeat > 1 && !acceptMultiConfig && prevMatch != NULL) {
        alarmFlag = true;
        LOG_ERROR(sLogger,
                  ("file", path + '/' + name)("include in multi config", logNameList)("best", prevMatch->mConfigName));
        for (vector<Config*>::iterator iter = multiConfigs.begin(); iter != multiConfigs.end(); ++iter) {
            LogtailAlarm::GetInstance()->SendAlarm(
                MULTI_CONFIG_MATCH_ALARM,
                path + '/' + name + " include in multi config, best and oldest match: " + prevMatch->GetProjectName()
                    + ',' + prevMatch->GetCategory() + ',' + prevMatch->mConfigName + ", allmatch: " + logNameList,
                (*iter)->GetProjectName(),
                (*iter)->GetCategory(),
                (*iter)->mRegion);
        }
    }
    if (prevMatch != NULL) {
        allConfig.push_back(prevMatch);
    }
    {
        ScopedSpinLock cachedLock(mCacheFileAllConfigMapLock);
        // use operator [], force update time
        mCacheFileAllConfigMap[cachedFileKey] = std::make_pair(allConfig, alarmFlag ? (int32_t)time(NULL) : (int32_t)0);
    }
    return (int32_t)allConfig.size();
}

void ConfigManagerBase::SendAllMatchAlarm(const string& path,
                                          const string& name,
                                          vector<Config*>& allConfig,
                                          int32_t maxMultiConfigSize) {
    string allConfigNames;
    for (vector<Config*>::iterator iter = allConfig.begin(); iter != allConfig.end(); ++iter) {
        allConfigNames.append("[")
            .append((*iter)->GetProjectName())
            .append(" : ")
            .append((*iter)->GetCategory())
            .append(" : ")
            .append((*iter)->mConfigName)
            .append("]");
    }
    LOG_ERROR(sLogger,
              ("file", path + '/' + name)("include in too many configs", allConfig.size())(
                  "max multi config size", maxMultiConfigSize)("allconfigs", allConfigNames));
    for (vector<Config*>::iterator iter = allConfig.begin(); iter != allConfig.end(); ++iter)
        LogtailAlarm::GetInstance()->SendAlarm(
            TOO_MANY_CONFIG_ALARM,
            path + '/' + name + " include in too many configs:" + ToString(allConfig.size())
                + ", max multi config size : " + ToString(maxMultiConfigSize) + ", allmatch: " + allConfigNames,
            (*iter)->GetProjectName(),
            (*iter)->GetCategory(),
            (*iter)->mRegion);
}

Config* ConfigManagerBase::FindConfigByName(const std::string& configName) {
    unordered_map<string, Config*>::iterator itr = mNameConfigMap.find(configName);
    if (itr != mNameConfigMap.end()) {
        return itr->second;
    }
    return NULL;
}

void ConfigManagerBase::RemoveAllConfigs() {
    mAllDockerContainerPathMap.clear();

    // Save all configs' container path map into mAllDockerContainerPathMap for later reload.
    {
        PTScopedLock guard(mDockerContainerPathCmdLock);
        for (auto it = mNameConfigMap.begin(); it != mNameConfigMap.end(); ++it) {
            if (it->second->mDockerContainerPaths) {
                mAllDockerContainerPathMap[it->first] = it->second->mDockerContainerPaths;
            }
            delete it->second;
        }
    }

    mNameConfigMap.clear();
    ScopedSpinLock lock(mCacheFileConfigMapLock);
    mCacheFileConfigMap.clear();
    ScopedSpinLock allLock(mCacheFileAllConfigMapLock);
    mCacheFileAllConfigMap.clear();
    ClearProjects();
    ClearRegions();
}

std::string ConfigManagerBase::GetDefaultPubAliuid() {
    ScopedSpinLock lock(mDefaultPubAKLock);
    return mDefaultPubAliuid;
}

void ConfigManagerBase::SetDefaultPubAliuid(const std::string& aliuid) {
    ScopedSpinLock lock(mDefaultPubAKLock);
    mDefaultPubAliuid = aliuid;
}

std::string ConfigManagerBase::GetDefaultPubAccessKeyId() {
    ScopedSpinLock lock(mDefaultPubAKLock);
    return mDefaultPubAccessKeyId;
}

void ConfigManagerBase::SetDefaultPubAccessKeyId(const std::string& accessKeyId) {
    ScopedSpinLock lock(mDefaultPubAKLock);
    mDefaultPubAccessKeyId = accessKeyId;
}

std::string ConfigManagerBase::GetDefaultPubAccessKey() {
    ScopedSpinLock lock(mDefaultPubAKLock);
    return mDefaultPubAccessKey;
}

void ConfigManagerBase::SetDefaultPubAccessKey(const std::string& accessKey) {
    ScopedSpinLock lock(mDefaultPubAKLock);
    mDefaultPubAccessKey = accessKey;
}

int32_t ConfigManagerBase::GetUserAK(const string& aliuid, std::string& accessKeyId, std::string& accessKey) {
    PTScopedLock lock(mUserInfosLock);
    unordered_map<string, UserInfo*>::iterator iter = mUserInfos.find(aliuid);
    if (iter == mUserInfos.end()) {
        UserInfo* ui = new UserInfo(aliuid, STRING_FLAG(default_access_key_id), STRING_FLAG(default_access_key), 0);
        mUserInfos.insert(pair<string, UserInfo*>(aliuid, ui));
        accessKeyId = ui->accessKeyId;
        accessKey = ui->accessKey;
        return ui->updateTime;
    } else {
        accessKeyId = (iter->second)->accessKeyId;
        accessKey = (iter->second)->accessKey;
        return (iter->second)->updateTime;
    }
}

void ConfigManagerBase::SetUserAK(const string& aliuid, const std::string& accessKeyId, const std::string& accessKey) {
    PTScopedLock lock(mUserInfosLock);
    unordered_map<string, UserInfo*>::iterator iter = mUserInfos.find(aliuid);
    if (iter == mUserInfos.end()) {
        UserInfo* ui = new UserInfo(aliuid, accessKeyId, accessKey, 0);
        mUserInfos.insert(pair<string, UserInfo*>(aliuid, ui));
    } else {
        (iter->second)->accessKeyId = accessKeyId;
        (iter->second)->accessKey = accessKey;
        (iter->second)->updateTime = time(NULL);
    }
}

void ConfigManagerBase::GetAliuidSet(Json::Value& aliuidArray) {
    ScopedSpinLock lock(mAliuidSetLock);
    for (set<string>::iterator iter = mAliuidSet.begin(); iter != mAliuidSet.end(); ++iter)
        aliuidArray.append(Json::Value(*iter));
}

std::string ConfigManagerBase::GetAliuidSet() {
    ScopedSpinLock lock(mAliuidSetLock);
    string aliuids = "";
    for (set<string>::iterator iter = mAliuidSet.begin(); iter != mAliuidSet.end(); ++iter)
        aliuids.append(*iter).append(" ");
    return aliuids;
}

void ConfigManagerBase::InsertAliuidSet(const std::string& aliuid) {
    ScopedSpinLock lock(mAliuidSetLock);
    mAliuidSet.insert(aliuid);
}

void ConfigManagerBase::SetAliuidSet(const std::vector<std::string>& aliuidList) {
    ScopedSpinLock lock(mAliuidSetLock);
    mAliuidSet.clear();
    for (vector<string>::const_iterator iter = aliuidList.begin(); iter != aliuidList.end(); ++iter)
        mAliuidSet.insert(*iter);
}

void ConfigManagerBase::GetUserDefinedIdSet(Json::Value& userDefinedIdArray) {
    ScopedSpinLock lock(mUserDefinedIdSetLock);
    for (set<string>::iterator iter = mUserDefinedIdSet.begin(); iter != mUserDefinedIdSet.end(); ++iter)
        userDefinedIdArray.append(Json::Value(*iter));
}

std::string ConfigManagerBase::GetUserDefinedIdSet() {
    ScopedSpinLock lock(mUserDefinedIdSetLock);
    string userDefinedIds = "";
    int i = 0;
    for (set<string>::iterator iter = mUserDefinedIdSet.begin(); iter != mUserDefinedIdSet.end(); ++iter, ++i) {
        if (i > 0)
            userDefinedIds.append(" ");
        userDefinedIds.append(*iter);
    }
    return userDefinedIds;
}

void ConfigManagerBase::SetUserDefinedIdSet(const std::vector<std::string>& userDefinedIdList) {
    ScopedSpinLock lock(mUserDefinedIdSetLock);
    mUserDefinedIdSet.clear();
    for (vector<string>::const_iterator iter = userDefinedIdList.begin(); iter != userDefinedIdList.end(); ++iter) {
        string trimedId = TrimString(TrimString(*iter, ' ', ' '), '\n', '\n');
        if (trimedId.size() > 0)
            mUserDefinedIdSet.insert(trimedId);
    }
}

void ConfigManagerBase::SetDefaultProfileProjectName(const string& profileProjectName) {
    ScopedSpinLock lock(mProfileLock);
    mDefaultProfileProjectName = profileProjectName;
}

void ConfigManagerBase::SetProfileProjectName(const std::string& region, const std::string& profileProject) {
    ScopedSpinLock lock(mProfileLock);
    mAllProfileProjectNames[region] = profileProject;
}

std::string ConfigManagerBase::GetProfileProjectName(const std::string& region, bool* existFlag) {
    ScopedSpinLock lock(mProfileLock);
    if (region.empty()) {
        if (existFlag != NULL) {
            *existFlag = false;
        }
        return mDefaultProfileProjectName;
    }
    std::unordered_map<std::string, std::string>::iterator iter = mAllProfileProjectNames.find(region);
    if (iter == mAllProfileProjectNames.end()) {
        if (existFlag != NULL) {
            *existFlag = false;
        }
        return mDefaultProfileProjectName;
    }
    if (existFlag != NULL) {
        *existFlag = true;
    }
    return iter->second;
}

int32_t ConfigManagerBase::GetConfigUpdateTotalCount() {
    return mConfigUpdateTotal;
}

int32_t ConfigManagerBase::GetConfigUpdateItemTotalCount() {
    return mConfigUpdateItemTotal;
}

int32_t ConfigManagerBase::GetLastConfigUpdateTime() {
    return mLastConfigUpdateTime;
}

int32_t ConfigManagerBase::GetLastConfigGetTime() {
    return mLastConfigGetTime;
}

void ConfigManagerBase::RestLastConfigTime() {
    mLastConfigUpdateTime = 0;
    mLastConfigGetTime = 0;
}

void ConfigManagerBase::GetAllProfileRegion(std::vector<std::string>& allRegion) {
    ScopedSpinLock lock(mProfileLock);
    if (mAllProfileProjectNames.find(mDefaultProfileRegion) == mAllProfileProjectNames.end()) {
        allRegion.push_back(mDefaultProfileRegion);
    }
    for (std::unordered_map<std::string, std::string>::iterator iter = mAllProfileProjectNames.begin();
         iter != mAllProfileProjectNames.end();
         ++iter) {
        allRegion.push_back(iter->first);
    }
}

std::string ConfigManagerBase::GetDefaultProfileProjectName() {
    ScopedSpinLock lock(mProfileLock);
    return mDefaultProfileProjectName;
}

void ConfigManagerBase::SetDefaultProfileRegion(const string& profileRegion) {
    ScopedSpinLock lock(mProfileLock);
    mDefaultProfileRegion = profileRegion;
}

std::string ConfigManagerBase::GetDefaultProfileRegion() {
    ScopedSpinLock lock(mProfileLock);
    return mDefaultProfileRegion;
}

void ConfigManagerBase::ReloadLogtailSysConf() {
    string userDefinedId;
    std::vector<std::string> userDefinedIdVec;
    if (ReadFileContent(AppConfig::GetInstance()->GetLogtailSysConfDir() + STRING_FLAG(user_defined_id_file),
                        userDefinedId)) {
        mUserDefinedId = TrimString(TrimString(userDefinedId, '\n', '\n'), ' ', ' ');

        userDefinedIdVec = SplitString(userDefinedId, "\n");
    }
    const char* userDefinedIdEnv = getenv(STRING_FLAG(ilogtail_user_defined_id_env_name).c_str());
    if (userDefinedIdEnv != NULL && strlen(userDefinedIdEnv) > 0) {
        mEnvFlag = true;
        string userDefinedIdEnvStr(userDefinedIdEnv);
        userDefinedIdEnvStr = TrimString(TrimString(userDefinedIdEnvStr, ',', ','), ' ', ' ');
        static string s_userDefinedIdEnv;
        if (s_userDefinedIdEnv != userDefinedIdEnvStr) {
            LOG_INFO(sLogger, ("load user defined id from env", userDefinedIdEnvStr));
            s_userDefinedIdEnv = userDefinedIdEnvStr;
        }
        if (mUserDefinedId.empty()) {
            mUserDefinedId = userDefinedIdEnvStr;
        }

        std::vector<std::string> idVec = SplitString(userDefinedIdEnvStr, ",");
        userDefinedIdVec.insert(userDefinedIdVec.end(), idVec.begin(), idVec.end());
    }

    if (!userDefinedIdVec.empty()) {
        SetUserDefinedIdSet(userDefinedIdVec);
    }

    vector<string> aliuidList;
    ReadAliuidsFile(aliuidList);

    const char* aliuidEnv = getenv(STRING_FLAG(ilogtail_aliuid_env_name).c_str());
    if (aliuidEnv != NULL && strlen(aliuidEnv) > 0) {
        mEnvFlag = true;
        string aliuidEnvStr(aliuidEnv);
        static string s_lastAliuidEnv;
        if (s_lastAliuidEnv != aliuidEnvStr) {
            LOG_INFO(sLogger, ("load aliyun user id from env", aliuidEnv));
            s_lastAliuidEnv = aliuidEnvStr;
        }
        aliuidEnvStr = TrimString(TrimString(aliuidEnvStr, ' ', ' '), ',', ',');
        vector<string> aliuidEnvList = SplitString(aliuidEnvStr, ",");
        aliuidList.insert(aliuidList.end(), aliuidEnvList.begin(), aliuidEnvList.end());
    }

    if (!aliuidList.empty()) {
        SetAliuidSet(aliuidList);
    }
    string defaultPubAliuid = GetDefaultPubAliuid();
    if (defaultPubAliuid.size() > 0)
        InsertAliuidSet(defaultPubAliuid);
}

void ConfigManagerBase::CorrectionAliuidFile(const Json::Value& aliuidArray) {
    for (int idx = 0; idx < (int)aliuidArray.size(); ++idx) {
        if (aliuidArray[idx].isString()) {
            string aliuid = TrimString(aliuidArray[idx].asString());
            if (aliuid.empty())
                continue;
            string fileName = AppConfig::GetInstance()->GetLogtailSysConfDir() + STRING_FLAG(logtail_sys_conf_users_dir)
                + PATH_SEPARATOR + aliuid;
            int fd = open(fileName.c_str(), O_CREAT | O_EXCL, 0755);
            if (fd == -1) {
                int savedErrno = GetErrno();
                if (savedErrno != EEXIST)
                    LOG_ERROR(sLogger, ("correction aliuid file fail", fileName)("errno", ErrnoToString(savedErrno)));
            } else
                close(fd);
        }
    }
}

void ConfigManagerBase::CorrectionAliuidFile() {
    static std::set<string> sLastAliuidSet;

    {
        ScopedSpinLock lock(mAliuidSetLock);
        // correct aliuid file when set size change and aliuid count < 10
        if (sLastAliuidSet.size() < mAliuidSet.size() && mAliuidSet.size() < (size_t)10) {
            sLastAliuidSet = mAliuidSet;
        } else {
            return;
        }
    }

    for (auto iter = sLastAliuidSet.begin(); iter != sLastAliuidSet.end(); ++iter) {
        const std::string& aliuid = *iter;
        LOG_INFO(sLogger, ("save aliuid file to local file system, aliuid", aliuid));
        string fileName = AppConfig::GetInstance()->GetLogtailSysConfDir() + STRING_FLAG(logtail_sys_conf_users_dir)
            + PATH_SEPARATOR + aliuid;
        int fd = open(fileName.c_str(), O_CREAT | O_EXCL, 0755);
        if (fd == -1) {
            int savedErrno = errno;
            if (savedErrno != EEXIST)
                LOG_ERROR(sLogger, ("correction aliuid file fail", fileName)("errno", strerror(savedErrno)));
        } else
            close(fd);
    }
}

// TODO: Move to RuntimeUtil.
void ConfigManagerBase::CorrectionLogtailSysConfDir() {
    string rootDir = AppConfig::GetInstance()->GetLogtailSysConfDir();
    do {
        fsutil::Dir dir(rootDir);
        if (dir.Open())
            break;

        bool changeDir = false;
        int savedErrno = GetErrno();
        if (fsutil::Dir::IsEACCES(savedErrno) || fsutil::Dir::IsENOTDIR(savedErrno)) {
            changeDir = true;
            LOG_ERROR(sLogger, ("invalid operation on dir", rootDir)("Open dir error", ErrnoToString(savedErrno)));
        } else if (fsutil::Dir::IsENOENT(savedErrno)) {
            if (!Mkdir(rootDir)) {
                savedErrno = GetErrno();
                if (!IsEEXIST(savedErrno)) {
                    changeDir = true;
                    LOG_ERROR(sLogger, ("create user config dir fail", rootDir)("error", ErrnoToString(savedErrno)));
                }
            } else
                LOG_INFO(sLogger, ("create user config dir success", rootDir));
        }
        if (changeDir) {
            rootDir = GetProcessExecutionDir();
            LOG_WARNING(sLogger, ("use default user config dir instead", rootDir));
        }
    } while (0);

    string userDir = rootDir + STRING_FLAG(logtail_sys_conf_users_dir);
    do {
        fsutil::Dir dir(userDir);
        if (dir.Open())
            break;

        int savedErrno = GetErrno();
        if (fsutil::Dir::IsEACCES(savedErrno) || fsutil::Dir::IsENOTDIR(savedErrno)
            || fsutil::Dir::IsENOENT(savedErrno)) {
            LOG_INFO(sLogger, ("invalid aliuid conf dir", userDir)("error", ErrnoToString(savedErrno)));
            if (!Mkdir(userDir)) {
                savedErrno = GetErrno();
                if (!IsEEXIST(savedErrno)) {
                    LOG_ERROR(sLogger, ("recreate aliuid conf dir", userDir)("error", ErrnoToString(savedErrno)));
                }
            } else {
                LOG_INFO(sLogger, ("recreate aliuid conf dir success", userDir));
            }
        }
    } while (0);
}

void ConfigManagerBase::GetAllPluginConfig(std::vector<Config*>& configVec) {
    unordered_map<string, Config*>::iterator itr = mNameConfigMap.begin();
    for (; itr != mNameConfigMap.end(); ++itr) {
        if (itr->second->mLogType == PLUGIN_LOG || !itr->second->mPluginConfig.empty()) {
            configVec.push_back(itr->second);
        }
    }
}

std::vector<Config*> ConfigManagerBase::GetMatchedConfigs(const std::function<bool(Config*)>& condition) {
    std::vector<Config*> configs;
    for (auto& iter : mNameConfigMap) {
        if (condition(iter.second)) {
            configs.push_back(iter.second);
        }
    }
    return configs;
}

bool ConfigManagerBase::ParseTimeZoneOffsetSecond(const string& logTZ, int& logTZSecond) {
    if (logTZ.size() != strlen("GMT+08:00") || logTZ[6] != ':' || (logTZ[3] != '+' && logTZ[3] != '-')) {
        return false;
    }
    if (logTZ.find("GMT") != (size_t)0) {
        return false;
    }
    string hourStr = logTZ.substr(4, 2);
    string minitueStr = logTZ.substr(7, 2);
    logTZSecond = StringTo<int>(hourStr) * 3600 + StringTo<int>(minitueStr) * 60;
    if (logTZ[3] == '-') {
        logTZSecond = -logTZSecond;
    }
    return true;
}

void ConfigManagerBase::GetSensitiveKeys(const Json::Value& value, Config* pConfig) {
    for (Json::Value::const_iterator iter = value.begin(); iter != value.end(); ++iter) {
        const Json::Value& sensitiveItem = *iter;
        if (sensitiveItem.isMember("key") && sensitiveItem["key"].isString() && sensitiveItem.isMember("type")
            && sensitiveItem["type"].isString() && sensitiveItem.isMember("regex_begin")
            && sensitiveItem["regex_begin"].isString() && sensitiveItem.isMember("regex_content")
            && sensitiveItem["regex_content"].isString() && sensitiveItem.isMember("all")
            && sensitiveItem["all"].isBool()) {
            string key = sensitiveItem["key"].asString();
            string type = sensitiveItem["type"].asString();
            string regexBegin = sensitiveItem["regex_begin"].asString();
            string regexContent = sensitiveItem["regex_content"].asString();
            bool all = sensitiveItem["all"].asBool();
            int32_t opt = SensitiveWordCastOption::CONST_OPTION;
            if (type != "const" && type != "md5") {
                // do not throw when parse sensitive key error
                LOG_ERROR(sLogger, ("The sensitive key type is invalid, type", type));
                LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                       "The sensitive key type is invalid",
                                                       pConfig->GetProjectName(),
                                                       pConfig->GetCategory());
                break;
                //throw ExceptionBase(string("The sensitive key type is invalid : ") + type);
            }


            string constVal;
            if (type == "const") {
                opt = SensitiveWordCastOption::CONST_OPTION;
                if (sensitiveItem.isMember("const") && sensitiveItem["const"].isString()) {
                    constVal = sensitiveItem["const"].asString();
                } else {
                    // do not throw when parse sensitive key error
                    LOG_ERROR(sLogger, ("The sensitive key config is invalid, key", key));
                    LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                           "The sensitive key config is invalid",
                                                           pConfig->GetProjectName(),
                                                           pConfig->GetCategory());
                    break;
                    //throw ExceptionBase(string("The sensitive key config is invalid : ") + key);
                }
            } else {
                opt = SensitiveWordCastOption::MD5_OPTION;
            }
            constVal = string("\\1") + constVal;
            string regexStr = string("(") + regexBegin + ")" + regexContent;
            std::shared_ptr<re2::RE2> pRegex(new re2::RE2(regexStr));
            if (!pRegex->ok()) {
                string errorMsg = pRegex->error();
                errorMsg += string(", regex : ") + regexStr;
                // do not throw when parse sensitive key error
                LOG_ERROR(sLogger, ("The sensitive regex is invalid, error", errorMsg));
                LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                       string("The sensitive key regex is invalid, ") + errorMsg,
                                                       pConfig->GetProjectName(),
                                                       pConfig->GetCategory());
                break;
                //throw ExceptionBase(string("The sensitive regex is invalid, ") + errorMsg);
            }
            SensitiveWordCastOption sensOpt;
            sensOpt.key = key;
            sensOpt.constValue = constVal;
            sensOpt.option = opt;
            sensOpt.replaceAll = all;
            sensOpt.mRegex = pRegex;
            pConfig->mSensitiveWordCastOptions[key].push_back(sensOpt);
            LOG_DEBUG(sLogger,
                      ("add senstive cast option", pConfig->mConfigName)("key", key)("const val", constVal)(
                          "type", type)("regex", regexStr)("all flag", all));
        } else {
            // do not throw when parse sensitive key error
            LOG_ERROR(sLogger, ("The sensitive key config is invalid, config", pConfig->mConfigName));
            LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                   "The sensitive key config is invalid",
                                                   pConfig->GetProjectName(),
                                                   pConfig->GetCategory());
            //throw ExceptionBase(string("The sensitive key config is invalid, config : ") + pConfig->mConfigName);
        }
    }
}

bool ConfigManagerBase::GetLocalConfigUpdate() {
    bool fileUpdateFlag = GetLocalConfigFileUpdate();
    bool dirUpdateFlag = GetLocalConfigDirUpdate();
    bool yamlDirUpdateFlag = GetLocalYamlConfigDirUpdate();
    return fileUpdateFlag || dirUpdateFlag || yamlDirUpdateFlag;
}

bool ConfigManagerBase::GetLocalConfigFileUpdate() {
    bool updateFlag = false;
    // read local config
    Json::Value localLogJson;
    std::string localConfigPath = AppConfig::GetInstance()->GetLocalUserConfigPath();
    ParseConfResult userLogRes = ParseConfig(localConfigPath, localLogJson);
    if (userLogRes != CONFIG_OK) {
        if (userLogRes == CONFIG_NOT_EXIST)
            LOG_DEBUG(sLogger, ("load user config fail, file not exist", localConfigPath));
        else if (userLogRes == CONFIG_INVALID_FORMAT) {
            LOG_ERROR(sLogger, ("load user config fail, file content is not valid json", localConfigPath));
            LogtailAlarm::GetInstance()->SendAlarm(USER_CONFIG_ALARM, string("local user config is not valid json"));
        }
    } else {
        if (localLogJson != mLocalConfigJson) {
            LOG_INFO(sLogger, ("local config update, old config", mLocalConfigJson.toStyledString()));
            mLocalConfigJson = localLogJson;
            LOG_INFO(sLogger, ("local config update, new config", mLocalConfigJson.toStyledString()));
            updateFlag = true;
        }
    }
    return updateFlag;
}

bool ConfigManagerBase::GetLocalConfigDirUpdate() {
    bool updateFlag = false;
    // list dir
    static std::string localConfigDirPath = AppConfig::GetInstance()->GetLocalUserConfigDirPath();
    fsutil::Dir localConfigDir(localConfigDirPath);
    if (!localConfigDir.Open()) {
        int savedErrno = GetErrno();
        if (fsutil::Dir::IsEACCES(savedErrno) || fsutil::Dir::IsENOTDIR(savedErrno)
            || fsutil::Dir::IsENOENT(savedErrno)) {
            LOG_DEBUG(sLogger, ("invalid local conf dir", localConfigDirPath)("error", ErrnoToString(savedErrno)));
            if (!Mkdir(localConfigDirPath.c_str())) {
                savedErrno = errno;
                if (!IsEEXIST(savedErrno)) {
                    LOG_ERROR(sLogger,
                              ("create local conf dir failed", localConfigDirPath)("error", strerror(savedErrno)));
                }
            }
        }
    }
    if (!localConfigDir.Open()) {
        return updateFlag;
    }

    vector<string> v;
    fsutil::Entry ent;
    while (ent = localConfigDir.ReadNext()) {
        if (!ent.IsRegFile()) {
            continue;
        }

        string flName = ent.Name();
        if (!EndWith(flName, ".json")) {
            continue;
        }
        string fullPath = localConfigDirPath + flName;
        v.push_back(fullPath);
    }

    std::sort(v.begin(), v.end());
    std::unordered_map<std::string, Json::Value> localConfigDirMap;
    for (size_t i = 0; i < v.size(); i++) {
        Json::Value subConfJson;
        ParseConfResult res = ParseConfig(v[i], subConfJson);
        if (res == CONFIG_OK) {
            auto iter = mLocalConfigDirMap.find(v[i]);
            if (iter == mLocalConfigDirMap.end() || iter->second != subConfJson) {
                updateFlag = true;
                LOG_INFO(sLogger, ("local user config file loaded", v[i]));
            }
            localConfigDirMap[v[i]] = subConfJson;
        } else {
            LOG_INFO(sLogger, ("invalid local user config file", v[i]));
            continue;
        }
    }
    if (mLocalConfigDirMap.size() != localConfigDirMap.size()) {
        LOG_INFO(
            sLogger,
            ("local user config removed or added, last", mLocalConfigDirMap.size())("now", localConfigDirMap.size()));
        updateFlag = true;
        if (localConfigDirMap.size() < mLocalConfigDirMap.size()) {
            SetConfigRemoveFlag(true);
        }
    }
    mLocalConfigDirMap = localConfigDirMap;
    return updateFlag;
}

bool ConfigManagerBase::GetLocalYamlConfigDirUpdate() {
    bool updateFlag = false;
    // list dir
    static std::string localConfigDirPath = AppConfig::GetInstance()->GetLocalUserYamlConfigDirPath();
    fsutil::Dir localConfigDir(localConfigDirPath);
    if (!localConfigDir.Open()) {
        int savedErrno = GetErrno();
        if (fsutil::Dir::IsEACCES(savedErrno) || fsutil::Dir::IsENOTDIR(savedErrno)
            || fsutil::Dir::IsENOENT(savedErrno)) {
            LOG_DEBUG(sLogger, ("invalid local yaml conf dir", localConfigDirPath)("error", ErrnoToString(savedErrno)));
            if (!Mkdir(localConfigDirPath.c_str())) {
                savedErrno = errno;
                if (!IsEEXIST(savedErrno)) {
                    LOG_ERROR(sLogger,
                              ("create local conf yaml dir failed", localConfigDirPath)("error", strerror(savedErrno)));
                }
            }
        }
    }
    if (!localConfigDir.Open()) {
        return updateFlag;
    }

    std::vector<std::string> v;
    fsutil::Entry ent;
    while (ent = localConfigDir.ReadNext()) {
        if (!ent.IsRegFile()) {
            continue;
        }

        std::string flName = ent.Name();
        if (!EndWith(flName, ".yaml")) {
            continue;
        }
        std::string fullPath = localConfigDirPath + flName;
        v.push_back(fullPath);
    }

    std::sort(v.begin(), v.end());
    std::unordered_map<std::string, YAML::Node> localConfigDirMap;
    for (size_t i = 0; i < v.size(); i++) {
        YAML::Node subConfYaml;
        ParseConfResult res = ParseConfig(v[i], subConfYaml);
        if (res == CONFIG_OK) {
            auto iter = mLocalYamlConfigDirMap.find(v[i]);
            if (iter == mLocalYamlConfigDirMap.end() || !(iter->second == subConfYaml)) {
                updateFlag = true;
                LOG_INFO(sLogger, ("local user yaml config file loaded", v[i]));
            }
            localConfigDirMap[v[i]] = subConfYaml;
        } else {
            LOG_INFO(sLogger, ("invalid local user yaml config file", v[i]));
            continue;
        }
    }
    if (mLocalYamlConfigDirMap.size() != localConfigDirMap.size()) {
        LOG_INFO(sLogger,
                 ("local user yaml config removed or added, last",
                  mLocalYamlConfigDirMap.size())("now", localConfigDirMap.size()));
        updateFlag = true;
        if (localConfigDirMap.size() < mLocalYamlConfigDirMap.size()) {
            SetConfigRemoveFlag(true);
        }
    }
    mLocalYamlConfigDirMap = localConfigDirMap;
    return updateFlag;
}

// UpdateConfigJson deals with config (only user log config, @configJson) from runtime plugin.
// If @configJson is valid and something changed, update config.
int32_t ConfigManagerBase::UpdateConfigJson(const std::string& configJson) {
    if (IsUpdate() == true)
        return 1;

    if (!IsValidJson(configJson.c_str(), configJson.size())) {
        LOG_ERROR(sLogger, ("invalid config json", configJson));
        return 2;
    }

    Json::Value jsonRoot;
    Json::Reader jsonReader;
    bool parseOk = jsonReader.parse(configJson, jsonRoot);
    if (!parseOk) {
        LOG_ERROR(sLogger, ("invalid config json", configJson));
        return 2;
    }

    if (BOOL_FLAG(logtail_config_update_enable)) {
        if (jsonRoot == mConfigJson) {
            LOG_INFO(sLogger, ("same config", configJson));
            return 3;
        }
        mConfigJson = jsonRoot;
        DumpConfigToLocal(AppConfig::GetInstance()->GetUserConfigPath(), mConfigJson);
        SetConfigRemoveFlag(true);
        LOG_INFO(sLogger, ("Update logtail config", configJson));
        StartUpdateConfig();
        mConfigUpdateTotal++;
        mLastConfigUpdateTime = ((int32_t)time(NULL));
        return 0;
    }
    return 4;
}


// DumpConfigToLocal dumps @configJson to local file named @fileName.
// In general, Logtail will call this function when mConfigJson was changed.
bool ConfigManagerBase::DumpConfigToLocal(std::string fileName, const Json::Value& configJson) {
    if (fileName.size() == 0)
        return false;
    if (fileName[0] != '/')
        fileName = GetProcessExecutionDir() + fileName;
    std::ofstream fout(fileName.c_str());
    if (!fout) {
        LOG_ERROR(sLogger, ("open config file error", fileName));
        return false;
    }
    fout << configJson.toStyledString();
    fout.close();
    // chmod to 644
#if defined(__linux__)
    if (chmod(fileName.c_str(), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH) != 0) {
        LOG_ERROR(sLogger, ("change config file mode error", fileName)("error", ErrnoToString(GetErrno())));
        return false;
    }
    // TODO: Should Windows do this?
#endif
    return true;
}

void ConfigManagerBase::InitUpdateConfig(bool configExistFlag) {
    mProcessStartTime = time(NULL);
}

bool ConfigManagerBase::TryGetUUID() {
    mUUIDthreadPtr = CreateThread([this]() { GetUUIDThread(); });
    // wait 1000 ms
    for (int i = 0; i < 100; ++i) {
        usleep(10 * 1000);
        if (!GetUUID().empty()) {
            return true;
        }
    }
    return false;
}

bool ConfigManagerBase::GetUUIDThread() {
    std::string uuid;
#if defined(__aarch64__) || defined(__sw_64__)
    // DMI can not work on such platforms but might crash Logtail, disable.
#else
    uuid = CalculateDmiUUID();
#endif
    SetUUID(uuid);
    return true;
}

void ConfigManagerBase::AddHandlerToDelete(EventHandler* handler) {
    mHandlersToDelete.push_back(handler);
}

void ConfigManagerBase::DeleteHandlers() {
    for (size_t i = 0; i < mHandlersToDelete.size(); ++i) {
        if (mHandlersToDelete[i] != mSharedHandler && mHandlersToDelete[i] != NULL)
            delete mHandlersToDelete[i];
    }
    mHandlersToDelete.clear();
}

//find config domain socket data, find postfix like "_category"
Config* ConfigManagerBase::FindDSConfigByCategory(const std::string& dsCtegory) {
    for (unordered_map<std::string, Config*>::iterator iter = mNameConfigMap.begin(); iter != mNameConfigMap.end();
         ++iter) {
        if (dsCtegory == iter->second->mCategory
            && (iter->second->mLogType != STREAM_LOG && iter->second->mLogType != PLUGIN_LOG)) {
            return iter->second;
        }
    }
    return NULL;
}

// ReloadMappingConfig reloads mapping config.
// **Nobody use this feature now...**
void ConfigManagerBase::ReloadMappingConfig() {
    Json::Value confJson;
    if (ParseConfig(AppConfig::GetInstance()->GetMappingConfigPath(), confJson) != CONFIG_OK)
        return;
    PTScopedLock lock(mMappingPathsLock);
    mMappingPaths.clear();
    if (confJson.isObject() && confJson.isMember("log_paths")) {
        const Json::Value& logPathJson = confJson["log_paths"];
        if (logPathJson.isObject()) {
            Json::Value::Members ids = logPathJson.getMemberNames();
            for (size_t i = 0; i < ids.size(); i++)
                mMappingPaths[ids[i]] = GetStringValue(logPathJson, ids[i], "");
        }
    }
}

std::string ConfigManagerBase::GetMappingPath(const std::string& id) {
    PTScopedLock lock(mMappingPathsLock);
    unordered_map<string, string>::iterator iter = mMappingPaths.find(id);
    return iter == mMappingPaths.end() ? "" : mMappingPaths[id];
}

// GetRelatedConfigs calculates related configs of @path.
// Two kind of relations:
// 1. No wildcard path: the base path of Config is the prefix of @path and within depth.
// 2. Wildcard path: @path matches and within depth.
void ConfigManagerBase::GetRelatedConfigs(const std::string& path, std::vector<Config*>& configs) {
    for (std::unordered_map<string, Config*>::iterator iter = mNameConfigMap.begin(); iter != mNameConfigMap.end();
         ++iter) {
        if (iter->second->mLogType == STREAM_LOG || iter->second->mLogType == PLUGIN_LOG)
            continue;
        if (iter->second->IsMatch(path, "")) {
            configs.push_back(iter->second);
        }
    }
}

SensitiveWordCastOption::~SensitiveWordCastOption() {
}

std::string ConfigManagerBase::GetAllProjectsSet() {
    string result;
    ScopedSpinLock lock(mProjectSetLock);
    for (std::set<string>::iterator iter = mProjectSet.begin(); iter != mProjectSet.end(); ++iter) {
        result.append(*iter).append(" ");
    }
    return result;
}

void ConfigManagerBase::InsertProject(const std::string& project) {
    ScopedSpinLock lock(mProjectSetLock);
    mProjectSet.insert(project);
}

void ConfigManagerBase::ClearProjects() {
    ScopedSpinLock lock(mProjectSetLock);
    mProjectSet.clear();
}

void ConfigManagerBase::InsertRegion(const std::string& region) {
    ScopedSpinLock lock(mRegionSetLock);
    mRegionSet.insert(region);
}

void ConfigManagerBase::ClearRegions() {
    ScopedSpinLock lock(mRegionSetLock);
    mRegionSet.clear();
}

bool ConfigManagerBase::CheckRegion(const std::string& region) const {
    ScopedSpinLock lock(mRegionSetLock);
    return mRegionSet.find(region) != mRegionSet.end();
}

bool ConfigManagerBase::UpdateContainerPath(DockerContainerPathCmd* cmd) {
    mDockerContainerPathCmdLock.lock();
    mDockerContainerPathCmdVec.push_back(cmd);
    mDockerContainerPathCmdLock.unlock();
    return true;
}

bool ConfigManagerBase::DoUpdateContainerPaths() {
    mDockerContainerPathCmdLock.lock();
    std::vector<DockerContainerPathCmd*> tmpPathCmdVec = mDockerContainerPathCmdVec;
    mDockerContainerPathCmdVec.clear();
    mDockerContainerPathCmdLock.unlock();
    LOG_INFO(sLogger, ("update container path", tmpPathCmdVec.size()));
    for (size_t i = 0; i < tmpPathCmdVec.size(); ++i) {
        Config* config = FindConfigByName(tmpPathCmdVec[i]->mConfigName);
        if (config == NULL) {
            LOG_ERROR(sLogger,
                      ("invalid container path update cmd", tmpPathCmdVec[i]->mConfigName)("params",
                                                                                           tmpPathCmdVec[i]->mParams));
            continue;
        }
        if (tmpPathCmdVec[i]->mDeleteFlag) {
            if (config->DeleteDockerContainerPath(tmpPathCmdVec[i]->mParams)) {
                LOG_DEBUG(sLogger,
                          ("container path delete cmd success",
                           tmpPathCmdVec[i]->mConfigName)("params", tmpPathCmdVec[i]->mParams));
            } else {
                LOG_ERROR(sLogger,
                          ("container path delete cmd fail", tmpPathCmdVec[i]->mConfigName)("params",
                                                                                            tmpPathCmdVec[i]->mParams));
            }
        } else {
            if (config->UpdateDockerContainerPath(tmpPathCmdVec[i]->mParams, tmpPathCmdVec[i]->mUpdateAllFlag)) {
                LOG_DEBUG(sLogger,
                          ("container path update cmd success", tmpPathCmdVec[i]->mConfigName)(
                              "params", tmpPathCmdVec[i]->mParams)("all", tmpPathCmdVec[i]->mUpdateAllFlag));
            } else {
                LOG_ERROR(sLogger,
                          ("container path update cmd fail", tmpPathCmdVec[i]->mConfigName)(
                              "params", tmpPathCmdVec[i]->mParams)("all", tmpPathCmdVec[i]->mUpdateAllFlag));
            }
        }
        delete tmpPathCmdVec[i];
    }
    return true;
}

bool ConfigManagerBase::IsUpdateContainerPaths() {
    mDockerContainerPathCmdLock.lock();
    bool rst = false;
    for (size_t i = 0; i < mDockerContainerPathCmdVec.size(); ++i) {
        DockerContainerPathCmd* pCmd = mDockerContainerPathCmdVec[i];
        if (pCmd->mDeleteFlag) {
            rst = true;
            break;
        }
        Config* pConfig = FindConfigByName(pCmd->mConfigName);
        if (pConfig == NULL) {
            continue;
        }
        if (!pConfig->IsSameDockerContainerPath(pCmd->mParams, pCmd->mUpdateAllFlag)) {
            rst = true;
            break;
        }
    }
    if (mDockerContainerPathCmdVec.size() > 0) {
        LOG_INFO(sLogger, ("check container path update flag", rst)("size", mDockerContainerPathCmdVec.size()));
    }
    if (rst == false) {
        for (size_t i = 0; i < mDockerContainerPathCmdVec.size(); ++i) {
            delete mDockerContainerPathCmdVec[i];
        }
        mDockerContainerPathCmdVec.clear();
    }
    mDockerContainerPathCmdLock.unlock();

    /********** qps limit : only update docker config INT32_FLAG(max_docker_config_update_times) times in 3 minutes ********/
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
    // return false if s_lastUpdateCount >= INT32_FLAG(max_docker_config_update_times) and last update time is in same window
    return false;
    /************************************************************************************************************************/
}

bool ConfigManagerBase::UpdateContainerStopped(DockerContainerPathCmd* cmd) {
    PTScopedLock lock(mDockerContainerStoppedCmdLock);
    mDockerContainerStoppedCmdVec.push_back(cmd);
    return true;
}

void ConfigManagerBase::GetContainerStoppedEvents(std::vector<Event*>& eventVec) {
    std::vector<DockerContainerPathCmd*> cmdVec;
    {
        PTScopedLock lock(mDockerContainerStoppedCmdLock);
        cmdVec.swap(mDockerContainerStoppedCmdVec);
    }
    for (auto& cmd : cmdVec) {
        // find config and container's path, then emit stopped event
        Config* config = FindConfigByName(cmd->mConfigName);
        if (!config) {
            continue;
        }
        DockerContainerPath dockerContainerPath;
        if (!DockerContainerPath::ParseByJSONStr(cmd->mParams, dockerContainerPath)) {
            continue;
        }
        std::vector<DockerContainerPath>::iterator iter = config->mDockerContainerPaths->begin();
        std::vector<DockerContainerPath>::iterator iend = config->mDockerContainerPaths->end();
        for (; iter != iend; ++iter) {
            if (iter->mContainerID == dockerContainerPath.mContainerID) {
                break;
            }
        }
        if (iter == iend) {
            continue;
        }
        Event* pStoppedEvent = new Event(iter->mContainerPath, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, -1, 0);
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

void ConfigManagerBase::SaveDockerConfig() {
    string dockerPathConfigName = AppConfig::GetInstance()->GetDockerFilePathConfig();
    Json::Value dockerPathValueRoot;
    dockerPathValueRoot["version"] = Json::Value(STRING_FLAG(ilogtail_docker_path_version));
    Json::Value dockerPathValueDetail;
    mDockerContainerPathCmdLock.lock();
    for (unordered_map<string, Config*>::iterator it = mNameConfigMap.begin(); it != mNameConfigMap.end(); ++it) {
        if (it->second->mDockerContainerPaths != NULL) {
            std::vector<DockerContainerPath>& containerPathVec = *(it->second->mDockerContainerPaths);
            for (size_t i = 0; i < containerPathVec.size(); ++i) {
                Json::Value dockerPathValue;
                dockerPathValue["config_name"] = Json::Value(it->first);
                dockerPathValue["container_id"] = Json::Value(containerPathVec[i].mContainerID);
                dockerPathValue["params"] = Json::Value(containerPathVec[i].mJsonStr);
                dockerPathValueDetail.append(dockerPathValue);
            }
        }
    }
    mDockerContainerPathCmdLock.unlock();
    dockerPathValueRoot["detail"] = dockerPathValueDetail;
    string dockerInfo = dockerPathValueRoot.toStyledString();
    OverwriteFile(dockerPathConfigName, dockerInfo);
    LOG_INFO(sLogger, ("dump docker path info", dockerPathConfigName));
}

void ConfigManagerBase::LoadDockerConfig() {
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
    std::vector<DockerContainerPathCmd*> localPaths;
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

        DockerContainerPathCmd* cmd = new DockerContainerPathCmd(configName, false, params, false);
        localPaths.push_back(cmd);
    }
    mDockerContainerPathCmdLock.lock();
    localPaths.insert(localPaths.end(), mDockerContainerPathCmdVec.begin(), mDockerContainerPathCmdVec.end());
    mDockerContainerPathCmdVec = localPaths;
    mDockerContainerPathCmdLock.unlock();

    DoUpdateContainerPaths();
}

bool ConfigManagerBase::LoadMountPaths() {
    if (!AppConfig::GetInstance()->IsContainerMode())
        return false;
    std::ifstream is;
    is.open(AppConfig::GetInstance()->GetContainerMountConfigPath().c_str());
    if (!is.good()) {
        LOG_ERROR(sLogger,
                  ("container mount config path not exist", AppConfig::GetInstance()->GetContainerMountConfigPath()));
        return false;
    }

    is.seekg(0, ios::end);
    int len = is.tellg();
    is.seekg(0, ios::beg);
    char* buffer = new char[len + 1];
    memset(buffer, 0, len + 1);
    is.read(buffer, len);
    is.close();

    string jsonStr(buffer);
    delete[] buffer;

    PTScopedLock lock(mDockerMountPathsLock);
    if (jsonStr == mDockerMountPaths.mJsonStr) {
        return false;
    }
    bool parseResult = DockerMountPaths::ParseByJsonStr(jsonStr, mDockerMountPaths);
    return parseResult;
}

DockerMountPaths ConfigManagerBase::GetMountPaths() {
    PTScopedLock lock(mDockerMountPathsLock);
    return mDockerMountPaths;
}

#ifdef APSARA_UNIT_TEST_MAIN
void ConfigManagerBase::CleanEnviroments() {
    RemoveAllConfigs();
    for (std::unordered_map<std::string, EventHandler*>::iterator iter = mDirEventHandlerMap.begin();
         iter != mDirEventHandlerMap.end();
         ++iter) {
        if (iter->second != mSharedHandler && iter->second)
            delete iter->second;
    }
    mDirEventHandlerMap.clear();

    // set it to null
    mLocalConfigJson = Json::Value();
}
#endif

void ReplaceEnvVarRefInConf(Json::Value& value) {
    if (value.isString()) {
        Json::Value tempValue{replaceEnvVarRefInStr(value.asString())};
        value.swapPayload(tempValue);
    } else if (value.isArray()) {
        Json::ValueIterator it = value.begin();
        Json::ValueIterator end = value.end();
        for (; it != end; ++it) {
            ReplaceEnvVarRefInConf(*it);
        }
    } else if (value.isObject()) {
        Json::ValueIterator it = value.begin();
        Json::ValueIterator end = value.end();
        for (; it != end; ++it) {
            ReplaceEnvVarRefInConf(*it);
        }
    }
}

static std::string unescapeDollar(std::string::const_iterator beginIt, std::string::const_iterator endIt) {
    std::string outStr;
    std::string::const_iterator lastMatchEnd = beginIt;
    static boost::regex reg(R"(\$\$|\$})");
    boost::regex_iterator<std::string::const_iterator> it{beginIt, endIt, reg};
    boost::regex_iterator<std::string::const_iterator> end;
    for (; it != end; ++it) {
        outStr.append(lastMatchEnd, (*it)[0].first); // original part
        outStr.append((*it)[0].first + 1, (*it)[0].second); // skip $ char
        lastMatchEnd = (*it)[0].second;
    }
    outStr.append(lastMatchEnd, endIt); // original part
    return outStr;
}

std::string replaceEnvVarRefInStr(const std::string& inStr) {
    std::string outStr;
    std::string::const_iterator lastMatchEnd = inStr.begin();
    static boost::regex reg(R"((?<!\$)\${([\w]+)(:(.*?))?(?<!\$)})");
    boost::regex_iterator<std::string::const_iterator> it{inStr.begin(), inStr.end(), reg};
    boost::regex_iterator<std::string::const_iterator> end;
    for (; it != end; ++it) {
        outStr.append(unescapeDollar(lastMatchEnd, (*it)[0].first)); // original part
        char* env = getenv((*it)[1].str().c_str());
        if (env != NULL) // replace to enviroment variable
        {
            outStr.append(env);
        } else if ((*it).size() == 4) // replace to default value
        {
            outStr.append((*it)[3].first, (*it)[3].second);
        }
        // else replace to empty string (do nothing)
        lastMatchEnd = (*it)[0].second;
    }
    outStr.append(unescapeDollar(lastMatchEnd, inStr.end())); // original part
    return outStr;
}

} // namespace logtail
