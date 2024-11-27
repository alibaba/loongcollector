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

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "pipeline/queue/SenderQueueManager.h"

using namespace std;

namespace logtail {

const string FlusherFile::sName = "flusher_file";

bool FlusherFile::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    static uint32_t cnt = 0;
    GenerateQueueKey(to_string(++cnt));
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
    // Pattern
    // GetMandatoryStringParam(config, "Pattern", mPattern, errorMsg);
    // MaxFileSize
    // GetMandatoryUIntParam(config, "MaxFileSize", mMaxFileSize, errorMsg);
    // MaxFiles
    // GetMandatoryUIntParam(config, "MaxFiles", mMaxFileSize, errorMsg);

    // create file writer
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(mFilePath, mMaxFileSize, mMaxFiles, true);
    mFileWriter = std::make_shared<spdlog::async_logger>(
        sName, file_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    mFileWriter->set_pattern(mPattern);

    mBatcher.Init(Json::Value(), this, DefaultFlushStrategyOptions{});
    mGroupSerializer = make_unique<JsonEventGroupSerializer>(this);
    mSendCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_OUT_EVENT_GROUPS_TOTAL);
    return true;
}

bool FlusherFile::Send(PipelineEventGroup&& g) {
    if (g.IsReplay()) {
        return SerializeAndPush(std::move(g));
    } else {
        vector<BatchedEventsList> res;
        mBatcher.Add(std::move(g), res);
        return SerializeAndPush(std::move(res));
    }
}

bool FlusherFile::Flush(size_t key) {
    BatchedEventsList res;
    mBatcher.FlushQueue(key, res);
    return SerializeAndPush(std::move(res));
}

bool FlusherFile::FlushAll() {
    vector<BatchedEventsList> res;
    mBatcher.FlushAll(res);
    return SerializeAndPush(std::move(res));
}

bool FlusherFile::SerializeAndPush(PipelineEventGroup&& group) {
    string serializedData, errorMsg;
    BatchedEvents g(std::move(group.MutableEvents()),
                    std::move(group.GetSizedTags()),
                    std::move(group.GetSourceBuffer()),
                    group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                    std::move(group.GetExactlyOnceCheckpoint()));
    mGroupSerializer->DoSerialize(move(g), serializedData, errorMsg);
    if (errorMsg.empty()) {
        mFileWriter->info(serializedData);
    } else {
        LOG_ERROR(sLogger, ("serialize pipeline event group error", errorMsg));
    }
    mFileWriter->flush();
    return true;
}

bool FlusherFile::SerializeAndPush(BatchedEventsList&& groupList) {
    string serializedData;
    for (auto& group : groupList) {
        string errorMsg;
        mGroupSerializer->DoSerialize(move(group), serializedData, errorMsg);
        if (errorMsg.empty()) {
            mFileWriter->info(serializedData);
        } else {
            LOG_ERROR(sLogger, ("serialize pipeline event group error", errorMsg));
        }
    }
    mFileWriter->flush();
    return true;
}

bool FlusherFile::SerializeAndPush(vector<BatchedEventsList>&& groupLists) {
    for (auto& groupList : groupLists) {
        SerializeAndPush(std::move(groupList));
    }
    return true;
}

} // namespace logtail