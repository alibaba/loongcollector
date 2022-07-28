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

#include "LogtailPlugin.h"
//#include "LogtailPluginAdapter.h"
#if defined(__linux__)
#include <dlfcn.h>
#elif defined(_MSC_VER)
#include <Windows.h>
#endif
#include "common/LogtailCommonFlags.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "config_manager/ConfigManager.h"
#include "sender/Sender.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogFileProfiler.h"
#include "app_config/AppConfig.h"
using namespace std;
using namespace logtail;

class DynamicLibLoader {
    void* mLibPtr = nullptr;

    std::string GetError() {
#if defined(__linux__)
        auto dlErr = dlerror();
        return (dlErr != NULL) ? dlErr : "";
#elif defined(_MSC_VER)
        return std::to_string(GetLastError());
#endif
    }

public:
    static void CloseLib(void* libPtr) {
        if (nullptr == libPtr)
            return;
#if defined(__linux__)
        dlclose(libPtr);
#elif defined(_MSC_VER)
        FreeLibrary((HINSTANCE)libPtr);
#endif
    }

    // Release releases the ownership of @mLibPtr to caller.
    void* Release() {
        auto ptr = mLibPtr;
        mLibPtr = nullptr;
        return ptr;
    }

    ~DynamicLibLoader() { CloseLib(mLibPtr); }

    // LoadDynLib loads dynamic library named @libName from current working dir.
    // For linux, the so name is 'lib+@libName.so'.
    // For Windows, the dll name is '@libName.dll'.
    // @return a non-NULL ptr to indicate lib handle, otherwise nullptr is returned.
    bool LoadDynLib(const std::string& libName, std::string& error) {
        auto workingDir = AppConfig::GetInstance()->GetWorkingDir();
        error.clear();

#if defined(__linux__)
        mLibPtr = dlopen((workingDir + "lib" + libName + ".so").c_str(), RTLD_LAZY);
#elif defined(_MSC_VER)
        mLibPtr = LoadLibrary((workingDir + libName + ".dll").c_str());
#endif
        if (mLibPtr != NULL)
            return true;
        error = GetError();
        return false;
    }

    // LoadMethod loads method named @methodName from opened lib.
    // If error is found, @error param will be set, otherwise empty.
    void* LoadMethod(const std::string& methodName, std::string& error) {
        error.clear();

#if defined(__linux__)
        dlerror(); // Clear last error.
        auto methodPtr = dlsym(mLibPtr, methodName.c_str());
        error = GetError();
        return methodPtr;
#elif defined(_MSC_VER)
        auto methodPtr = GetProcAddress((HINSTANCE)mLibPtr, methodName.c_str());
        if (methodPtr != NULL)
            return methodPtr;
        error = std::to_string(GetLastError());
        return NULL;
#endif
    }
};

LogtailPlugin* LogtailPlugin::s_instance = NULL;

LogtailPlugin::LogtailPlugin() {
    mPluginAdapterPtr = NULL;
    mPluginBasePtr = NULL;
    mLoadConfigFun = NULL;
    mHoldOnFun = NULL;
    mResumeFun = NULL;
    mLoadGlobalConfigFun = NULL;
    mProcessRawLogFun = NULL;
    mPluginValid = false;
    mPluginAlarmConfig.mCategory = "logtail_alarm";
    mPluginAlarmConfig.mAliuid = STRING_FLAG(logtail_profile_aliuid);
    mPluginAlarmConfig.mLogstoreKey = 0;
    mPluginProfileConfig.mCategory = "shennong_log_profile";
    mPluginProfileConfig.mAliuid = STRING_FLAG(logtail_profile_aliuid);
    mPluginProfileConfig.mLogstoreKey = 0;

    mPluginCfg["LogtailSysConfDir"] = AppConfig::GetInstance()->GetLogtailSysConfDir();
    mPluginCfg["HostIP"] = LogFileProfiler::mIpAddr;
    mPluginCfg["Hostname"] = LogFileProfiler::mHostname;
}

