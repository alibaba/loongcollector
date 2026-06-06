// Copyright 2026 iLogtail Authors
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

#include "ebpf/plugin/agentsight/AgentsightMessageUtil.h"

#include <cstring>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "common/HashUtil.h"

namespace logtail::ebpf {
namespace {

bool ParseMessagesArray(const std::string& json, rapidjson::Document& doc) {
    if (json.empty()) {
        return false;
    }
    doc.Parse(json.c_str(), json.size());
    return !doc.HasParseError() && doc.IsArray();
}

std::string SerializeJsonArrayOmitSystem(const std::string& messagesJson) {
    rapidjson::Document doc;
    if (!ParseMessagesArray(messagesJson, doc)) {
        return messagesJson;
    }
    rapidjson::Document out(rapidjson::kArrayType);
    auto& alloc = out.GetAllocator();
    for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
        const auto& msg = doc[i];
        if (msg.IsObject() && msg.HasMember("role") && msg["role"].IsString()
            && std::strcmp(msg["role"].GetString(), "system") == 0) {
            continue;
        }
        rapidjson::Value copy;
        copy.CopyFrom(msg, alloc);
        out.PushBack(copy, alloc);
    }
    if (out.Size() == doc.Size()) {
        return messagesJson;
    }
    if (out.Empty()) {
        return {};
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    out.Accept(writer);
    return std::string(buf.GetString(), buf.GetSize());
}

// H_in / messages_hash: keep only role + parts per message.
void NormalizeMessageForHash(const rapidjson::Value& src,
                             rapidjson::Value& dst,
                             rapidjson::Document::AllocatorType& alloc) {
    if (!src.IsObject()) {
        dst.CopyFrom(src, alloc);
        return;
    }
    dst.SetObject();
    if (src.HasMember("role")) {
        rapidjson::Value role;
        role.CopyFrom(src["role"], alloc);
        dst.AddMember("role", role, alloc);
    }
    if (src.HasMember("parts")) {
        rapidjson::Value parts;
        parts.CopyFrom(src["parts"], alloc);
        dst.AddMember("parts", parts, alloc);
    }
}

// H_out replay: keep only role per message (parts may differ between response and replay, e.g. tool_call id).
void NormalizeMessageForOutputHash(const rapidjson::Value& src,
                                   rapidjson::Value& dst,
                                   rapidjson::Document::AllocatorType& alloc) {
    if (!src.IsObject()) {
        dst.CopyFrom(src, alloc);
        return;
    }
    dst.SetObject();
    if (src.HasMember("role")) {
        rapidjson::Value role;
        role.CopyFrom(src["role"], alloc);
        dst.AddMember("role", role, alloc);
    }
}

using NormalizeMessageFn = void (*)(const rapidjson::Value&, rapidjson::Value&, rapidjson::Document::AllocatorType&);

std::string SerializeJsonArrayRangeForHash(const std::string& messagesJson,
                                           size_t startIndex,
                                           size_t elementCount,
                                           NormalizeMessageFn normalize) {
    rapidjson::Document doc;
    if (!ParseMessagesArray(messagesJson, doc)) {
        return {};
    }
    const size_t docSize = static_cast<size_t>(doc.Size());
    if (startIndex >= docSize || elementCount == 0) {
        return {};
    }
    const size_t take = std::min(elementCount, docSize - startIndex);
    rapidjson::Document slice(rapidjson::kArrayType);
    auto& alloc = slice.GetAllocator();
    for (size_t i = 0; i < take; ++i) {
        rapidjson::Value item;
        normalize(doc[static_cast<rapidjson::SizeType>(startIndex + i)], item, alloc);
        slice.PushBack(item, alloc);
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    slice.Accept(writer);
    return std::string(buf.GetString(), buf.GetSize());
}

std::string HashJsonArrayRangeNormalized(const std::string& fullMessagesJson,
                                         size_t startIndex,
                                         size_t elementCount,
                                         NormalizeMessageFn normalize) {
    if (elementCount == 0) {
        return {};
    }
    const std::string slice = SerializeJsonArrayRangeForHash(fullMessagesJson, startIndex, elementCount, normalize);
    if (slice.empty()) {
        return {};
    }
    return logtail::CalcSHA256Hex(slice);
}

std::string
HashJsonArrayRangeForInputHash(const std::string& fullMessagesJson, size_t startIndex, size_t elementCount) {
    return HashJsonArrayRangeNormalized(fullMessagesJson, startIndex, elementCount, NormalizeMessageForHash);
}

std::string
HashJsonArrayRangeForOutputHash(const std::string& fullMessagesJson, size_t startIndex, size_t elementCount) {
    return HashJsonArrayRangeNormalized(fullMessagesJson, startIndex, elementCount, NormalizeMessageForOutputHash);
}

} // namespace

size_t CountJsonArrayElements(const std::string& messagesJson) {
    rapidjson::Document doc;
    if (!ParseMessagesArray(messagesJson, doc)) {
        return 0;
    }
    return doc.Size();
}

std::string SerializeJsonArrayRange(const std::string& messagesJson, size_t startIndex, size_t elementCount) {
    rapidjson::Document doc;
    if (!ParseMessagesArray(messagesJson, doc)) {
        return {};
    }
    const size_t docSize = static_cast<size_t>(doc.Size());
    if (startIndex >= docSize || elementCount == 0) {
        return {};
    }
    const size_t take = std::min(elementCount, docSize - startIndex);
    rapidjson::Document slice(rapidjson::kArrayType);
    auto& alloc = slice.GetAllocator();
    for (size_t i = 0; i < take; ++i) {
        rapidjson::Value item;
        item.CopyFrom(doc[static_cast<rapidjson::SizeType>(startIndex + i)], alloc);
        slice.PushBack(item, alloc);
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    slice.Accept(writer);
    return std::string(buf.GetString(), buf.GetSize());
}

std::string SerializeJsonArrayPrefix(const std::string& messagesJson, size_t prefixCount) {
    return SerializeJsonArrayRange(messagesJson, 0, prefixCount);
}

std::string SerializeJsonArraySuffix(const std::string& messagesJson, size_t startIndex) {
    rapidjson::Document doc;
    if (!ParseMessagesArray(messagesJson, doc)) {
        return {};
    }
    const size_t docSize = static_cast<size_t>(doc.Size());
    if (startIndex >= docSize) {
        return {};
    }
    return SerializeJsonArrayRange(messagesJson, startIndex, docSize - startIndex);
}

std::string HashJsonArrayRange(const std::string& fullMessagesJson, size_t startIndex, size_t elementCount) {
    return HashJsonArrayRangeForInputHash(fullMessagesJson, startIndex, elementCount);
}

std::string HashJsonArrayPrefix(const std::string& fullMessagesJson, size_t prefixCount) {
    return HashJsonArrayRangeForInputHash(fullMessagesJson, 0, prefixCount);
}

std::string HashJsonArrayPrefixForOutput(const std::string& fullMessagesJson, size_t prefixCount) {
    return HashJsonArrayRangeForOutputHash(fullMessagesJson, 0, prefixCount);
}

std::string ExtractSystemInstructionsJson(const std::string& requestMessagesJson) {
    rapidjson::Document doc;
    if (!ParseMessagesArray(requestMessagesJson, doc)) {
        return {};
    }
    rapidjson::Document systems(rapidjson::kArrayType);
    auto& alloc = systems.GetAllocator();
    for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
        const auto& msg = doc[i];
        if (!msg.IsObject() || !msg.HasMember("role") || !msg["role"].IsString()) {
            continue;
        }
        if (std::strcmp(msg["role"].GetString(), "system") != 0) {
            continue;
        }
        rapidjson::Value copy;
        copy.CopyFrom(msg, alloc);
        systems.PushBack(copy, alloc);
    }
    if (systems.Empty()) {
        return {};
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    systems.Accept(writer);
    return std::string(buf.GetString(), buf.GetSize());
}

bool FinishReasonAlreadyInArray(const rapidjson::Value& reasons, const char* reason, rapidjson::SizeType len) {
    for (rapidjson::SizeType i = 0; i < reasons.Size(); ++i) {
        const auto& v = reasons[i];
        if (v.IsString() && v.GetStringLength() == len && std::memcmp(v.GetString(), reason, len) == 0) {
            return true;
        }
    }
    return false;
}

void AppendFinishReasonString(const char* reason,
                              rapidjson::SizeType len,
                              rapidjson::Document& reasons,
                              rapidjson::Document::AllocatorType& alloc) {
    if (reason == nullptr || len == 0) {
        return;
    }
    if (FinishReasonAlreadyInArray(reasons, reason, len)) {
        return;
    }
    reasons.PushBack(rapidjson::Value(reason, len, alloc), alloc);
}

void CollectFinishReasonFromMessageObject(const rapidjson::Value& msg,
                                          rapidjson::Document& reasons,
                                          rapidjson::Document::AllocatorType& alloc) {
    if (!msg.IsObject()) {
        return;
    }
    if (msg.HasMember("finish_reason") && msg["finish_reason"].IsString()) {
        AppendFinishReasonString(
            msg["finish_reason"].GetString(), msg["finish_reason"].GetStringLength(), reasons, alloc);
    }
    if (!msg.HasMember("parts") || !msg["parts"].IsArray()) {
        return;
    }
    for (rapidjson::SizeType i = 0; i < msg["parts"].Size(); ++i) {
        CollectFinishReasonFromMessageObject(msg["parts"][i], reasons, alloc);
    }
}

bool TryMergeFinishReasonsJsonArray(const std::string& raw,
                                    rapidjson::Document& reasons,
                                    rapidjson::Document::AllocatorType& alloc) {
    if (raw.empty() || raw.front() != '[') {
        return false;
    }
    rapidjson::Document parsed;
    parsed.Parse(raw.c_str(), raw.size());
    if (parsed.HasParseError() || !parsed.IsArray()) {
        return false;
    }
    for (rapidjson::SizeType i = 0; i < parsed.Size(); ++i) {
        const auto& item = parsed[i];
        if (!item.IsString()) {
            continue;
        }
        AppendFinishReasonString(item.GetString(), item.GetStringLength(), reasons, alloc);
    }
    return !reasons.Empty();
}

std::string SerializeFinishReasonsArray(const rapidjson::Document& reasons) {
    if (!reasons.IsArray() || reasons.Empty()) {
        return {};
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    reasons.Accept(writer);
    const std::string out(buf.GetString(), buf.GetSize());
    if (out.empty() || out.front() != '[') {
        return {};
    }
    return out;
}

std::string FormatFinishReasonsJson(const std::string& responseMessagesJson, const std::string& fallbackFinishReason) {
    rapidjson::Document reasons(rapidjson::kArrayType);
    auto& alloc = reasons.GetAllocator();

    rapidjson::Document doc;
    if (ParseMessagesArray(responseMessagesJson, doc)) {
        for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
            CollectFinishReasonFromMessageObject(doc[i], reasons, alloc);
        }
    }

    if (reasons.Empty() && !fallbackFinishReason.empty()) {
        if (!TryMergeFinishReasonsJsonArray(fallbackFinishReason, reasons, alloc)) {
            AppendFinishReasonString(fallbackFinishReason.c_str(),
                                     static_cast<rapidjson::SizeType>(fallbackFinishReason.size()),
                                     reasons,
                                     alloc);
        }
    }

    return SerializeFinishReasonsArray(reasons);
}

std::string ComputeInputMessagesHash(const std::string& fullMessagesJson) {
    const size_t count = CountJsonArrayElements(fullMessagesJson);
    if (count == 0) {
        return {};
    }
    return HashJsonArrayPrefix(fullMessagesJson, count);
}

std::string ComputeInputMessagesDelta(const std::string& fullMessagesJson,
                                      const AgentsightSessionInputState* previousState) {
    const size_t curCount = CountJsonArrayElements(fullMessagesJson);
    if (curCount == 0) {
        return {};
    }

    auto finalizeDelta = [](std::string delta) { return SerializeJsonArrayOmitSystem(delta); };

    if (previousState == nullptr || previousState->messageCount == 0 || previousState->messagesHash.empty()) {
        return finalizeDelta(SerializeJsonArrayPrefix(fullMessagesJson, curCount));
    }

    const size_t prevInCount = previousState->messageCount;
    if (curCount < prevInCount) {
        return finalizeDelta(SerializeJsonArrayPrefix(fullMessagesJson, curCount));
    }

    const std::string prefixHash = HashJsonArrayPrefix(fullMessagesJson, prevInCount);
    if (!prefixHash.empty() && prefixHash == previousState->messagesHash) {
        // cur = prev_in || replay_out || delta. Only skip N_out when the normalized replay slice
        // hash matches H_out; otherwise keep everything after N_in (no漏).
        const size_t prevOutCount = previousState->outputMessageCount;
        size_t deltaStart = prevInCount;
        if (prevOutCount > 0 && curCount >= prevInCount + prevOutCount) {
            if (!previousState->outputMessagesHash.empty()) {
                const std::string replayHash
                    = HashJsonArrayRangeForOutputHash(fullMessagesJson, prevInCount, prevOutCount);
                if (!replayHash.empty() && replayHash == previousState->outputMessagesHash) {
                    deltaStart = prevInCount + prevOutCount;
                }
            } else {
                deltaStart = prevInCount + prevOutCount;
            }
        }
        if (curCount > deltaStart) {
            return finalizeDelta(SerializeJsonArraySuffix(fullMessagesJson, deltaStart));
        }
        return {};
    }

    return finalizeDelta(SerializeJsonArrayPrefix(fullMessagesJson, curCount));
}

void UpdateSessionOutputState(const std::string& responseMessagesJson, AgentsightSessionInputState& state) {
    state.outputMessageCount = CountJsonArrayElements(responseMessagesJson);
    if (state.outputMessageCount > 0) {
        state.outputMessagesHash = HashJsonArrayPrefixForOutput(responseMessagesJson, state.outputMessageCount);
    } else {
        state.outputMessagesHash.clear();
    }
}

void CommitSessionStateAfterEmit(const std::string& requestMessagesJson,
                                 const std::string& responseMessagesJson,
                                 AgentsightSessionInputState& state) {
    UpdateSessionOutputState(responseMessagesJson, state);
    state.messageCount = CountJsonArrayElements(requestMessagesJson);
    if (state.messageCount > 0) {
        state.messagesHash = HashJsonArrayPrefix(requestMessagesJson, state.messageCount);
    } else {
        state.messagesHash.clear();
    }
}

std::string ResolveSessionStateKey(const std::string& sessionId, const std::string& turnId) {
    if (!sessionId.empty()) {
        return sessionId;
    }
    return turnId;
}

std::string FormatGenAiStepId(size_t stepNumber) {
    return "step_" + std::to_string(stepNumber);
}

} // namespace logtail::ebpf
