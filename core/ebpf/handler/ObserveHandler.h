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

#pragma once

#include <vector>

#include "ebpf/handler/AbstractHandler.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

class MeterHandler : public AbstractHandler {
public:
    MeterHandler(const logtail::PipelineContext* ctx, QueueKey key, uint32_t idx) : AbstractHandler(ctx, key, idx) {}

    virtual void handle(std::vector<std::unique_ptr<ApplicationBatchMeasure>>&, uint64_t) = 0;
};

class OtelMeterHandler : public MeterHandler {
public:
    OtelMeterHandler(const logtail::PipelineContext* ctx, QueueKey key, uint32_t idx) : MeterHandler(ctx, key, idx) {}
    void handle(std::vector<std::unique_ptr<ApplicationBatchMeasure>>& measures, uint64_t timestamp) override;
};

class SpanHandler : public AbstractHandler {
public:
    SpanHandler(const logtail::PipelineContext* ctx, QueueKey key, uint32_t idx) : AbstractHandler(ctx, key, idx) {}
    virtual void handle(std::vector<std::unique_ptr<ApplicationBatchSpan>>&) = 0;
};

class OtelSpanHandler : public SpanHandler {
public:
    OtelSpanHandler(const logtail::PipelineContext* ctx, QueueKey key, uint32_t idx) : SpanHandler(ctx, key, idx) {}
    void handle(std::vector<std::unique_ptr<ApplicationBatchSpan>>&) override;
};

class EventHandler : public AbstractHandler {
public:
    EventHandler(const logtail::PipelineContext* ctx, QueueKey key, uint32_t idx) : AbstractHandler(ctx, key, idx) {}
    void handle(std::vector<std::unique_ptr<ApplicationBatchEvent>>&);
};

#ifdef __ENTERPRISE__

class ArmsMeterHandler : public MeterHandler {
public:
    ArmsMeterHandler(const logtail::PipelineContext* ctx, QueueKey key, uint32_t idx) : MeterHandler(ctx, key, idx) {}
    void handle(std::vector<std::unique_ptr<ApplicationBatchMeasure>>& measures, uint64_t timestamp) override;
};

class ArmsSpanHandler : public SpanHandler {
public:
    ArmsSpanHandler(const logtail::PipelineContext* ctx, QueueKey key, uint32_t idx) : SpanHandler(ctx, key, idx) {}
    void handle(std::vector<std::unique_ptr<ApplicationBatchSpan>>&) override;
};

#endif

}
}