LogtailPlugin::~LogtailPlugin() {
    DynamicLibLoader::CloseLib(mPluginBasePtr);
    DynamicLibLoader::CloseLib(mPluginAdapterPtr);
}

void LogtailPlugin::LoadConfig() {
    vector<Config*> pluginConfigs;
    ConfigManager::GetInstance()->GetAllPluginConfig(pluginConfigs);
    if (pluginConfigs.size() > 0) {
        for (size_t i = 0; i < pluginConfigs.size(); ++i) {
            Config* pConfig = pluginConfigs[i];
            GoString goProject;
            GoString goLogstore;
            GoString goConfigName;
            GoString goPluginConfig;

            goProject.n = pConfig->mProjectName.size();
            goProject.p = pConfig->mProjectName.c_str();
            goLogstore.n = pConfig->mCategory.size();
            goLogstore.p = pConfig->mCategory.c_str();
            goConfigName.n = pConfig->mConfigName.size();
            goConfigName.p = pConfig->mConfigName.c_str();
            goPluginConfig.n = pConfig->mPluginConfig.size();
            goPluginConfig.p = pConfig->mPluginConfig.c_str();

            long long logStoreKey = pConfig->mLogstoreKey;
            if (mPluginValid && mLoadConfigFun != NULL) {
                GoInt loadRst = mLoadConfigFun(goProject, goLogstore, goConfigName, logStoreKey, goPluginConfig);
                if (loadRst != 0) {
                    LOG_WARNING(sLogger,
                                ("load plugin error",
                                 pConfig->mProjectName)(pConfig->mCategory, pConfig->mPluginConfig)("result", loadRst));
                    LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                           string("load plugin config error, invalid config. please "
                                                                  "check you config and logtail's plugin log."),
                                                           pConfig->GetProjectName(),
                                                           pConfig->GetCategory(),
                                                           pConfig->mRegion);
                }
            }
        }
    }
}

void LogtailPlugin::HoldOn(bool exitFlag) {
    if (mPluginValid && mHoldOnFun != NULL) {
        LOG_INFO(sLogger, ("logtail plugin HoldOn", "start"));
        auto holdOnStart = GetCurrentTimeInMilliSeconds();
        mHoldOnFun(exitFlag ? 1 : 0);
        auto holdOnCost = GetCurrentTimeInMilliSeconds() - holdOnStart;
        LOG_INFO(sLogger, ("logtail plugin HoldOn", "success")("cost", holdOnCost));
        if (holdOnCost >= 60 * 1000) {
            LogtailAlarm::GetInstance()->SendAlarm(HOLD_ON_TOO_SLOW_ALARM,
                                                   "Plugin HoldOn is too slow: " + std::to_string(holdOnCost));
        }
    }
}

void LogtailPlugin::Resume() {
    LoadPluginBase();
    LoadConfig();
    if (mPluginValid && mResumeFun != NULL) {
        LOG_INFO(sLogger, ("logtail plugin Resume", "start"));
        mResumeFun();
        LOG_INFO(sLogger, ("logtail plugin Resume", "success"));
    }
}

void LogtailPlugin::ProcessRawLog(const std::string& configName,
                                  char* rawLog,
                                  int32_t rawLogSize,
                                  const std::string& packId,
                                  const std::string& topic) {
    if (rawLogSize <= 0)
        return;

    if (mPluginValid && mProcessRawLogFun != NULL) {
        GoString goConfigName;
        GoSlice goRawLog;
        GoString goPackId;
        GoString goTopic;

        goConfigName.n = configName.size();
        goConfigName.p = configName.c_str();
        goRawLog.len = rawLogSize - 1;
        goRawLog.cap = rawLogSize - 1;
        goRawLog.data = (void*)rawLog;
        goPackId.n = packId.size();
        goPackId.p = packId.c_str();
        goTopic.n = topic.size();
        goTopic.p = topic.c_str();
        GoInt rst = mProcessRawLogFun(goConfigName, goRawLog, goPackId, goTopic);
        if (rst != (GoInt)0) {
            LOG_WARNING(sLogger, ("process raw log error", configName)("result", rst));
        }
    }
}

