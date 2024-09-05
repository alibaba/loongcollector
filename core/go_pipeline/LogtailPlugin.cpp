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

#include "go_pipeline/LogtailPlugin.h"

#include <json/json.h>

#include "app_config/AppConfig.h"
#include "common/DynamicLibHelper.h"
#include "common/HashUtil.h"
#include "common/JsonUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/TimeUtil.h"
#include "pipeline/compression/CompressorFactory.h"
#include "container_manager/ConfigContainerInfoUpdateCmd.h"
#include "file_server/ConfigManager.h"
#include "logger/Logger.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogtailAlarm.h"
#include "pipeline/PipelineManager.h"
#include "profile_sender/ProfileSender.h"
#include "pipeline/queue/SenderQueueManager.h"

DEFINE_FLAG_BOOL(enable_sls_metrics_format, "if enable format metrics in SLS metricstore log pattern", false);
DEFINE_FLAG_BOOL(enable_containerd_upper_dir_detect,
                 "if enable containerd upper dir detect when locating rootfs",
                 false);

using namespace std;
using namespace logtail;

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
    mPluginAlarmConfig.mLogstore = "logtail_alarm";
    mPluginAlarmConfig.mAliuid = STRING_FLAG(logtail_profile_aliuid);
    mPluginAlarmConfig.mCompressor
        = CompressorFactory::GetInstance()->Create(Json::Value(), PipelineContext(), "flusher_sls", CompressType::ZSTD);
    mPluginProfileConfig.mLogstore = "shennong_log_profile";
    mPluginProfileConfig.mAliuid = STRING_FLAG(logtail_profile_aliuid);
    mPluginProfileConfig.mCompressor
        = CompressorFactory::GetInstance()->Create(Json::Value(), PipelineContext(), "flusher_sls", CompressType::ZSTD);
    mPluginContainerConfig.mLogstore = "logtail_containers";
    mPluginContainerConfig.mAliuid = STRING_FLAG(logtail_profile_aliuid);
    mPluginContainerConfig.mCompressor
        = CompressorFactory::GetInstance()->Create(Json::Value(), PipelineContext(), "flusher_sls", CompressType::ZSTD);

    mPluginCfg["LogtailSysConfDir"] = AppConfig::GetInstance()->GetLogtailSysConfDir();
    mPluginCfg["HostIP"] = LogFileProfiler::mIpAddr;
    mPluginCfg["Hostname"] = LogFileProfiler::mHostname;
    mPluginCfg["EnableContainerdUpperDirDetect"] = BOOL_FLAG(enable_containerd_upper_dir_detect);
    mPluginCfg["EnableSlsMetricsFormat"] = BOOL_FLAG(enable_sls_metrics_format);
}

LogtailPlugin::~LogtailPlugin() {
    DynamicLibLoader::CloseLib(mPluginBasePtr);
    DynamicLibLoader::CloseLib(mPluginAdapterPtr);
}

bool LogtailPlugin::LoadPipeline(const std::string& pipelineName,
                                 const std::string& pipeline,
                                 const std::string& project,
                                 const std::string& logstore,
                                 const std::string& region,
                                 logtail::QueueKey logstoreKey) {
    if (!mPluginValid) {
        LoadPluginBase();
    }

    if (mPluginValid && mLoadConfigFun != NULL) {
        GoString goProject;
        GoString goLogstore;
        GoString goConfigName;
        GoString goPluginConfig;

        goConfigName.n = pipelineName.size();
        goConfigName.p = pipelineName.c_str();
        goPluginConfig.n = pipeline.size();
        goPluginConfig.p = pipeline.c_str();
        goProject.n = project.size();
        goProject.p = project.c_str();
        goLogstore.n = logstore.size();
        goLogstore.p = logstore.c_str();
        long long goLogStoreKey = static_cast<long long>(logstoreKey);

        return mLoadConfigFun(goProject, goLogstore, goConfigName, goLogStoreKey, goPluginConfig) == 0;
    }

    return false;
}

