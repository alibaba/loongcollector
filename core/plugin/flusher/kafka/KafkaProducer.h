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
#include <librdkafka/rdkafka.h>

#include <functional>
#include <memory>
#include <string>

#include "plugin/flusher/kafka/KafkaConstant.h"

namespace logtail {

struct KafkaConfig;

class KafkaProducer {
public:
    struct HeadersTemplate;
    enum class ErrorType { SUCCESS, NETWORK_ERROR, AUTH_ERROR, SERVER_ERROR, PARAMS_ERROR, QUEUE_FULL, OTHER_ERROR };

    struct ErrorInfo {
        ErrorType type;
        std::string message;
        int code;
    };

    using Callback = std::function<void(bool success, const ErrorInfo& errorInfo)>;

    KafkaProducer();
    virtual ~KafkaProducer();

    virtual bool Init(const KafkaConfig& config);

    virtual HeadersTemplate* CreateHeadersTemplate(const std::vector<std::pair<std::string, std::string>>& headers);
    virtual void DestroyHeadersTemplate(HeadersTemplate* tpl);

    virtual void ProduceAsync(const std::string& topic,
                              std::string&& value,
                              Callback callback,
                              const std::string& key = std::string(),
                              HeadersTemplate* headersTemplate = nullptr);
    virtual bool Flush(int timeoutMs);
    virtual void Close();


    static ErrorType MapKafkaError(rd_kafka_resp_err_t err);


    static void DeliveryReportCallback(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* opaque);

    KafkaProducer(const KafkaProducer&) = delete;
    KafkaProducer& operator=(const KafkaProducer&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace logtail