const std::string tagDelimiter = "^^^";
const std::string tagSeparator = "~=~";
const std::string tagPrefix = "__tag__:";

void LogtailPlugin::ProcessRawLogV2(const std::string& configName,
                                    char* rawLog,
                                    int32_t rawLogSize,
                                    const std::string& packId,
                                    const std::string& topic,
                                    const std::string& tags) {
    if (rawLogSize <= 0 || !(mPluginValid && mProcessRawLogV2Fun != NULL)) {
        return;
    }

    GoString goConfigName;
    GoSlice goRawLog;
    GoString goPackId;
    GoString goTopic;
    GoSlice goTags;

    goConfigName.n = configName.size();
    goConfigName.p = configName.c_str();
    goRawLog.data = (void*)rawLog;
    goRawLog.len = rawLogSize - 1;
    goRawLog.cap = rawLogSize - 1;
    goPackId.n = packId.size();
    goPackId.p = packId.c_str();
    goTopic.n = topic.size();
    goTopic.p = topic.c_str();
    goTags.data = (void*)tags.c_str();
    goTags.len = goTags.cap = tags.length();

    GoInt rst = mProcessRawLogV2Fun(goConfigName, goRawLog, goPackId, goTopic, goTags);
    if (rst != (GoInt)0) {
        LOG_WARNING(sLogger, ("process raw log V2 error", configName)("result", rst));
    }
}

int LogtailPlugin::IsValidToSend(long long logstoreKey) {
    return Sender::Instance()->GetSenderFeedBackInterface()->IsValidToPush(logstoreKey) ? 0 : -1;
}

int LogtailPlugin::SendPb(const char* configName,
                          int32_t configNameSize,
                          const char* logstoreName,
                          int logstoreSize,
                          char* pbBuffer,
                          int32_t pbSize,
                          int32_t lines) {
    return SendPbV2(configName, configNameSize, logstoreName, logstoreSize, pbBuffer, pbSize, lines, NULL, 0);
}

int LogtailPlugin::SendPbV2(const char* configName,
                            int32_t configNameSize,
                            const char* logstoreName,
                            int logstoreSize,
                            char* pbBuffer,
                            int32_t pbSize,
                            int32_t lines,
                            const char* shardHash,
                            int shardHashSize) {
    static Config* alarmConfig = &(LogtailPlugin::GetInstance()->mPluginAlarmConfig);
    static Config* profileConfig = &(LogtailPlugin::GetInstance()->mPluginProfileConfig);
    string configNameStr = string(configName, configNameSize);

    string logstore;
    if (logstoreSize > 0 && logstoreName != NULL) {
        logstore.assign(logstoreName, (size_t)logstoreSize);
    }

    //LOG_DEBUG(sLogger, ("send pb", configNameStr)("pb size", pbSize)("lines", lines));
    Config* pConfig = NULL;
    if (configNameStr == alarmConfig->mCategory) {
        pConfig = alarmConfig;
        pConfig->mProjectName = ConfigManager::GetInstance()->GetDefaultProfileProjectName();
        pConfig->mRegion = ConfigManager::GetInstance()->GetDefaultProfileRegion();
        if (0 == pConfig->mProjectName.size()) {
            return 0;
        }
    } else if (configNameStr == profileConfig->mCategory) {
        pConfig = profileConfig;
        pConfig->mProjectName = ConfigManager::GetInstance()->GetDefaultProfileProjectName();
        pConfig->mRegion = ConfigManager::GetInstance()->GetDefaultProfileRegion();
        if (0 == pConfig->mProjectName.size()) {
            return 0;
        }
    } else {
        pConfig = ConfigManager::GetInstance()->FindConfigByName(configNameStr);
    }
    if (pConfig != NULL) {
        std::string shardHashStr;
        if (shardHashSize > 0) {
            shardHashStr.assign(shardHash, static_cast<size_t>(shardHashSize));
        }
        return Sender::Instance()->SendPb(pConfig, pbBuffer, pbSize, lines, logstore, shardHashStr) ? 0 : -1;
    } else {
        LOG_ERROR(sLogger, ("error", "SendPbV2 can not find config")("config", configNameStr));
    }
    return -2;
}

