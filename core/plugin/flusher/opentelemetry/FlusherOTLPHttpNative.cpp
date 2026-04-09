/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "plugin/flusher/opentelemetry/FlusherOTLPHttpNative.h"

#include "collection_pipeline/queue/SenderQueueManager.h"
#include "collection_pipeline/serializer/OTLPHttpSerializer.h"
#include "common/ParamExtractor.h"
#include "common/http/HttpRequest.h"
#include "logger/Logger.h"
#include "runner/sink/http/HttpSinkRequest.h"

namespace logtail {

const std::string FlusherOTLPHttpNative::sName = "flusher_otlp_http_native";

bool FlusherOTLPHttpNative::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    std::string errorMsg;

    if (!GetMandatoryStringParam(config, "Url", mUrl, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // Optional format: "json" (default) or "protobuf"
    std::string formatStr;
    if (config.isMember("Format")) {
        formatStr = config["Format"].asString();
        if (formatStr == "protobuf") {
            mFormat = OTLPHttpFormat::Protobuf;
        } else if (formatStr == "json") {
            mFormat = OTLPHttpFormat::JSON;
        } else {
            LOG_WARNING(sLogger,
                        ("FlusherOTLPHttpNative invalid Format value", formatStr)(
                            "action", "use default json format")("plugin", sName));
        }
    }

    if (config.isMember("EnableTLS")) {
        mEnableTLS = config["EnableTLS"].asBool();
    }

    // Parse extra headers
    if (config.isMember("Headers") && config["Headers"].isObject()) {
        const auto& headers = config["Headers"];
        auto memberNames = headers.getMemberNames();
        for (const auto& name : memberNames) {
            mExtraHeaders[name] = headers[name].asString();
        }
    }

    // Create sender queue
    GenerateQueueKey(mUrl);
    SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mPluginID, mUrl, *mContext);

    // Create OTLP serializer (used for JSON path)
    mSerializer = std::make_unique<OTLPEventGroupSerializer>(this);

    // Metrics
    mSendCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);
    mSendSuccessCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL);
    mSendFailCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL);

    LOG_INFO(sLogger,
             ("FlusherOTLPHttpNative initialized", "success")("url", mUrl)("format",
                                                                            formatStr.empty() ? "json" : formatStr)(
                 "tls", mEnableTLS));
    return true;
}

bool FlusherOTLPHttpNative::Start() {
    LOG_INFO(sLogger, ("FlusherOTLPHttpNative started", "success")("url", mUrl));
    return true;
}

bool FlusherOTLPHttpNative::Stop(bool isPipelineRemoving) {
    LOG_INFO(sLogger, ("FlusherOTLPHttpNative stopped", "success"));
    return true;
}

bool FlusherOTLPHttpNative::Send(PipelineEventGroup&& g) {
    ADD_COUNTER(mSendCnt, 1);
    if (mFormat == OTLPHttpFormat::Protobuf) {
        return SerializeAndPushProtobuf(std::move(g));
    }
    return SerializeAndPush(std::move(g));
}

bool FlusherOTLPHttpNative::SerializeAndPush(PipelineEventGroup&& group) {
    if (group.GetEvents().empty()) {
        return true;
    }

    BatchedEvents batched(std::move(group.MutableEvents()),
                          std::move(group.GetSizedTags()),
                          std::move(group.GetSourceBuffer()),
                          group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                          std::move(group.GetExactlyOnceCheckpoint()));
    for (const auto& extraSourceBuffer : group.GetExtraSourceBuffers()) {
        batched.mSourceBuffers.emplace_back(extraSourceBuffer);
    }

    std::string serializedData, errorMsg;
    if (!mSerializer->DoSerialize(std::move(batched), serializedData, errorMsg)) {
        LOG_WARNING(sLogger,
                    ("failed to serialize OTLP event group",
                     errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
        return false;
    }

    auto item = std::make_unique<SenderQueueItem>(std::move(serializedData),
                                                   serializedData.size(),
                                                   this,
                                                   mQueueKey,
                                                   RawDataType::EVENT_GROUP);

    return PushToQueue(std::move(item));
}

bool FlusherOTLPHttpNative::SerializeAndPushProtobuf(PipelineEventGroup&& group) {
    if (group.GetEvents().empty()) {
        return true;
    }

    BatchedEvents batched(std::move(group.MutableEvents()),
                          std::move(group.GetSizedTags()),
                          std::move(group.GetSourceBuffer()),
                          group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                          std::move(group.GetExactlyOnceCheckpoint()));
    for (const auto& extraSourceBuffer : group.GetExtraSourceBuffers()) {
        batched.mSourceBuffers.emplace_back(extraSourceBuffer);
    }

    std::string serializedData, errorMsg;
    if (!mSerializer->SerializeToBinaryString(std::move(batched), serializedData, errorMsg)) {
        LOG_WARNING(sLogger,
                    ("failed to serialize OTLP event group to protobuf",
                     errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
        return false;
    }

    auto item = std::make_unique<SenderQueueItem>(std::move(serializedData),
                                                   serializedData.size(),
                                                   this,
                                                   mQueueKey,
                                                   RawDataType::EVENT_GROUP);

    return PushToQueue(std::move(item));
}

bool FlusherOTLPHttpNative::Flush(size_t key) {
    return true;
}

bool FlusherOTLPHttpNative::FlushAll() {
    return true;
}

bool FlusherOTLPHttpNative::BuildRequest(SenderQueueItem* item,
                                   std::unique_ptr<HttpSinkRequest>& req,
                                   bool* keepItem,
                                   std::string* errMsg) {
    *keepItem = true;

    if (item->mData.empty()) {
        *keepItem = false;
        return true;
    }

    std::string body = item->mData;

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = (mFormat == OTLPHttpFormat::Protobuf)
                                  ? "application/x-protobuf"
                                  : "application/json";
    for (const auto& [key, val] : mExtraHeaders) {
        headers[key] = val;
    }

    // Parse URL to extract host, port, and path
    std::string url = mUrl;
    std::string host;
    int32_t port = mEnableTLS ? 443 : 80;
    std::string path;

    // Remove scheme
    size_t schemeEnd = url.find("://");
    if (schemeEnd != std::string::npos) {
        url = url.substr(schemeEnd + 3);
    }

    // Extract host and path
    size_t pathStart = url.find('/');
    if (pathStart != std::string::npos) {
        host = url.substr(0, pathStart);
        path = url.substr(pathStart);
    } else {
        host = url;
        path = "/";
    }

    // Extract port if present
    size_t portSep = host.find(':');
    if (portSep != std::string::npos) {
        port = std::stoi(host.substr(portSep + 1));
        host = host.substr(0, portSep);
    }

    req = std::make_unique<HttpSinkRequest>("POST",
                                            mEnableTLS,
                                            host,
                                            port,
                                            path,
                                            "",
                                            headers,
                                            std::move(body),
                                            item);
    return true;
}

void FlusherOTLPHttpNative::OnSendDone(const HttpResponse& response, SenderQueueItem* item) {
    int32_t statusCode = response.GetStatusCode();
    if (statusCode >= 200 && statusCode < 300) {
        ADD_COUNTER(mSendSuccessCnt, 1);
        DealSenderQueueItemAfterSend(item, false);
    } else {
        ADD_COUNTER(mSendFailCnt, 1);
        LOG_WARNING(sLogger,
                    ("FlusherOTLPHttpNative response error", statusCode));
        DealSenderQueueItemAfterSend(item, true); // keep for retry
    }
}

} // namespace logtail
