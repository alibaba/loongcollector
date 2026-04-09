/*
 * Copyright 2025 iLogtail Authors
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

#include <memory>
#include <string>

#include "collection_pipeline/plugin/interface/HttpFlusher.h"
#include "collection_pipeline/serializer/OTLPHttpSerializer.h"

namespace logtail {

enum class OTLPHttpFormat : uint8_t { JSON = 0, Protobuf = 1 };

class FlusherOTLPHttpNative : public HttpFlusher {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool Send(PipelineEventGroup&& g) override;
    bool Flush(size_t key) override;
    bool FlushAll() override;
    bool BuildRequest(SenderQueueItem* item,
                      std::unique_ptr<HttpSinkRequest>& req,
                      bool* keepItem,
                      std::string* errMsg) override;
    void OnSendDone(const HttpResponse& response, SenderQueueItem* item) override;

    const std::string& GetUrl() const { return mUrl; }
    OTLPHttpFormat GetFormat() const { return mFormat; }

private:
    std::string mUrl;
    OTLPHttpFormat mFormat = OTLPHttpFormat::Protobuf;
    bool mEnableTLS = false;
    std::unordered_map<std::string, std::string> mExtraHeaders;

    CounterPtr mSendCnt;
    CounterPtr mSendSuccessCnt;
    CounterPtr mSendFailCnt;

    std::unique_ptr<OTLPEventGroupSerializer> mSerializer;

    bool SerializeAndPush(PipelineEventGroup&& group);
    bool SerializeAndPushProtobuf(PipelineEventGroup&& group);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherOTLPHttpNativeUnittest;
#endif
};

} // namespace logtail