int LogtailPlugin::ExecPluginCmd(
    const char* configName, int configNameSize, int cmdId, const char* params, int paramsLen) {
    if (cmdId < (int)PLUGIN_CMD_MIN || cmdId > (int)PLUGIN_CMD_MAX) {
        LOG_ERROR(sLogger, ("invalid cmd", cmdId));
        return -2;
    }
    string configNameStr(configName, configNameSize);
    string paramsStr(params, paramsLen);
    PluginCmdType cmdType = (PluginCmdType)cmdId;
    LOG_DEBUG(sLogger, ("exec cmd", cmdType)("config", configNameStr)("detail", paramsStr));
    switch (cmdType) {
        case PLUGIN_DOCKER_UPDATE_FILE: {
            DockerContainerPathCmd* cmd = new DockerContainerPathCmd(configNameStr, false, paramsStr, false);
            ConfigManager::GetInstance()->UpdateContainerPath(cmd);
        } break;
        case PLUGIN_DOCKER_REMOVE_FILE: {
            DockerContainerPathCmd* cmd = new DockerContainerPathCmd(configNameStr, true, paramsStr, false);
            ConfigManager::GetInstance()->UpdateContainerPath(cmd);
            cmd = new DockerContainerPathCmd(configNameStr, true, paramsStr, false);
            ConfigManager::GetInstance()->UpdateContainerStopped(cmd);
        } break;
        case PLUGIN_DOCKER_UPDATE_FILE_ALL: {
            DockerContainerPathCmd* cmd = new DockerContainerPathCmd(configNameStr, false, paramsStr, true);
            ConfigManager::GetInstance()->UpdateContainerPath(cmd);
        } break;
        default:
            LOG_ERROR(sLogger, ("unknown cmd", cmdType));
            break;
    }
    return 0;
}