void LogtailPlugin::HoldOn(bool exitFlag) {
    if (mPluginValid && mHoldOnFun != NULL) {
        LOG_INFO(sLogger, ("Go pipelines pause", "starts"));
        auto holdOnStart = GetCurrentTimeInMilliSeconds();
        mHoldOnFun(exitFlag ? 1 : 0);
        auto holdOnCost = GetCurrentTimeInMilliSeconds() - holdOnStart;
        LOG_INFO(sLogger, ("Go pipelines pause", "succeeded")("cost", ToString(holdOnCost) + "ms"));
        if (holdOnCost >= 60 * 1000) {
            LogtailAlarm::GetInstance()->SendAlarm(HOLD_ON_TOO_SLOW_ALARM,
                                                   "Pausing Go pipelines took " + ToString(holdOnCost) + "ms");
        }
    }
}

void LogtailPlugin::Resume() {
    if (mPluginValid && mResumeFun != NULL) {
        LOG_INFO(sLogger, ("Go pipelines resume", "starts"));
        mResumeFun();
        LOG_INFO(sLogger, ("Go pipelines resume", "succeeded"));
    }
}

int LogtailPlugin::IsValidToSend(long long logstoreKey) {
    // TODO: because go profile pipeline is not controlled by C++, we cannot know queue key in advance
    // therefore, we assume true here. This could be a potential problem if network is not available for profile info.
    // However, since go profile pipeline will be stopped only during process exit, it should be fine.
    if (logstoreKey == -1) {
        return true;
    }
    return SenderQueueManager::GetInstance()->IsValidToPush(logstoreKey) ? 0 : -1;
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
    static FlusherSLS* alarmConfig = &(LogtailPlugin::GetInstance()->mPluginAlarmConfig);
    static FlusherSLS* profileConfig = &(LogtailPlugin::GetInstance()->mPluginProfileConfig);
    static FlusherSLS* containerConfig = &(LogtailPlugin::GetInstance()->mPluginContainerConfig);

    string configNameStr = string(configName, configNameSize);

    string logstore;
    if (logstoreSize > 0 && logstoreName != NULL) {
        logstore.assign(logstoreName, (size_t)logstoreSize);
    }

    // LOG_DEBUG(sLogger, ("send pb", configNameStr)("pb size", pbSize)("lines", lines));
    FlusherSLS* pConfig = NULL;
    if (configNameStr == alarmConfig->mLogstore) {
        pConfig = alarmConfig;
        pConfig->mProject = ProfileSender::GetInstance()->GetDefaultProfileProjectName();
        pConfig->mRegion = ProfileSender::GetInstance()->GetDefaultProfileRegion();
        if (pConfig->mProject.empty()) {
            return 0;
        }
    } else if (configNameStr == profileConfig->mLogstore) {
        pConfig = profileConfig;
        pConfig->mProject = ProfileSender::GetInstance()->GetDefaultProfileProjectName();
        pConfig->mRegion = ProfileSender::GetInstance()->GetDefaultProfileRegion();
        if (pConfig->mProject.empty()) {
            return 0;
        }
    } else if (configNameStr == containerConfig->mLogstore) {
        pConfig = containerConfig;
        pConfig->mProject = ProfileSender::GetInstance()->GetDefaultProfileProjectName();
        pConfig->mRegion = ProfileSender::GetInstance()->GetDefaultProfileRegion();
        if (pConfig->mProject.empty()) {
            return 0;
        }
    } else {
        shared_ptr<Pipeline> p = PipelineManager::GetInstance()->FindConfigByName(configNameStr);
        if (!p) {
            LOG_INFO(sLogger,
                     ("error", "SendPbV2 can not find config, maybe config updated")("config", configNameStr)(
                         "logstore", logstore));
            return -2;
        }
        // TODO: support multi-flusher
        pConfig = const_cast<FlusherSLS*>(static_cast<const FlusherSLS*>(p->GetFlushers()[0]->GetPlugin()));
    }
    std::string shardHashStr;
    if (shardHashSize > 0) {
        shardHashStr.assign(shardHash, static_cast<size_t>(shardHashSize));
    }
    return pConfig->Send(std::string(pbBuffer, pbSize), shardHashStr, logstore) ? 0 : -1;
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
    // cmd 解析json
    Json::Value jsonParams;
    std::string errorMsg;
    if (paramsStr.size() < 5UL || !ParseJsonTable(paramsStr, jsonParams, errorMsg)) {
        LOG_ERROR(sLogger, ("invalid docker container params", paramsStr)("errorMsg", errorMsg));
        return -2;
    }

    switch (cmdType) {
        case PLUGIN_DOCKER_UPDATE_FILE: {
            ConfigContainerInfoUpdateCmd* cmd = new ConfigContainerInfoUpdateCmd(configNameStr, false, jsonParams);
            ConfigManager::GetInstance()->UpdateContainerPath(cmd);
        } break;
        case PLUGIN_DOCKER_STOP_FILE: {
            ConfigContainerInfoUpdateCmd* cmd = new ConfigContainerInfoUpdateCmd(configNameStr, true, jsonParams);
            ConfigManager::GetInstance()->UpdateContainerStopped(cmd);
        } break;
        case PLUGIN_DOCKER_REMOVE_FILE: {
            ConfigContainerInfoUpdateCmd* cmd = new ConfigContainerInfoUpdateCmd(configNameStr, true, jsonParams);
            ConfigManager::GetInstance()->UpdateContainerPath(cmd);
        } break;
        case PLUGIN_DOCKER_UPDATE_FILE_ALL: {
            ConfigContainerInfoUpdateCmd* cmd = new ConfigContainerInfoUpdateCmd(configNameStr, false, jsonParams);
            ConfigManager::GetInstance()->UpdateContainerPath(cmd);
        } break;
        default:
            LOG_ERROR(sLogger, ("unknown cmd", cmdType));
            break;
    }
    return 0;
}


