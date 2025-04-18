/*
 * Copyright 2022 iLogtail Authors
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

#include <array>
#include <string>
#include <unordered_set>

#include "json/value.h"

#include "AppConfig.h"
#include "common/StringView.h"

namespace logtail {

static const size_t ID_MAX_LENGTH = 128;
template <size_t N>
inline void SetID(const std::string& id, std::array<char, N>& target, size_t& targetLen) {
    if (id.empty()) {
        target[0] = '\0';
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
    size_t mInstanceIDLen = 0UL;

    std::array<char, ID_MAX_LENGTH> mUserID{};
    size_t mUserIDLen = 0UL;

    std::array<char, ID_MAX_LENGTH> mRegionID{};
    size_t mRegionIDLen = 0UL;

    friend class InstanceIdentityUnittest;
};
struct Hostid {
    enum Type {
        CUSTOM,
        ECS,
        ECS_ASSIST,
        LOCAL,
    };
    static std::string TypeToString(Type type) {
        switch (type) {
            case Type::CUSTOM:
                return "CUSTOM";
            case Type::ECS:
                return "ECS";
            case Type::ECS_ASSIST:
                return "ECS_ASSIST";
            case Type::LOCAL:
                return "LOCAL";
            default:
                return "UNKNOWN";
        }
    }

    Hostid() = default;
    Hostid(const std::string& id, const Type& type) { SetHostID(id, type); }

    void SetHostID(const std::string& id, const Type& type) {
        SetID(id, mId, mIdLen);
        mType = type;
    }

    [[nodiscard]] StringView GetHostID() const { return StringView(mId.data(), mIdLen); }

    [[nodiscard]] Type GetType() const { return mType; }

private:
    std::array<char, ID_MAX_LENGTH> mId{};
    size_t mIdLen = 0UL;

    Type mType;

    friend class InstanceIdentityUnittest;
};
class Entity {
public:
    bool IsECSValid() const { return mECSMeta.IsValid(); }
    StringView GetEcsInstanceID() const { return mECSMeta.GetInstanceID(); }
    StringView GetEcsUserID() const { return mECSMeta.GetUserID(); }
    StringView GetEcsRegionID() const { return mECSMeta.GetRegionID(); }
    StringView GetHostID() const { return mHostid.GetHostID(); }
    Hostid::Type GetHostIdType() const { return mHostid.GetType(); }
    [[nodiscard]] const ECSMeta& GetECSMeta() const { return mECSMeta; }

    void SetECSMeta(const ECSMeta& meta) { mECSMeta = meta; }
    void SetHostID(const Hostid& hostid) { mHostid = hostid; }

private:
    ECSMeta mECSMeta;
    Hostid mHostid;
};

std::string GetOsDetail();
std::string GetUsername();
std::string GetHostName();
std::string GetHostIpByHostName();
std::string GetHostIpByInterface(const std::string& intf);
uint32_t GetHostIpValueByInterface(const std::string& intf);
std::string GetHostIp(const std::string& intf = "");
void GetAllPids(std::unordered_set<int32_t>& pids);
bool GetKernelInfo(std::string& kernelRelease, int64_t& kernelVersion);
bool GetRedHatReleaseInfo(std::string& os, int64_t& osVersion, std::string bashPath = "");
bool IsDigitsDotsHostname(const char* hostname);
// GetAnyAvailableIP walks through all interfaces (AF_INET) to find an available IP.
// Priority:
// - IP that does not start with "127.".
// - IP from interface at first.
//
// NOTE: logger must be initialized before calling this.
std::string GetAnyAvailableIP();

bool FetchECSMeta(ECSMeta& metaObj);

class InstanceIdentity {
public:
    InstanceIdentity();
    static InstanceIdentity* Instance() {
        static InstanceIdentity sInstance;
        return &sInstance;
    }

    // 注意: 不要在类初始化时调用并缓存结果，因为此时ECS元数据可能尚未就绪
    // 建议在实际使用时再调用此方法
    const Entity* GetEntity() { return &mEntity.getReadBuffer(); }

    bool UpdateInstanceIdentity(const ECSMeta& meta);
    void DumpInstanceIdentity();

    bool InitFromFile();

    void InitFromNetwork();

private:
    // 从云助手获取序列号
    void getSerialNumberFromEcsAssist();
    // 从本地文件获取hostid
    void getLocalHostId();

    void updateHostId(const ECSMeta& meta);
    void dumpInstanceIdentityToFile();

#if defined(_MSC_VER)
    std::string mEcsAssistMachineIdFile = "C:\\ProgramData\\aliyun\\assist\\hybrid\\machine-id";
#else
    std::string mEcsAssistMachineIdFile = "/usr/local/share/aliyun-assist/hybrid/machine-id";
#endif

    bool mHasTriedToGetSerialNumber = false;
    std::string mSerialNumber;

    DoubleBuffer<Entity> mEntity;

    Json::Value mInstanceIdentityJson;
    std::string mInstanceIdentityFile;
    bool mHasGeneratedLocalHostId = false;
    std::string mLocalHostId;
#ifdef __ENTERPRISE__
    friend class EnterpriseConfigProvider;
#endif
};

} // namespace logtail