bool LogtailPlugin::LoadPluginBase() {
    if (mPluginValid)
        return true;
    vector<Config*> pluginConfigs;
    ConfigManager::GetInstance()->GetAllPluginConfig(pluginConfigs);
    const char* dockerEnvConfig = getenv("ALICLOUD_LOG_DOCKER_ENV_CONFIG");
    bool dockerEnvConfigEnabled = (dockerEnvConfig != NULL && strlen(dockerEnvConfig) > 0
                                   && (dockerEnvConfig[0] == 't' || dockerEnvConfig[0] == 'T'));
    if (pluginConfigs.size() == (size_t)0 && !dockerEnvConfigEnabled) {
        LOG_INFO(sLogger, ("no plugin config and no docker env config, do not load plugin base", ""));
        return true;
    }
    LOG_INFO(sLogger,
             ("load plugin base, config count", pluginConfigs.size())("docker env config", dockerEnvConfigEnabled)(
                 "dl file", (AppConfig::GetInstance()->GetWorkingDir() + "libPluginAdapter.so")));

    // load plugin adapter
    if (mPluginAdapterPtr == NULL) {
        DynamicLibLoader loader;
        std::string error;
        if (!loader.LoadDynLib("PluginAdapter", error)) {
            LOG_ERROR(sLogger, ("open adapter lib error, Message", error));
            return false;
        }

        auto versionFun = (PluginAdapterVersion)loader.LoadMethod("PluginAdapterVersion", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load version function error, Message", error));
            return false;
        }
        int version = versionFun();
        if (!(version / 100 == 2 || version / 100 == 3)) {
            LOG_ERROR(sLogger, ("check plugin adapter version error, version", version));
            return false;
        }
        LOG_INFO(sLogger, ("check plugin adapter version success, version", version));

        // Be compatible with old libPluginAdapter.so, V2 -> V1.
        auto registerV2Fun = (RegisterLogtailCallBackV2)loader.LoadMethod("RegisterLogtailCallBackV2", error);
        if (error.empty()) {
            registerV2Fun(LogtailPlugin::IsValidToSend,
                          LogtailPlugin::SendPb,
                          LogtailPlugin::SendPbV2,
                          LogtailPlugin::ExecPluginCmd);
        } else {
            LOG_WARNING(sLogger, ("load RegisterLogtailCallBackV2 failed", error)("try to load V1", ""));

            auto registerFun = (RegisterLogtailCallBack)loader.LoadMethod("RegisterLogtailCallBack", error);
            if (!error.empty()) {
                LOG_WARNING(sLogger, ("load RegisterLogtailCallBack failed", error));
                return false;
            }
            registerFun(LogtailPlugin::IsValidToSend, LogtailPlugin::SendPb, LogtailPlugin::ExecPluginCmd);
        }

        mPluginAdapterPtr = loader.Release();
    }

    InitPluginBaseFun initBase = NULL;
    InitPluginBaseV2Fun initBaseV2 = NULL;
    // load plugin base
    if (mPluginBasePtr == NULL) {
        DynamicLibLoader loader;
        std::string error;
        if (!loader.LoadDynLib("PluginBase", error)) {
            LOG_ERROR(sLogger, ("open plugin base dl error, Message", error));
            return false;
        }

        // Try V2 -> V1.
        initBaseV2 = (InitPluginBaseV2Fun)loader.LoadMethod("InitPluginBaseV2", error);
        if (!error.empty()) {
            LOG_WARNING(sLogger, ("load InitPluginBaseFunV2 error", error)("try to load V1", ""));

            initBase = (InitPluginBaseFun)loader.LoadMethod("InitPluginBase", error);
            if (!error.empty()) {
                LOG_ERROR(sLogger, ("load InitPluginBase error", error));
                return false;
            }
        }
        mLoadGlobalConfigFun = (LoadGlobalConfigFun)loader.LoadMethod("LoadGlobalConfig", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load LoadGlobalConfig error, Message", error));
            return false;
        }
        mLoadConfigFun = (LoadConfigFun)loader.LoadMethod("LoadConfig", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load LoadConfig error, Message", error));
            return false;
        }
        mUnloadConfigFun = (UnloadConfigFun)loader.LoadMethod("UnloadConfig", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load UnloadConfig error, Message", error));
            return false;
        }
        mHoldOnFun = (HoldOnFun)loader.LoadMethod("HoldOn", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load HoldOn error, Message", error));
            return false;
        }
        mResumeFun = (ResumeFun)loader.LoadMethod("Resume", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load Resume error, Message", error));
            return false;
        }
        mProcessRawLogFun = (ProcessRawLogFun)loader.LoadMethod("ProcessRawLog", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load ProcessRawLog error, Message", error));
            return false;
        }
        mProcessRawLogV2Fun = (ProcessRawLogV2Fun)loader.LoadMethod("ProcessRawLogV2", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load ProcessRawLogV2 error, Message", error));
            return false;
        }

        mPluginBasePtr = loader.Release();
    }

    GoInt initRst = 0;
    if (initBaseV2) {
        std::string cfgStr = mPluginCfg.toStyledString();
        GoString goCfgStr;
        goCfgStr.p = cfgStr.c_str();
        goCfgStr.n = cfgStr.length();

        initRst = initBaseV2(goCfgStr);
    } else {
        initRst = initBase();
    }
    if (initRst != 0) {
        LOG_ERROR(sLogger, ("init plugin base error", initRst));
        mPluginValid = false;
    } else {
        LOG_INFO(sLogger, ("init plugin base", "success"));
        mPluginValid = true;
    }
    return mPluginValid;
}
