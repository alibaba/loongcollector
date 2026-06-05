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

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace logtail::ebpf {
namespace {

constexpr uint32_t kSha256Init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

constexpr uint32_t kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

uint32_t Rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32U - n));
}

void Sha256Block(const uint8_t block[64], uint32_t state[8]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
               | (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + S1 + ch + kSha256K[i] + w[i];
        const uint32_t S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void Sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint32_t state[8];
    std::memcpy(state, kSha256Init, sizeof(state));

    size_t fullBlocks = len / 64;
    for (size_t i = 0; i < fullBlocks; ++i) {
        Sha256Block(data + i * 64, state);
    }

    uint8_t block[64] = {};
    const size_t rem = len % 64;
    if (rem > 0) {
        std::memcpy(block, data + fullBlocks * 64, rem);
    }
    block[rem] = 0x80;
    if (rem >= 56) {
        Sha256Block(block, state);
        std::memset(block, 0, sizeof(block));
    }
    const uint64_t bitLen = static_cast<uint64_t>(len) * 8ULL;
    for (int i = 0; i < 8; ++i) {
        block[63 - i] = static_cast<uint8_t>(bitLen >> (8 * i));
    }
    Sha256Block(block, state);

    for (int i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<uint8_t>(state[i] >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(state[i] >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(state[i]);
    }
}

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

// Session hashes (H_in, H_out, messages_hash): keep only role + parts per message.
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

std::string SerializeJsonArrayRangeForHash(const std::string& messagesJson,
                                           size_t startIndex,
                                           size_t elementCount) {
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
        NormalizeMessageForHash(doc[static_cast<rapidjson::SizeType>(startIndex + i)], item, alloc);
        slice.PushBack(item, alloc);
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    slice.Accept(writer);
    return std::string(buf.GetString(), buf.GetSize());
}

std::optional<std::string> JsonValueToString(const rapidjson::Value& v) {
    if (v.IsString()) {
        return std::string(v.GetString(), v.GetStringLength());
    }
    if (v.IsInt64()) {
        return std::to_string(v.GetInt64());
    }
    if (v.IsUint64()) {
        return std::to_string(v.GetUint64());
    }
    if (v.IsInt()) {
        return std::to_string(v.GetInt());
    }
    if (v.IsUint()) {
        return std::to_string(v.GetUint());
    }
    if (v.IsDouble()) {
        std::ostringstream oss;
        oss << v.GetDouble();
        return oss.str();
    }
    if (v.IsBool()) {
        return v.GetBool() ? "true" : "false";
    }
    return std::nullopt;
}

} // namespace

std::string Sha256Hex(const std::string& data) {
    uint8_t digest[32];
    Sha256(reinterpret_cast<const uint8_t*>(data.data()), data.size(), digest);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const auto b : digest) {
        oss << std::setw(2) << static_cast<unsigned>(b);
    }
    return oss.str();
}

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

std::string HashJsonArrayRangeForHash(const std::string& fullMessagesJson,
                                      size_t startIndex,
                                      size_t elementCount) {
    if (elementCount == 0) {
        return {};
    }
    const std::string slice = SerializeJsonArrayRangeForHash(fullMessagesJson, startIndex, elementCount);
    if (slice.empty()) {
        return {};
    }
    return Sha256Hex(slice);
}

std::string HashJsonArrayRange(const std::string& fullMessagesJson, size_t startIndex, size_t elementCount) {
    return HashJsonArrayRangeForHash(fullMessagesJson, startIndex, elementCount);
}

std::string HashJsonArrayPrefix(const std::string& fullMessagesJson, size_t prefixCount) {
    return HashJsonArrayRangeForHash(fullMessagesJson, 0, prefixCount);
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
        AppendFinishReasonString(msg["finish_reason"].GetString(),
                                 msg["finish_reason"].GetStringLength(),
                                 reasons,
                                 alloc);
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

std::string FormatFinishReasonsJson(const std::string& responseMessagesJson,
                                    const std::string& fallbackFinishReason) {
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

AgentsightParsedRequestParams ParseRequestParametersJson(const std::string& requestParamsJson) {
    AgentsightParsedRequestParams out;
    if (requestParamsJson.empty()) {
        return out;
    }
    rapidjson::Document doc;
    doc.Parse(requestParamsJson.c_str(), requestParamsJson.size());
    if (doc.HasParseError() || !doc.IsObject()) {
        return out;
    }
    auto take = [&](const char* key, std::optional<std::string>& dest) {
        if (!doc.HasMember(key)) {
            return;
        }
        if (auto s = JsonValueToString(doc[key])) {
            dest = std::move(*s);
        }
    };
    take("temperature", out.temperature);
    take("max_tokens", out.maxTokens);
    take("frequency_penalty", out.frequencyPenalty);
    take("presence_penalty", out.presencePenalty);
    take("top_p", out.topP);
    take("top_k", out.topK);
    take("seed", out.seed);
    take("n", out.choiceCount);
    return out;
}

AgentsightInputUploadPlan PlanInputMessagesUpload(const std::string& fullMessagesJson,
                                                  const AgentsightSessionInputState* previousState) {
    AgentsightInputUploadPlan plan;
    plan.inputMessageCount = CountJsonArrayElements(fullMessagesJson);
    if (plan.inputMessageCount > 0) {
        plan.messagesHash = HashJsonArrayPrefix(fullMessagesJson, plan.inputMessageCount);
    }

    plan.sendFullMessages = true;
    if (previousState != nullptr && previousState->messageCount > 0 && !previousState->messagesHash.empty()) {
        if (plan.inputMessageCount >= previousState->messageCount) {
            const std::string prefixHash
                = HashJsonArrayPrefix(fullMessagesJson, previousState->messageCount);
            if (!prefixHash.empty() && prefixHash == previousState->messagesHash) {
                plan.sendFullMessages = false;
            }
        }
    }

    return plan;
}

std::string ComputeInputMessagesDelta(const std::string& fullMessagesJson,
                                      const AgentsightSessionInputState* previousState) {
    const size_t curCount = CountJsonArrayElements(fullMessagesJson);
    if (curCount == 0) {
        return {};
    }

    auto finalizeDelta = [](std::string delta) {
        return SerializeJsonArrayOmitSystem(delta);
    };

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
                    = HashJsonArrayRange(fullMessagesJson, prevInCount, prevOutCount);
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

    // Legacy: messageCount once stored in+out. Recover input-only N_in when possible.
    const size_t prevOutCount = previousState->outputMessageCount;
    if (prevOutCount > 0 && prevInCount > prevOutCount) {
        const std::string inputOnlyHash
            = HashJsonArrayPrefix(fullMessagesJson, prevInCount - prevOutCount);
        if (!inputOnlyHash.empty() && inputOnlyHash == previousState->messagesHash) {
            const size_t baseIn = prevInCount - prevOutCount;
            size_t deltaStart = baseIn;
            if (!previousState->outputMessagesHash.empty() && curCount >= baseIn + prevOutCount) {
                // Same normalized replay hash vs H_out as the primary branch above.
                const std::string replayHash
                    = HashJsonArrayRange(fullMessagesJson, baseIn, prevOutCount);
                if (!replayHash.empty() && replayHash == previousState->outputMessagesHash) {
                    deltaStart = baseIn + prevOutCount;
                }
            } else if (curCount >= baseIn + prevOutCount) {
                deltaStart = baseIn + prevOutCount;
            }
            if (curCount > deltaStart) {
                return finalizeDelta(SerializeJsonArraySuffix(fullMessagesJson, deltaStart));
            }
            return {};
        }
    }

    return finalizeDelta(SerializeJsonArrayPrefix(fullMessagesJson, curCount));
}

void UpdateSessionOutputState(const std::string& responseMessagesJson, AgentsightSessionInputState& state) {
    state.outputMessageCount = CountJsonArrayElements(responseMessagesJson);
    if (state.outputMessageCount > 0) {
        state.outputMessagesHash = HashJsonArrayPrefix(responseMessagesJson, state.outputMessageCount);
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

std::string ResolveTurnStepStateKey(const std::string& sessionId, const std::string& turnId) {
    if (!sessionId.empty() && !turnId.empty()) {
        std::string key;
        key.reserve(sessionId.size() + 1 + turnId.size());
        key.append(sessionId);
        key.push_back('\x1e');
        key.append(turnId);
        return key;
    }
    return ResolveSessionStateKey(sessionId, turnId);
}

std::string FormatGenAiStepId(size_t stepNumber) {
    return "step_" + std::to_string(stepNumber);
}

} // namespace logtail::ebpf
