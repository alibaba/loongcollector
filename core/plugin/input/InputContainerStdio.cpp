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

#include "plugin/input/InputContainerStdio.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/FileSystemUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "file_server/FileServer.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "plugin/processor/inner/ProcessorMergeMultilineLogNative.h"
#include "plugin/processor/inner/ProcessorParseContainerLogNative.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"

using namespace std;

namespace logtail {

const string InputContainerStdio::sName = "input_container_stdio";

bool InputContainerStdio::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;
    if (!AppConfig::GetInstance()->IsPurageContainerMode()) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "iLogtail is not in container, but container stdout collection is required.",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    static Json::Value fileDiscoveryConfig(Json::objectValue);
    if (fileDiscoveryConfig.empty()) {
        fileDiscoveryConfig["FilePaths"] = Json::Value(Json::arrayValue);
        fileDiscoveryConfig["FilePaths"].append("/**/*.log");
        fileDiscoveryConfig["AllowingCollectingFilesInRootDir"] = true;
    }

    {
        string key = "AllowingIncludedByMultiConfigs";
        const Json::Value* itr = config.find(key.c_str(), key.c_str() + key.length());
        if (itr != nullptr) {
            fileDiscoveryConfig[key] = *itr;
        }
    }
    if (!mFileDiscovery.Init(fileDiscoveryConfig, *mContext, sName)) {
        return false;
    }
    mFileDiscovery.SetEnableContainerDiscoveryFlag(true);
    mFileDiscovery.SetDeduceAndSetContainerBaseDirFunc(DeduceAndSetContainerBaseDir);

    if (!mContainerDiscovery.Init(config, *mContext, sName)) {
        return false;
    }
    mContainerDiscovery.GenerateContainerMetaFetchingGoPipeline(
        optionalGoPipeline, nullptr, mContext->GetPipeline().GenNextPluginMeta(false));

