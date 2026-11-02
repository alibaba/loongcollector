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

#include <limits>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "common/HashUtil.h"

namespace logtail::ebpf {
namespace {

bool ParseMessagesArray(StringView json, rapidjson::Document& doc) {
    if (json.empty()) {
        return false;
    }
    // Two-argument Parse reads exactly size() bytes (MemoryStream); never use the single-argument
    // form here — the view may carry embedded NULs (binary payloads) or lack NUL termination.
    doc.Parse(json.data(), json.size());
    return !doc.HasParseError() && doc.IsArray();
}

bool IsSystemRoleMessage(const rapidjson::Value& msg) {
    return msg.IsObject() && msg.HasMember("role") && msg["role"].IsString()
        && std::strcmp(msg["role"].GetString(), "system") == 0;
}

// H_in (internal delta state): keep only role + parts per message.
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

constexpr size_t kHashAllNonSystemMessages = std::numeric_limits<size_t>::max();

struct NonSystemInputScan {
    size_t nonSystemCount = 0;
    size_t idxAfterHashPrefix = 0;
    std::string hashPrefix;
};

std::string HashNormalizedMessageSlice(const rapidjson::Document& slice) {
    if (!slice.IsArray() || slice.Empty()) {
        return {};
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    slice.Accept(writer);
    return logtail::CalcSHA256Hex(buf.GetString(), buf.GetSize());
}

// Finite hashPrefixCount: hash the first N non-system messages, set idxAfterHashPrefix, then stop
// (delta only needs nonSystemCount >= N). kHashAllNonSystemMessages: hash and count every non-system.
NonSystemInputScan ScanNonSystemInput(const rapidjson::Document& doc, size_t hashPrefixCount) {
    NonSystemInputScan result;
    const bool hashAll = hashPrefixCount == kHashAllNonSystemMessages;
    if (!doc.IsArray() || doc.Empty()) {
        return result;
    }

    rapidjson::Document hashSlice(rapidjson::kArrayType);
    auto& hashAlloc = hashSlice.GetAllocator();
    size_t hashTaken = 0;
    for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
        if (IsSystemRoleMessage(doc[i])) {
            continue;
        }
        ++result.nonSystemCount;
        if (hashAll || hashTaken < hashPrefixCount) {
            rapidjson::Value item;
            NormalizeMessageForHash(doc[i], item, hashAlloc);
            hashSlice.PushBack(item, hashAlloc);
            ++hashTaken;
            if (!hashAll && hashPrefixCount > 0 && hashTaken == hashPrefixCount) {
                result.idxAfterHashPrefix = static_cast<size_t>(i) + 1;
                break;
            }
        }
    }
    if (hashTaken > 0) {
        result.hashPrefix = HashNormalizedMessageSlice(hashSlice);
    }
    if (!hashAll && hashPrefixCount > 0 && hashTaken < hashPrefixCount) {
        result.idxAfterHashPrefix = static_cast<size_t>(doc.Size());
    }
    return result;
}

std::string SerializeJsonArrayFromDoc(const rapidjson::Document& doc, size_t startIndex, size_t elementCount) {
    if (!doc.IsArray()) {
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

std::string SerializeJsonArrayOmitSystemFromDoc(const rapidjson::Document& doc, size_t startIndex) {
    if (!doc.IsArray()) {
        return {};
    }
    const size_t docSize = static_cast<size_t>(doc.Size());
    if (startIndex >= docSize) {
        return {};
    }
    rapidjson::Document out(rapidjson::kArrayType);
    auto& alloc = out.GetAllocator();
    bool omittedSystem = false;
    for (rapidjson::SizeType i = static_cast<rapidjson::SizeType>(startIndex); i < doc.Size(); ++i) {
        const auto& msg = doc[i];
        if (IsSystemRoleMessage(msg)) {
            omittedSystem = true;
            continue;
        }
        rapidjson::Value copy;
        copy.CopyFrom(msg, alloc);
        out.PushBack(copy, alloc);
    }
    if (out.Empty()) {
        return {};
    }
    if (!omittedSystem && startIndex == 0) {
        return SerializeJsonArrayFromDoc(doc, 0, docSize);
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    out.Accept(writer);
    return std::string(buf.GetString(), buf.GetSize());
}

std::string
HashJsonArrayRangeForOutputHashFromDoc(const rapidjson::Document& doc, size_t startIndex, size_t elementCount) {
    if (!doc.IsArray() || elementCount == 0) {
        return {};
    }
    const size_t docSize = static_cast<size_t>(doc.Size());
    if (startIndex >= docSize) {
        return {};
    }
    const size_t take = std::min(elementCount, docSize - startIndex);
    rapidjson::Document slice(rapidjson::kArrayType);
    auto& alloc = slice.GetAllocator();
    for (size_t i = 0; i < take; ++i) {
        rapidjson::Value item;
        NormalizeMessageForOutputHash(doc[static_cast<rapidjson::SizeType>(startIndex + i)], item, alloc);
        slice.PushBack(item, alloc);
    }
    return HashNormalizedMessageSlice(slice);
}

void UpdateSessionOutputState(StringView responseMessagesJson, AgentsightSessionInputState& state) {
    rapidjson::Document doc;
    if (!ParseMessagesArray(responseMessagesJson, doc)) {
        state.outputMessageCount = 0;
        state.outputMessagesHash.clear();
        return;
    }
    state.outputMessageCount = static_cast<size_t>(doc.Size());
    if (state.outputMessageCount > 0) {
        state.outputMessagesHash = HashJsonArrayRangeForOutputHashFromDoc(doc, 0, state.outputMessageCount);
    } else {
        state.outputMessagesHash.clear();
    }
}

} // namespace

std::string ComputeContentSha256Hex(StringView content) {
    if (content.empty()) {
        return {};
    }
    return logtail::CalcSHA256Hex(content.data(), content.size());
}

std::string ComputeSystemInstructionsHash(StringView requestMessagesJson) {
    return ComputeContentSha256Hex(ExtractSystemInstructionsJson(requestMessagesJson));
}

std::string ComputeToolDefinitionsHash(StringView toolDefinitionsJson) {
    return ComputeContentSha256Hex(toolDefinitionsJson);
}

std::string ExtractSystemInstructionsJson(StringView requestMessagesJson) {
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

bool TryMergeFinishReasonsJsonArray(StringView raw,
                                    rapidjson::Document& reasons,
                                    rapidjson::Document::AllocatorType& alloc) {
    if (raw.empty() || raw.front() != '[') {
        return false;
    }
    rapidjson::Document parsed;
    parsed.Parse(raw.data(), raw.size());
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

std::string FormatFinishReasonsJson(StringView responseMessagesJson, StringView fallbackFinishReason) {
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
            // rapidjson::Value(reason, len, alloc) copies exactly len bytes; no NUL required.
            AppendFinishReasonString(fallbackFinishReason.data(),
                                     static_cast<rapidjson::SizeType>(fallbackFinishReason.size()),
                                     reasons,
                                     alloc);
        }
    }

    return SerializeFinishReasonsArray(reasons);
}

std::string ComputeInputMessagesDelta(StringView fullMessagesJson, const AgentsightSessionInputState* previousState) {
    rapidjson::Document curDoc;
    if (!ParseMessagesArray(fullMessagesJson, curDoc)) {
        return {};
    }
    const size_t curCount = static_cast<size_t>(curDoc.Size());
    if (curCount == 0) {
        return {};
    }

    auto deltaFromStart
        = [&](size_t startIndex) -> std::string { return SerializeJsonArrayOmitSystemFromDoc(curDoc, startIndex); };

    if (previousState == nullptr || previousState->messageCount == 0 || previousState->messagesHash.empty()) {
        return deltaFromStart(0);
    }

    const size_t prevInCount = previousState->messageCount;
    const NonSystemInputScan scan = ScanNonSystemInput(curDoc, prevInCount);
    if (scan.nonSystemCount < prevInCount) {
        return deltaFromStart(0);
    }

    if (!scan.hashPrefix.empty() && scan.hashPrefix == previousState->messagesHash) {
        // cur = prev_in || replay_out || delta. Only skip N_out when the normalized replay slice
        // hash matches H_out; otherwise keep everything after N_in (no漏).
        const size_t idxAfterIn = scan.idxAfterHashPrefix;
        const size_t prevOutCount = previousState->outputMessageCount;
        size_t deltaStart = idxAfterIn;
        if (prevOutCount > 0 && curCount >= idxAfterIn + prevOutCount) {
            if (!previousState->outputMessagesHash.empty()) {
                const std::string replayHash = HashJsonArrayRangeForOutputHashFromDoc(curDoc, idxAfterIn, prevOutCount);
                if (!replayHash.empty() && replayHash == previousState->outputMessagesHash) {
                    deltaStart = idxAfterIn + prevOutCount;
                }
            } else {
                deltaStart = idxAfterIn + prevOutCount;
            }
        }
        if (curCount > deltaStart) {
            return deltaFromStart(deltaStart);
        }
        return {};
    }

    return deltaFromStart(0);
}

void CommitSessionStateAfterEmit(StringView requestMessagesJson,
                                 StringView responseMessagesJson,
                                 StringView toolDefinitionsJson,
                                 AgentsightSessionInputState& state) {
    UpdateSessionOutputState(responseMessagesJson, state);
    rapidjson::Document requestDoc;
    if (ParseMessagesArray(requestMessagesJson, requestDoc)) {
        const NonSystemInputScan scan = ScanNonSystemInput(requestDoc, kHashAllNonSystemMessages);
        state.messageCount = scan.nonSystemCount;
        state.messagesHash = scan.hashPrefix;
    } else {
        state.messageCount = 0;
        state.messagesHash.clear();
    }
    state.systemInstructionsHash = ComputeSystemInstructionsHash(requestMessagesJson);
    state.toolDefinitionsHash = ComputeToolDefinitionsHash(toolDefinitionsJson);
}

std::string ResolveSessionStateKey(StringView sessionId, StringView turnId) {
    // The LRU key must own its bytes: the views point into the record's arena, which dies with
    // the event group, long before the cached entry expires.
    if (!sessionId.empty()) {
        return sessionId.to_string();
    }
    return turnId.to_string();
}

std::string FormatGenAiStepId(StringView turnId, size_t stepNumber) {
    if (turnId.empty()) {
        return {};
    }
    return turnId.to_string() + ":s" + std::to_string(stepNumber);
}

} // namespace logtail::ebpf
