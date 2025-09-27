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

#include <cassert>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "plugin/flusher/kafka/KafkaConfig.h"
#include "plugin/flusher/kafka/KafkaProducer.h"

namespace logtail {

struct ProduceRequest {
    std::string Topic;
    std::string Key;
    std::string Value;
    KafkaProducer::Callback Callback;
    std::vector<std::pair<std::string, std::string>> Headers;
};

class MockKafkaProducer : public KafkaProducer {
public:
    MockKafkaProducer() = default;
    ~MockKafkaProducer() override = default;

    bool Init(const KafkaConfig& config) override {
        mInitialized = true;
        mConfig = config;
        return mInitSuccess;
    }

    HeadersTemplate* CreateHeadersTemplate(const std::vector<std::pair<std::string, std::string>>& headers) override {
        if (headers.empty())
            return nullptr;
        auto* key = reinterpret_cast<HeadersTemplate*>(new int(1));
        mHeadersMap[key] = headers;
        return key;
    }
    void DestroyHeadersTemplate(HeadersTemplate* tpl) override {
        if (!tpl)
            return;
        mHeadersMap.erase(tpl);
        delete reinterpret_cast<int*>(tpl);
    }

    void ProduceAsync(const std::string& topic,
                      std::string&& value,
                      Callback callback,
                      const std::string& key = std::string(),
                      HeadersTemplate* headersTemplate = nullptr) override {
        ProduceRequest request{topic, key, std::move(value), std::move(callback), {}};
        auto it = mHeadersMap.find(headersTemplate);
        if (it != mHeadersMap.end())
            request.Headers = it->second;
        mRequests.emplace_back(std::move(request));

        if (mAutoComplete) {
            CompleteLastRequest();
        }
    }

    bool Flush(int timeoutMs) override {
        mFlushCalled = true;

        if (!mInitialized) {
            return false;
        }
        return mFlushSuccess;
    }

    void Close() override { mClosed = true; }

    void SetInitSuccess(bool success) { mInitSuccess = success; }
    void SetFlushSuccess(bool success) { mFlushSuccess = success; }
    void SetAutoComplete(bool autoComplete) { mAutoComplete = autoComplete; }
    void CompleteLastRequest(bool success = true, const std::string& errorMsg = "") {
        KafkaProducer::ErrorInfo errorInfo;
        errorInfo.type = success ? KafkaProducer::ErrorType::SUCCESS : KafkaProducer::ErrorType::OTHER_ERROR;
        errorInfo.message = errorMsg;
        errorInfo.code = success ? 0 : -1;
        CompleteLastRequest(success, errorInfo);
    }

    void CompleteLastRequest(bool success, const KafkaProducer::ErrorInfo& errorInfo) {
        if (!mRequests.empty()) {
            auto& last = mRequests.back();
            last.Callback(success, errorInfo);
            mCompletedRequests.emplace_back(std::move(last));
            mRequests.pop_back();
        }
    }

    void CompleteAllRequests(bool success = true, const std::string& errorMsg = "") {
        KafkaProducer::ErrorInfo errorInfo;
        errorInfo.type = success ? KafkaProducer::ErrorType::SUCCESS : KafkaProducer::ErrorType::OTHER_ERROR;
        errorInfo.message = errorMsg;
        errorInfo.code = success ? 0 : -1;
        CompleteAllRequests(success, errorInfo);
    }

    void CompleteAllRequests(bool success, const KafkaProducer::ErrorInfo& errorInfo) {
        for (auto& request : mRequests) {
            request.Callback(success, errorInfo);
            mCompletedRequests.emplace_back(std::move(request));
        }
        mRequests.clear();
    }

    bool IsInitialized() const { return mInitialized; }
    bool IsClosed() const { return mClosed; }
    bool IsFlushCalled() const { return mFlushCalled; }
    const KafkaConfig& GetConfig() const { return mConfig; }
    const std::vector<ProduceRequest>& GetRequests() const { return mRequests; }
    const std::vector<ProduceRequest>& GetCompletedRequests() const { return mCompletedRequests; }
    size_t GetRequestCount() const { return mRequests.size() + mCompletedRequests.size(); }

private:
    std::unordered_map<HeadersTemplate*, std::vector<std::pair<std::string, std::string>>> mHeadersMap;
    bool mInitialized = false;
    bool mClosed = false;
    bool mFlushCalled = false;
    bool mInitSuccess = true;
    bool mFlushSuccess = true;
    bool mAutoComplete = true;

    KafkaConfig mConfig;
    std::vector<ProduceRequest> mRequests;
    std::vector<ProduceRequest> mCompletedRequests;
};

} // namespace logtail
