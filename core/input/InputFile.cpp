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

#include "input/InputFile.h"

#include <filesystem>

#include "StringTools.h"
#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "config_manager/ConfigManager.h"
#include "file_server/FileServer.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineManager.h"

using namespace std;

DEFINE_FLAG_INT32(search_checkpoint_default_dir_depth, "0 means only search current directory", 0);
DEFINE_FLAG_INT32(max_exactly_once_concurrency, "", 512);

namespace logtail {

const string InputFile::sName = "input_file";

InputFile::InputFile()
    : mMaxCheckpointDirSearchDepth(static_cast<uint32_t>(INT32_FLAG(search_checkpoint_default_dir_depth))) {
}

bool InputFile::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;

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
        mFileDiscovery.SetEnableContainerDiscoveryFlag(true);
        if (!mContainerDiscovery.Init(config, *mContext, sName)) {
            return false;
        }
        mContainerDiscovery.GenerateContainerMetaFetchingGoPipeline(optionalGoPipeline, &mFileDiscovery);
    }

    if (!mFileReader.Init(config, *mContext, sName)) {
        return false;
    }
    mFileReader.mInputType = FileReaderOptions::InputType::InputFile;

    // 过渡使用
    mFileDiscovery.SetTailingAllMatchedFiles(mFileReader.mTailingAllMatchedFiles);
    mFileDiscovery.SetDeduceAndSetContainerBaseDirFunc(DeduceAndSetContainerBaseDir);

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
            return false;
        }
    }

    // MaxCheckpointDirSearchDepth
    if (!GetOptionalUIntParam(config, "MaxCheckpointDirSearchDepth", mMaxCheckpointDirSearchDepth, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mMaxCheckpointDirSearchDepth,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // ExactlyOnceConcurrency (param is unintentionally named as EnableExactlyOnce, which should be deprecated in the
    // future)
    uint32_t exactlyOnceConcurrency = 0;
    if (!GetOptionalUIntParam(config, "EnableExactlyOnce", exactlyOnceConcurrency, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mExactlyOnceConcurrency,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (exactlyOnceConcurrency > static_cast<uint32_t>(INT32_FLAG(max_exactly_once_concurrency))) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              "uint param EnableExactlyOnce is larger than "
                                  + ToString(INT32_FLAG(max_exactly_once_concurrency)),
                              mExactlyOnceConcurrency,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else {
        mExactlyOnceConcurrency = exactlyOnceConcurrency;
    }

    return true;
}

std::string InputFile::GetLogPath(const FileDiscoveryOptions* fileDiscovery) {
    std::string logPath;
    if (!fileDiscovery->GetWildcardPaths().empty()) {
        logPath = fileDiscovery->GetWildcardPaths()[0];
    } else {
        logPath = fileDiscovery->GetBasePath();
    }
    return logPath;
}

bool InputFile::SetContainerBaseDir(ContainerInfo& containerInfo, const std::string& logPath) {
    if (!containerInfo.mRealBaseDir.empty()) {
        return true;
    }
    size_t pthSize = logPath.size();

    size_t size = containerInfo.mMounts.size();
    size_t bestMatchedMountsIndex = size;
    // ParseByJSONObj 确保 Destination、Source、mUpperDir 不会以\或者/结尾
    for (size_t i = 0; i < size; ++i) {
        StringView dst = containerInfo.mMounts[i].mDestination;
        size_t dstSize = dst.size();

        if (StartWith(logPath, dst)
            && (pthSize == dstSize || (pthSize > dstSize && (logPath[dstSize] == '/' || logPath[dstSize] == '\\')))
            && (bestMatchedMountsIndex == size
                || containerInfo.mMounts[bestMatchedMountsIndex].mDestination.size() < dstSize)) {
            bestMatchedMountsIndex = i;
        }
    }
    if (bestMatchedMountsIndex < size) {
        containerInfo.mRealBaseDir = STRING_FLAG(default_container_host_path)
            + containerInfo.mMounts[bestMatchedMountsIndex].mSource
            + logPath.substr(containerInfo.mMounts[bestMatchedMountsIndex].mDestination.size());
        LOG_DEBUG(sLogger,
                  ("set container base dir",
                   containerInfo.mRealBaseDir)("source", containerInfo.mMounts[bestMatchedMountsIndex].mSource)(
                      "destination", containerInfo.mMounts[bestMatchedMountsIndex].mDestination)("logPath", logPath));
    } else {
        containerInfo.mRealBaseDir = STRING_FLAG(default_container_host_path) + containerInfo.mUpperDir + logPath;
    }
    LOG_INFO(sLogger, ("set container base dir", containerInfo.mRealBaseDir)("container id", containerInfo.mID));
    return true;
}

bool InputFile::DeduceAndSetContainerBaseDir(ContainerInfo& containerInfo,
                                             const PipelineContext*,
                                             const FileDiscoveryOptions* fileDiscovery) {
    std::string logPath = GetLogPath(fileDiscovery);
    return SetContainerBaseDir(containerInfo, logPath);
}

bool InputFile::Start() {
    if (mEnableContainerDiscovery) {
        mFileDiscovery.SetContainerInfo(
            FileServer::GetInstance()->GetAndRemoveContainerInfo(mContext->GetPipeline().Name()));
    }
    FileServer::GetInstance()->AddFileDiscoveryConfig(mContext->GetConfigName(), &mFileDiscovery, mContext);
    FileServer::GetInstance()->AddFileReaderConfig(mContext->GetConfigName(), &mFileReader, mContext);
    FileServer::GetInstance()->AddMultilineConfig(mContext->GetConfigName(), &mMultiline, mContext);
    FileServer::GetInstance()->AddExactlyOnceConcurrency(mContext->GetConfigName(), mExactlyOnceConcurrency);
    return true;
}

bool InputFile::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving && mEnableContainerDiscovery) {
        FileServer::GetInstance()->SaveContainerInfo(mContext->GetPipeline().Name(), mFileDiscovery.GetContainerInfo());
    }
    FileServer::GetInstance()->RemoveFileDiscoveryConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveFileReaderConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveMultilineConfig(mContext->GetConfigName());
    FileServer::GetInstance()->RemoveExactlyOnceConcurrency(mContext->GetConfigName());
    return true;
}

} // namespace logtail
