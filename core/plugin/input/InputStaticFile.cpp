// Copyright 2025 iLogtail Authors
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

#include "plugin/input/InputStaticFile.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/ParamExtractor.h"
#include "file_server/FileServer.h"
#include "file_server/StaticFileServer.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"

using namespace std;

namespace logtail {

const string InputStaticFile::sName = "input_static_file_onetime";

bool InputStaticFile::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;

    // SetConfigPriority must be called before GlobalConfig::Init() to avoid overriding the priority set by the user
    mContext->SetConfigPriority(2);

    if (!mFileDiscovery.Init(config, *mContext, sName)) {
        return false;
    }

    // EnableContainerDiscovery
    if (!GetOptionalBoolParam(config, "EnableContainerDiscovery", mEnableContainerDiscovery, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              false,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (mEnableContainerDiscovery && !AppConfig::GetInstance()->IsPurageContainerMode()) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "iLogtail is not in container, but container discovery is required",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (mEnableContainerDiscovery) {
        if (!mContainerDiscovery.Init(config, *mContext, sName)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               "container discovery config is invalid",
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        mContainerDiscovery.mIsStdio = false;
        mFileDiscovery.SetEnableContainerDiscoveryFlag(true);
        mFileDiscovery.SetDeduceAndSetContainerBaseDirFunc(DeduceAndSetContainerBaseDir);
        mContainerDiscovery.GenerateContainerMetaFetchingGoPipeline(
            optionalGoPipeline, &mFileDiscovery, mContext->GetPipeline().GenNextPluginMeta(false));
        mFileDiscovery.SetContainerDiscoveryOptions(std::move(mContainerDiscovery));
    }

    if (!mFileReader.Init(config, *mContext, sName)) {
        return false;
    }
    // explicitly set here to skip realtime file checkpoint loading
    mFileReader.mTailingAllMatchedFiles = true;
    mFileReader.mInputType = FileReaderOptions::InputType::InputFile;

    // Multiline
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
            // should not happen
            return false;
        }
    }

    if (!mFileTag.Init(config, *mContext, sName, mEnableContainerDiscovery)) {
        // should not happen
        return false;
    }

    // Initialize metrics
    mMonitorFileTotal = GetMetricsRecordRef().CreateIntGauge(METRIC_PLUGIN_MONITOR_FILE_TOTAL);
    static const std::unordered_map<std::string, MetricType> inputStaticFileMetricKeys = {
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_SIZE_BYTES, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_SOURCE_SIZE_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_SOURCE_READ_OFFSET_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
    };
    mPluginMetricManager = std::make_shared<PluginMetricManager>(
        GetMetricsRecordRef()->GetLabels(), inputStaticFileMetricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    mPluginMetricManager->RegisterSizeGauge(mMonitorFileTotal);

    return CreateInnerProcessors();
}

bool InputStaticFile::Start() {
    if (mEnableContainerDiscovery) {
        mFileDiscovery.SetContainerInfo(std::make_shared<vector<ContainerInfo>>());
        mFileDiscovery.SetFullContainerList(std::make_shared<std::set<std::string>>());
    }

    // Add plugin metric manager
    FileServer::GetInstance()->AddPluginMetricManager(mContext->GetConfigName(), mPluginMetricManager);

    StaticFileServer::GetInstance()->AddInput(mContext->GetConfigName(),
                                              mIndex,
                                              &mFileDiscovery,
                                              &mFileReader,
                                              &mMultiline,
                                              &mFileTag,
                                              mFileContainerMetaMap,
                                              mContext);
    return true;
}

bool InputStaticFile::Stop(bool isPipelineRemoving) {
    bool keepingCheckpoint = !isPipelineRemoving && mContext->IsOnetimePipelineRunningBeforeStart();
    StaticFileServer::GetInstance()->RemoveInput(mContext->GetConfigName(), mIndex, keepingCheckpoint);

    // Remove plugin metric manager
    FileServer::GetInstance()->RemovePluginMetricManager(mContext->GetConfigName());

    return true;
}


bool InputStaticFile::CreateInnerProcessors() {
    unique_ptr<ProcessorInstance> processor;
    {
        Json::Value detail;
        if (mContext->IsFirstProcessorJson() || mMultiline.mMode == MultilineOptions::Mode::JSON) {
            mContext->SetRequiringJsonReaderFlag(true);
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
            detail["SplitChar"] = Json::Value('\0');
        } else if (mMultiline.IsMultiline()) {
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitMultilineLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
            detail["Mode"] = Json::Value("custom");
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
        } else {
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
        }
        detail["EnableRawContent"]
            = Json::Value(!mContext->HasNativeProcessors() && !mContext->IsExactlyOnceEnabled()
                          && !mContext->IsFlushingThroughGoPipeline() && !mFileTag.EnableLogPositionMeta());
        if (!processor->Init(detail, *mContext)) {
            // should not happen
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
    }
    return true;
}

bool InputStaticFile::DeduceAndSetContainerBaseDir(ContainerInfo& containerInfo,
                                                   const CollectionPipelineContext*,
                                                   const FileDiscoveryOptions* fileDiscovery) {
    return SetContainerBaseDirs(containerInfo, fileDiscovery);
}

} // namespace logtail