bool LogtailPlugin::LoadPluginBase() {
    if (mPluginValid) {
        return true;
    }

    // load plugin adapter
    if (mPluginAdapterPtr == NULL) {
        DynamicLibLoader loader;
        std::string error;
        if (!loader.LoadDynLib("PluginAdapter", error, AppConfig::GetInstance()->GetWorkingDir())) {
            LOG_ERROR(sLogger, ("open adapter lib error, Message", error));
            return mPluginValid;
        }

        auto versionFun = (PluginAdapterVersion)loader.LoadMethod("PluginAdapterVersion", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load version function error, Message", error));
            return mPluginValid;
        }
        int version = versionFun();
        if (!(version / 100 == 2 || version / 100 == 3)) {
            LOG_ERROR(sLogger, ("invalid plugin adapter version, version", version));
            return mPluginValid;
        }
        LOG_INFO(sLogger, ("valid plugin adapter version, version", version));

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
                return mPluginValid;
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
        if (!loader.LoadDynLib("PluginBase", error, AppConfig::GetInstance()->GetWorkingDir())) {
            LOG_ERROR(sLogger, ("open plugin base dl error, Message", error));
            return mPluginValid;
        }

        // Try V2 -> V1.
        initBaseV2 = (InitPluginBaseV2Fun)loader.LoadMethod("InitPluginBaseV2", error);
        if (!error.empty()) {
            LOG_WARNING(sLogger, ("load InitPluginBaseFunV2 error", error)("try to load V1", ""));

            initBase = (InitPluginBaseFun)loader.LoadMethod("InitPluginBase", error);
            if (!error.empty()) {
                LOG_ERROR(sLogger, ("load InitPluginBase error", error));
                return mPluginValid;
            }
        }
        // 加载全局配置，目前应该没有调用点
        mLoadGlobalConfigFun = (LoadGlobalConfigFun)loader.LoadMethod("LoadGlobalConfig", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load LoadGlobalConfig error, Message", error));
            return mPluginValid;
        }
        // 加载单个配置，目前应该是Resume的时候，全量加载一次
        mLoadConfigFun = (LoadConfigFun)loader.LoadMethod("LoadConfig", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load LoadConfig error, Message", error));
            return mPluginValid;
        }
        // 更新配置，目前应该没有调用点
        mUnloadConfigFun = (UnloadConfigFun)loader.LoadMethod("UnloadConfig", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load UnloadConfig error, Message", error));
            return mPluginValid;
        }
        // 插件暂停
        mHoldOnFun = (HoldOnFun)loader.LoadMethod("HoldOn", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load HoldOn error, Message", error));
            return mPluginValid;
        }
        // 插件恢复
        mResumeFun = (ResumeFun)loader.LoadMethod("Resume", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load Resume error, Message", error));
            return mPluginValid;
        }
        // C++传递原始二进制数据到golang插件，v1和v2的区别:是否传递tag
        mProcessRawLogFun = (ProcessRawLogFun)loader.LoadMethod("ProcessRawLog", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load ProcessRawLog error, Message", error));
            return mPluginValid;
        }
        mProcessRawLogV2Fun = (ProcessRawLogV2Fun)loader.LoadMethod("ProcessRawLogV2", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load ProcessRawLogV2 error, Message", error));
            return mPluginValid;
        }
        // C++获取容器信息的
        mGetContainerMetaFun = (GetContainerMetaFun)loader.LoadMethod("GetContainerMeta", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load GetContainerMeta error, Message", error));
            return mPluginValid;
        }
        // C++传递单条数据到golang插件
        mProcessLogsFun = (ProcessLogsFun)loader.LoadMethod("ProcessLog", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load ProcessLogs error, Message", error));
            return mPluginValid;
        }
        // C++传递数据到golang插件
        mProcessLogGroupFun = (ProcessLogGroupFun)loader.LoadMethod("ProcessLogGroup", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load ProcessLogGroup error, Message", error));
            return mPluginValid;
        }
        // 获取golang插件部分统计信息
        mGetPipelineMetricsFun = (GetPipelineMetricsFun)loader.LoadMethod("GetPipelineMetrics", error);
        if (!error.empty()) {
            LOG_ERROR(sLogger, ("load GetPipelineMetrics error, Message", error));
            return mPluginValid;
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
        LOG_ERROR(sLogger, ("Go plugin system init", "failed")("res", initRst));
        mPluginValid = false;
    } else {
        LOG_INFO(sLogger, ("Go plugin system init", "succeeded"));
        mPluginValid = true;
    }
    return mPluginValid;
}


void LogtailPlugin::ProcessLog(const std::string& configName,
                               sls_logs::Log& log,
                               const std::string& packId,
                               const std::string& topic,
                               const std::string& tags) {
    if (!log.has_time() || !(mPluginValid && mProcessLogsFun != NULL)) {
        return;
    }

    std::string packIdPrefix = ToHexString(HashString(packId));
    std::string realConfigName = configName + "/2";
    GoString goConfigName;
    GoSlice goLog;
    GoString goPackId;
    GoString goTopic;
    GoSlice goTags;
    goConfigName.n = realConfigName.size();
    goConfigName.p = realConfigName.c_str();
    goPackId.n = packIdPrefix.size();
    goPackId.p = packIdPrefix.c_str();
    goTopic.n = topic.size();
    goTopic.p = topic.c_str();
    goTags.data = (void*)tags.c_str();
    goTags.len = goTags.cap = tags.length();
    std::string sLog = log.SerializeAsString();
    goLog.len = goLog.cap = sLog.length();
    goLog.data = (void*)sLog.c_str();
    GoInt rst = mProcessLogsFun(goConfigName, goLog, goPackId, goTopic, goTags);
    if (rst != (GoInt)0) {
        LOG_WARNING(sLogger, ("process log error", configName)("result", rst));
    }
}

void LogtailPlugin::ProcessLogGroup(const std::string& configName,
                                    const std::string& logGroup,
                                    const std::string& packId) {
    if (logGroup.empty() || !(mPluginValid && mProcessLogsFun != NULL)) {
        return;
    }
    std::string realConfigName = configName + "/2";
    std::string packIdPrefix = ToHexString(HashString(packId));
    GoString goConfigName;
    GoSlice goLog;
    GoString goPackId;
    goConfigName.n = realConfigName.size();
    goConfigName.p = realConfigName.c_str();
    goPackId.n = packIdPrefix.size();
    goPackId.p = packIdPrefix.c_str();
    goLog.len = goLog.cap = logGroup.length();
    goLog.data = (void*)logGroup.c_str();
    GoInt rst = mProcessLogGroupFun(goConfigName, goLog, goPackId);
    if (rst != (GoInt)0) {
        LOG_WARNING(sLogger, ("process loggroup error", configName)("result", rst));
    }
}

void LogtailPlugin::GetPipelineMetrics(std::vector<std::map<std::string, std::string>>& metircsList) {
    if (mGetPipelineMetricsFun != nullptr) {
        auto metrics = mGetPipelineMetricsFun();
        if (metrics != nullptr) {
            for (int i = 0; i < metrics->count; ++i) {
                std::map<std::string, std::string> item;
                InnerPluginMetric* innerpm = metrics->metrics[i];
                if (innerpm != nullptr) {
                    for (int j = 0; j < innerpm->count; ++j) {
                        InnerKeyValue* innerkv = innerpm->keyValues[j];
                        if (innerkv != nullptr) {
                            item.insert(std::make_pair(std::string(innerkv->key), std::string(innerkv->value)));
                            free(innerkv->key);
                            free(innerkv->value);
                            free(innerkv);
                        }
                    }
                    free(innerpm->keyValues);
                    free(innerpm);
                }
                metircsList.emplace_back(item);
            }
            free(metrics->metrics);
            free(metrics);
        }
    }
}

K8sContainerMeta LogtailPlugin::GetContainerMeta(const string& containerID) {
    if (mPluginValid && mGetContainerMetaFun != nullptr) {
        GoString id;
        id.n = containerID.size();
        id.p = containerID.c_str();
        auto innerMeta = mGetContainerMetaFun(id);
        if (innerMeta != NULL) {
            K8sContainerMeta meta;
            meta.ContainerName.assign(innerMeta->containerName);
            meta.Image.assign(innerMeta->image);
            meta.K8sNamespace.assign(innerMeta->k8sNamespace);
            meta.PodName.assign(innerMeta->podName);
            for (int i = 0; i < innerMeta->containerLabelsSize; ++i) {
                std::string key, value;
                key.assign(innerMeta->containerLabelsKey[i]);
                value.assign(innerMeta->containerLabelsVal[i]);
                meta.containerLabels.insert(std::make_pair(std::move(key), std::move(key)));
            }
            for (int i = 0; i < innerMeta->k8sLabelsSize; ++i) {
                std::string key, value;
                key.assign(innerMeta->k8sLabelsKey[i]);
                value.assign(innerMeta->k8sLabelsVal[i]);
                meta.k8sLabels.insert(std::make_pair(std::move(key), std::move(key)));
            }
            for (int i = 0; i < innerMeta->envSize; ++i) {
                std::string key, value;
                key.assign(innerMeta->envsKey[i]);
                value.assign(innerMeta->envsVal[i]);
                meta.envs.insert(std::make_pair(std::move(key), std::move(key)));
            }
            free(innerMeta->containerName);
            free(innerMeta->image);
            free(innerMeta->k8sNamespace);
            free(innerMeta->podName);
            if (innerMeta->containerLabelsSize > 0) {
                for (int i = 0; i < innerMeta->containerLabelsSize; ++i) {
                    free(innerMeta->containerLabelsKey[i]);
                    free(innerMeta->containerLabelsVal[i]);
                }
                free(innerMeta->containerLabelsKey);
                free(innerMeta->containerLabelsVal);
            }
            if (innerMeta->k8sLabelsSize > 0) {
                for (int i = 0; i < innerMeta->k8sLabelsSize; ++i) {
                    free(innerMeta->k8sLabelsKey[i]);
                    free(innerMeta->k8sLabelsVal[i]);
                }
                free(innerMeta->k8sLabelsKey);
                free(innerMeta->k8sLabelsVal);
            }
            if (innerMeta->envSize > 0) {
                for (int i = 0; i < innerMeta->envSize; ++i) {
                    free(innerMeta->envsKey[i]);
                    free(innerMeta->envsVal[i]);
                }
                free(innerMeta->envsKey);
                free(innerMeta->envsVal);
            }
            free(innerMeta);
            return meta;
        }
    }
    return K8sContainerMeta();
}
