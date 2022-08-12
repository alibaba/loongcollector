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

#include <string>
#include <memory>
#include <unordered_map>
#include <fcntl.h>
#include <sys/socket.h>
#include <ostream>
#include <iostream>
#include <boost/filesystem/operations.hpp>
#include "Logger.h"
#include "interface/network.h"
#include "linux/rtnetlink.h"
#include "interface/helper.h"
#include "interface/statistics.h"


namespace logtail {

enum class TCPConnectionStat {
    Unknown = 0,
    Established = 1,
    TimeWait = 6,
    CloseWait = 8,
    Listening = 10,
};

struct ConnectionInfo {
    sa_family_t family;
    SockAddress localAddr;
    SockAddress remoteAddr;
    // local port and remote port save inode num in unix socket, so use uint32.
    uint32_t localPort;
    uint32_t remotePort;
    TCPConnectionStat stat = TCPConnectionStat::Unknown;
    PacketRoleType role;

    friend std::ostream& operator<<(std::ostream& os, const ConnectionInfo& info) {
        os << "family: " << info.family << " localAddr: " << SockAddressToString(info.localAddr)
           << " remoteAddr: " << SockAddressToString(info.remoteAddr) << " localPort: " << info.localPort
           << " remotePort: " << info.remotePort << " stat: " << ((int)info.stat)
           << " role: " << PacketRoleTypeToString(info.role);
        return os;
    }

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    void Print() { std::cout << *this << std::endl; }
};

typedef std::shared_ptr<ConnectionInfo> ConnectionInfoPtr;

struct ConnectionInfoPtrHashFn {
    size_t operator()(const ConnectionInfoPtr& ptr) const {
        size_t hash = XXH32(reinterpret_cast<char*>(ptr->localAddr.Addr.IPV6), sizeof(union SockAddressDetail), 0);
        hash = XXH32(&ptr->localPort, sizeof(ptr->localPort), hash);
        return hash;
    }
};

struct ConnectionInfoPtrEqFn {
    size_t operator()(const ConnectionInfoPtr& a, const ConnectionInfoPtr& b) const {
        return a->localPort == b->localPort && a->localAddr == b->localAddr;
    }
};


// NetLinkBinder bind the specific ns fd to the global network ns.
class NetLinkBinder {
public:
    explicit NetLinkBinder(uint32_t pid, const std::string& procPath = "/proc/");

    ~NetLinkBinder() { this->Close(); }

    bool Status() const { return this->success; }

private:
    void Close();

private:
    int mFd = -1;
    int mOrgFd = -1;
    bool success = false;
};

// NetLinkProber fetch all connections in global network ns.
class NetLinkProber {
public:
    explicit NetLinkProber(uint32_t pid, uint32_t inode, const std::string& procPath = "/proc/");

    void FetchInetConnections(std::unordered_map<uint32_t, ConnectionInfoPtr>& infos,
                              int connStat
                              = (1 << (int)TCPConnectionStat::Established) | (1 << (int)TCPConnectionStat::Listening));

    void FetchUnixConnections(std::unordered_map<uint32_t, ConnectionInfoPtr>& infos,
                              int connStat
                              = (1 << (int)TCPConnectionStat::Established) | (1 << (int)TCPConnectionStat::Listening));

    int8_t Status() const { return this->mStatus; }

    uint32_t Inode() const { return this->mInode; }

    ~NetLinkProber();

private:
    /**
     * Send dump connections request with netlink
     * reference：
     * 1.-0 https://man7.org/linux/man-pages/man7/sock_diag.7.html
     * 2. pixie
     */
    template <typename msgType>
    bool SendMsg(const msgType& msg, std::string& errorMsg);

    /**
     * Receive dump connections response with netlink
     * reference：
     * 1. https://man7.org/linux/man-pages/man7/sock_diag.7.html
     * 2. pixie
     */
    template <typename msgType>
    bool ReceiveMsg(std::unordered_map<uint32_t, ConnectionInfoPtr>& infos, std::string& errorMsg);

    int mFd = -1;
    int8_t mStatus = 0;
    uint32_t mInode = 0;
};


class NamespacedProberManger {
public:
    static NamespacedProberManger* GetInstance(std::string& baseProcPath) {
        static auto* instance = new NamespacedProberManger(baseProcPath);
        return instance;
    }

    std::shared_ptr<NetLinkProber> GetOrCreateProber(uint32_t pid);

    void GarbageCollection();

private:
    explicit NamespacedProberManger(std::string& baseProcPath) : mBaseProcPath(baseProcPath) {}

    std::unordered_map<uint32_t, std::shared_ptr<NetLinkProber>> mProbers{};
    std::string mBaseProcPath;
};


class ConnectionMetaManager {
public:
    static ConnectionMetaManager* GetInstance() {
        static auto* instance = new ConnectionMetaManager;
        return instance;
    }
    bool Init(const std::string& procBashPath = "/proc/");

    ConnectionInfoPtr GetConnectionInfo(uint32_t pid, uint32_t fd);

    bool GarbageCollection();

    void Print();

private:
    ConnectionMetaManager() { mConnMetaStatistic = ConnectionMetaStatistic::GetInstance(); }

private:
    ConnectionMetaStatistic* mConnMetaStatistic;
    std::string mBashProcPath;
    std::unordered_map<uint32_t, ConnectionInfoPtr> mConnectionMeta{};
    std::unordered_set<uint32_t> mProberFetchLog{};
};


uint32_t ReadInodeNum(const std::string& path, const std::string& prefix, int8_t& errorCode);

uint32_t ReadNetworkNsInodeNum(const std::string& path, int8_t& errorCode);

uint32_t ReadSocketInodeNum(const std::string& path, int8_t& errorCode);

void ReadFdLink(std::string& fdPath, std::string& fdLinkPath);
} // namespace logtail