    if (!mFileReader.Init(config, *mContext, sName)) {
        return false;
    }
    mFileReader.mInputType = FileReaderOptions::InputType::InputContainerStdio;
    // Multiline
    {
        const char* key = "Multiline";
        const Json::Value* itr = config.find(key, key + strlen(key));
        if (itr) {
            if (!itr->isObject()) {
                PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                     mContext->GetAlarm(),
                                     "param Multiline is not of type object",
                                     sName,
                                     mContext->GetConfigName(),
                                     mContext->GetProjectName(),
                                     mContext->GetLogstoreName(),
                                     mContext->GetRegion());
            } else if (!mMultiline.Init(*itr, *mContext, sName)) {
                return false;
            }
        }
    }

    // Tag
    if (!mFileTag.Init(config, *mContext, sName, true)) {
        return false;
    }

    // IgnoringStdout
    if (!GetOptionalBoolParam(config, "IgnoringStdout", mIgnoringStdout, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mIgnoringStdout,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // IgnoringStderr
    if (!GetOptionalBoolParam(config, "IgnoringStderr", mIgnoringStderr, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mIgnoringStderr,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // IgnoreParseWarning
    if (!GetOptionalBoolParam(config, "IgnoreParseWarning", mIgnoreParseWarning, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mIgnoreParseWarning,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // KeepingSourceWhenParseFail
    if (!GetOptionalBoolParam(config, "KeepingSourceWhenParseFail", mKeepingSourceWhenParseFail, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mKeepingSourceWhenParseFail,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    if (!mIgnoringStdout && !mIgnoringStderr && mMultiline.IsMultiline()) {
        string warningMsg = "This logtail config has multiple lines of configuration, and when collecting stdout and "
                            "stderr logs at the same time, there may be issues with merging multiple lines";
        LOG_WARNING(sLogger, ("warning", warningMsg)("config", mContext->GetConfigName()));
        warningMsg = "warning msg: " + warningMsg + "\tconfig: " + mContext->GetConfigName();
        mContext->GetAlarm().SendAlarmWarning(CATEGORY_CONFIG_ALARM,
                                              warningMsg,
                                              GetContext().GetRegion(),
                                              GetContext().GetProjectName(),
                                              GetContext().GetConfigName(),
                                              GetContext().GetLogstoreName());
    }

    // init PluginMetricManager
    static const std::unordered_map<std::string, MetricType> inputFileMetricKeys = {
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_SIZE_BYTES, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_SOURCE_SIZE_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_SOURCE_READ_OFFSET_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
    };
    mPluginMetricManager = std::make_shared<PluginMetricManager>(
        GetMetricsRecordRef()->GetLabels(), inputFileMetricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    // Register a Gauge metric to record PluginMetricManager‘s map size
    mMonitorFileTotal = GetMetricsRecordRef().CreateIntGauge(METRIC_PLUGIN_MONITOR_FILE_TOTAL);
    mPluginMetricManager->RegisterSizeGauge(mMonitorFileTotal);

    return CreateInnerProcessors();
}

std::string InputContainerStdio::TryGetRealPath(const std::string& path) {
    std::string tmpPath = path;
#if defined(__linux__)
    if (tmpPath.empty()) {
        return "";
    }

    int index = 0; // assume path is absolute
    for (int i = 0; i < 10; i++) {
        fsutil::PathStat buf;
        if (fsutil::PathStat::stat(tmpPath, buf)) {
            return tmpPath;
        }
        while (true) {
            size_t j = tmpPath.find('/', index + 1);
            if (j == std::string::npos) {
                index = tmpPath.length();
            } else {
                index = j;
            }

            std::string subPath = tmpPath.substr(0, index);
            fsutil::PathStat buf;
            if (!fsutil::PathStat::lstat(subPath.c_str(), buf)) {
                return "";
            }
            if (buf.IsLink()) {
                // subPath is a symlink
                char target[PATH_MAX + 1]{0};
                readlink(subPath.c_str(), target, sizeof(target));
                std::string partialPath = STRING_FLAG(default_container_host_path)
                    + std::string(target); // You need to implement this function
                tmpPath = partialPath + tmpPath.substr(index);
                fsutil::PathStat buf;
                if (!fsutil::PathStat::stat(partialPath.c_str(), buf)) {
                    // path referenced by partialPath does not exist or has symlink
                    index = 0;
                } else {
                    index = partialPath.length();
                }
                break;
            }
        }
    }
    return "";
#elif defined(_MSC_VER)
    fsutil::PathStat buf;
    if (fsutil::PathStat::stat(tmpPath, buf)) {
        return tmpPath;
    }
    return "";
#endif
}

bool InputContainerStdio::DeduceAndSetContainerBaseDir(ContainerInfo& containerInfo,
                                                       const CollectionPipelineContext* ctx,
                                                       const FileDiscoveryOptions*) {
    if (!containerInfo.mRealBaseDir.empty()) {
        return true;
    }
    // ParseByJSONObj 确保 mLogPath不会以\或者/ 结尾
    std::string realPath = TryGetRealPath(STRING_FLAG(default_container_host_path) + containerInfo.mLogPath);
    if (realPath.empty()) {
        LOG_ERROR(
            sLogger,
            ("failed to set container base dir", "container log path not existed")("container id", containerInfo.mID)(
                "container log path", containerInfo.mLogPath)("input", sName)("config", ctx->GetPipeline().Name()));
        ctx->GetAlarm().SendAlarmWarning(
            INVALID_CONTAINER_PATH_ALARM,
            "failed to set container base dir: container log path not existed\tcontainer id: "
                + ToString(containerInfo.mID) + "\tcontainer log path: " + containerInfo.mLogPath
                + "\tconfig: " + ctx->GetPipeline().Name(),
            ctx->GetRegion(),
            ctx->GetProjectName(),
            ctx->GetPipeline().Name(),
            ctx->GetLogstoreName());
        return false;
    }
    size_t pos = realPath.find_last_of('/');
    if (pos != std::string::npos) {
        containerInfo.mRealBaseDir = realPath.substr(0, pos);
    }
    if (containerInfo.mRealBaseDir.length() > 1 && containerInfo.mRealBaseDir.back() == '/') {
        containerInfo.mRealBaseDir.pop_back();
    }
    LOG_INFO(sLogger,
             ("set container base dir",
              containerInfo.mRealBaseDir)("container id", containerInfo.mID)("config", ctx->GetPipeline().Name()));
    return true;
}

bool InputContainerStdio::Start() {
    FileServer::GetInstance()->AddPluginMetricManager(mContext->GetConfigName(), mPluginMetricManager);
    mFileDiscovery.SetContainerInfo(
        FileServer::GetInstance()->GetAndRemoveContainerInfo(mContext->GetPipeline().Name()));
    FileServer::GetInstance()->AddFileDiscoveryConfig(mContext->GetConfigName(), &mFileDiscovery, mContext);
    FileServer::GetInstance()->AddFileReaderConfig(mContext->GetConfigName(), &mFileReader, mContext);
    FileServer::GetInstance()->AddMultilineConfig(mContext->GetConfigName(), &mMultiline, mContext);
    FileServer::GetInstance()->AddFileTagConfig(mContext->GetConfigName(), &mFileTag, mContext);

    return true;
}

bool InputContainerStdio::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving) {
        FileServer::GetInstance()->SaveContainerInfo(mContext->GetPipeline().Name(), mFileDiscovery.GetContainerInfo());
    }
    FileServer::GetInstance()->RemoveFileDiscoveryConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveFileReaderConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveMultilineConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveFileTagConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemovePluginMetricManager(mContext->GetConfigName());
    return true;
}

bool InputContainerStdio::CreateInnerProcessors() {
    unique_ptr<ProcessorInstance> processor;
    // ProcessorSplitLogStringNative
    {
        Json::Value detail;
        processor = PluginRegistry::GetInstance()->CreateProcessor(ProcessorSplitLogStringNative::sName,
                                                                   mContext->GetPipeline().GenNextPluginMeta(false));
        detail["SplitChar"] = Json::Value('\n');
        if (!processor->Init(detail, *mContext)) {
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
    }
    // ProcessorParseContainerLogNative
    {
        Json::Value detail;
        processor = PluginRegistry::GetInstance()->CreateProcessor(ProcessorParseContainerLogNative::sName,
                                                                   mContext->GetPipeline().GenNextPluginMeta(false));
        detail["IgnoringStdout"] = Json::Value(mIgnoringStdout);
        detail["IgnoringStderr"] = Json::Value(mIgnoringStderr);
        detail["KeepingSourceWhenParseFail"] = Json::Value(mKeepingSourceWhenParseFail);
        detail["IgnoreParseWarning"] = Json::Value(mIgnoreParseWarning);
        if (!processor->Init(detail, *mContext)) {
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
    }
    // ProcessorMergeMultilineLogNative
    {
        Json::Value detail;
        processor = PluginRegistry::GetInstance()->CreateProcessor(ProcessorMergeMultilineLogNative::sName,
                                                                   mContext->GetPipeline().GenNextPluginMeta(false));
        detail["MergeType"] = Json::Value("flag");
        if (!processor->Init(detail, *mContext)) {
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
    }
    if (mMultiline.IsMultiline()) {
        Json::Value detail;
        if (mContext->IsFirstProcessorJson() || mMultiline.mMode == MultilineOptions::Mode::JSON) {
            mContext->SetRequiringJsonReaderFlag(true);
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
            detail["SplitChar"] = Json::Value('\0');
        } else {
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorMergeMultilineLogNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
            detail["Mode"] = Json::Value("custom");
            detail["MergeType"] = Json::Value("regex");
            detail["StartPattern"] = Json::Value(mMultiline.mStartPattern);
            detail["ContinuePattern"] = Json::Value(mMultiline.mContinuePattern);
            detail["EndPattern"] = Json::Value(mMultiline.mEndPattern);
            detail["IgnoringUnmatchWarning"] = Json::Value(mMultiline.mIgnoringUnmatchWarning);
            if (mMultiline.mUnmatchedContentTreatment == MultilineOptions::UnmatchedContentTreatment::DISCARD) {
                detail["UnmatchedContentTreatment"] = Json::Value("discard");
            } else if (mMultiline.mUnmatchedContentTreatment
                       == MultilineOptions::UnmatchedContentTreatment::SINGLE_LINE) {
                detail["UnmatchedContentTreatment"] = Json::Value("single_line");
            }
        }
        if (!processor->Init(detail, *mContext)) {
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
    }
    return true;
}

} // namespace logtail
