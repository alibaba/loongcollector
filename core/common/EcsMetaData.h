/*
 * Copyright 2025 loongcollector Authors
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

#pragma once

#include <common/StringView.h>
#include <string.h>

#include <array>

#include "json/value.h"

extern const std::string sInstanceIdKey;
extern const std::string sOwnerAccountIdKey;
extern const std::string sRegionIdKey;
extern const std::string sAccessKeyId;
extern const std::string sAccessKeySecret;
extern const std::string sSecurityToken;
extern const std::string sExpiration;
extern const char* sEcsRamTimeFormat;

namespace logtail {
static const size_t ID_MAX_LENGTH = 128;

enum class EcsMetaDataType { META, RAM_CREDENTIALS };

template <size_t N>
inline void SetID(const std::string& id, std::array<char, N>& target, size_t& targetLen) {
    if (id.empty()) {
        target[0] = '\0';
        targetLen = 0;
        return;
    }

    if (N == 0) {
        targetLen = 0;
        return;
    }

    targetLen = std::min(id.size(), N - 1);
    std::memcpy(target.data(), id.data(), targetLen);
    target[targetLen] = '\0';
}
struct ECSMeta {
    ECSMeta() = default;

    void SetInstanceID(const std::string& id) { SetID(id, mInstanceID, mInstanceIDLen); }

    void SetUserID(const std::string& id) { SetID(id, mUserID, mUserIDLen); }

    void SetRegionID(const std::string& id) { SetID(id, mRegionID, mRegionIDLen); }

    [[nodiscard]] StringView GetInstanceID() const { return StringView(mInstanceID.data(), mInstanceIDLen); }
    [[nodiscard]] StringView GetUserID() const { return StringView(mUserID.data(), mUserIDLen); }
    [[nodiscard]] StringView GetRegionID() const { return StringView(mRegionID.data(), mRegionIDLen); }

    [[nodiscard]] bool IsValid() const {
        return !GetInstanceID().empty() && !GetUserID().empty() && !GetRegionID().empty();
    }

private:
    std::array<char, ID_MAX_LENGTH> mInstanceID{};
    size_t mInstanceIDLen = (size_t)0;

    std::array<char, ID_MAX_LENGTH> mUserID{};
    size_t mUserIDLen = (size_t)0;

    std::array<char, ID_MAX_LENGTH> mRegionID{};
    size_t mRegionIDLen = (size_t)0;

    friend class InstanceIdentityUnittest;
};

size_t FetchECSMetaCallback(char* buffer, size_t size, size_t nmemb, std::string* res);

bool ParseECSMeta(const std::string& meta, ECSMeta& metaObj);

bool ParseCredentials(const Json::Value& doc,
                      std::string& accessKeyId,
                      std::string& accessKeySecret,
                      std::string& secToken,
                      int64_t& expTime);

bool FetchECSMeta(ECSMeta& metaObj);

bool FetchECSRamCredentials(std::string& accessKeyId,
                            std::string& accessKeySecret,
                            std::string& secToken,
                            int64_t& expTime);
} // namespace logtail
