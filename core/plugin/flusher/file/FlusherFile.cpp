// Copyright 2024 iLogtail Authors
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

#include "plugin/flusher/file/FlusherFile.h"

#include "collection_pipeline/queue/SenderQueueManager.h"

using namespace std;

namespace logtail {

const string FlusherFile::sName = "flusher_file";

bool FlusherFile::Init(const Json::Value& config, [[maybe_unused]] Json::Value& optionalGoPipeline) {
    static uint32_t sCnt = 0;
    GenerateQueueKey(to_string(++sCnt));
    SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mPluginID, *mContext);

    string errorMsg;
    // FilePath
    if (!GetMandatoryStringParam(config, "FilePath", mFilePath, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    // MaxFileSize
    GetMandatoryUIntParam(config, "MaxFileSize", mMaxFileSize, errorMsg);
    // MaxFiles
    GetMandatoryUIntParam(config, "MaxFiles", mMaxFiles, errorMsg);

    // create file writer
    mThreadPool = std::make_shared<spdlog::details::thread_pool>(10, 1);
    try {
        mFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(mFilePath, mMaxFileSize, mMaxFiles, true);
    } catch (const spdlog::spdlog_ex& e) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           e.what(),
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    mFileWriter
        = std::make_shared<spdlog::async_logger>(sName, mFileSink, mThreadPool, spdlog::async_overflow_policy::block);
    mFileWriter->set_pattern("%v");

    mGroupSerializer = make_unique<JsonEventGroupSerializer>(this);
    mSendCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_OUT_EVENT_GROUPS_TOTAL);
    return true;
}

bool FlusherFile::Send(PipelineEventGroup&& g) {
    return SerializeAndPush(std::move(g));
}

bool FlusherFile::Flush([[maybe_unused]] size_t key) {
    return true;
}

bool FlusherFile::FlushAll() {
    return true;
}

bool FlusherFile::SerializeAndPush(PipelineEventGroup&& group) {
    string serializedData;
    string errorMsg;
    BatchedEvents g(std::move(group.MutableEvents()),
                    std::move(group.GetSizedTags()),
                    std::move(group.GetSourceBuffer()),
                    group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                    std::move(group.GetExactlyOnceCheckpoint()));
    mGroupSerializer->DoSerialize(std::move(g), serializedData, errorMsg);
    if (errorMsg.empty()) {
        if (!serializedData.empty() && serializedData.back() == '\n') {
            serializedData.pop_back();
        }
        mFileWriter->info(serializedData);
        mFileWriter->flush();
    } else {
        LOG_ERROR(sLogger, ("serialize pipeline event group error", errorMsg));
    }
    return true;
}

} // namespace logtail
