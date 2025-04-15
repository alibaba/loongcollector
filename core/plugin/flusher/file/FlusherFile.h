/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <vector>

#include "spdlog/spdlog.h"

#include "collection_pipeline/batch/Batcher.h"
#include "collection_pipeline/plugin/interface/Flusher.h"
#include "collection_pipeline/serializer/JsonSerializer.h"

namespace logtail {

class FlusherFile : public Flusher {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Send(PipelineEventGroup&& g) override;
    bool Flush(size_t key) override;
    bool FlushAll() override;

private:
    bool SerializeAndPush(PipelineEventGroup&& group);

    std::shared_ptr<spdlog::logger> mFileWriter;
    std::string mFilePath;
    uint32_t mMaxFileSize = 1024 * 1024 * 10;
    uint32_t mMaxFiles = 10;
    std::unique_ptr<EventGroupSerializer> mGroupSerializer;

    CounterPtr mSendCnt;
};

} // namespace logtail
