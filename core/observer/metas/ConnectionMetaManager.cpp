// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sys/socket.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include "ConnectionMetaManager.h"
#include "FileSystemUtil.h"
#include "Logger.h"
#include <netinet/in.h>
#include "LogtailAlarm.h"
#include "MachineInfoUtil.h"
#include "DynamicLibHelper.h"

namespace logtail {

uint32_t ReadInodeNum(const std::string& path, const std::string& prefix, int8_t& errorCode) {
    size_t pathLen = path.size(), prefixLen = prefix.size();
    if (pathLen - prefixLen < 3 || path.compare(0, prefixLen, prefix)) {
        errorCode = -1;
        return 0;
    }
    if (path[prefixLen] != '[' || path[pathLen - 1] != ']') {
        errorCode = -2;
        return 0;
    }
    errorCode = 0;
    return (uint32_t)std::strtol(path.c_str() + prefixLen + 1, NULL, 10);
}

uint32_t ReadNetworkNsInodeNum(const std::string& path, int8_t& errorCode) {
    const static std::string prefix = "net:";
    return ReadInodeNum(path, prefix, errorCode);
}

uint32_t ReadSocketInodeNum(const std::string& path, int8_t& errorCode) {
    const static std::string prefix = "socket:";
    return ReadInodeNum(path, prefix, errorCode);
}


bool ConnectionMetaManager::Init(const std::string& procBashPath) {
    if (!this->mBashProcPath.empty()) {
        return true;
    }
    std::string bashPath = procBashPath;
    if (procBashPath[procBashPath.size() - 1] != '/') {
        bashPath.append("/");
    }

    if (!IsAccessibleDirectory(bashPath)) {
        LOG_ERROR(sLogger, ("init observer connection manager", "fail")("proc path", bashPath));
        LogtailAlarm::GetInstance()->SendAlarm(
            OBSERVER_INIT_ALARM,
            "init observer connection manager fails because the proc path cannot be accessed: " + bashPath);
        return false;
    }
    this->mBashProcPath = bashPath;
    LOG_INFO(sLogger, ("init observer connection manager", "success")("proc path", bashPath));
    return true;
}

ConnectionInfoPtr ConnectionMetaManager::GetConnectionInfo(uint32_t pid, uint32_t fd) {
    ++mConnMetaStatistic->mGetSocketInfoCount;
    std::string fdPath = this->mBashProcPath;
    fdPath.append(std::to_string(pid)).append("/fd/").append(std::to_string(fd));
    std::string fdLinkPath;
    ReadFdLink(fdPath, fdLinkPath);
    if (fdLinkPath.empty()) {
        LOG_DEBUG(sLogger, ("fetch connection meta", "fail")("path", fdPath));
        ++mConnMetaStatistic->mGetSocketInfoFailCount;
        return nullptr;
    }
    int8_t errorCode;
    uint32_t inode = ReadSocketInodeNum(fdLinkPath, errorCode);
    if (errorCode < 0) {
        LOG_DEBUG(sLogger, ("fetch connection meta", "fail")("fdLink", fdLinkPath));
        ++mConnMetaStatistic->mGetSocketInfoFailCount;
        return nullptr;
    }
    auto meta = mConnectionMeta.find(inode);
    if (meta != mConnectionMeta.end()) {
        return meta->second;
    }
    static auto sProberManger = NamespacedProberManger::GetInstance(this->mBashProcPath);
    auto prober = sProberManger->GetOrCreateProber(pid);
    ++mConnMetaStatistic->mGetNetlinkProberCount;
    if (prober == nullptr) {
        LOG_DEBUG(sLogger, ("fetch connection meta", "fail")("prober create fail", pid));
        ++mConnMetaStatistic->mGetSocketInfoFailCount;
        ++mConnMetaStatistic->mGetNetlinkProberFailCount;
        return nullptr;
    }
    if (mProberFetchLog.find(prober->Inode()) != mProberFetchLog.end()) {
        ++mConnMetaStatistic->mGetSocketInfoFailCount;
        return nullptr;
    }
    mProberFetchLog.insert(prober->Inode());
    ++mConnMetaStatistic->mFetchNetlinkCount;
    prober->FetchInetConnections(this->mConnectionMeta);
    prober->FetchUnixConnections(this->mConnectionMeta);

    meta = mConnectionMeta.find(inode);
    if (meta != mConnectionMeta.end()) {
        LOG_DEBUG(
            sLogger,
            ("ConnectionManager find info", "success")("pid", pid)("inode", inode)("meta", meta->second->ToString()));
        return meta->second;
    }
    LOG_DEBUG(sLogger, ("ConnectionManager find info", "fail")("pid", pid)("inode", inode));
    ++mConnMetaStatistic->mGetSocketInfoFailCount;
    return nullptr;
}

bool ConnectionMetaManager::GarbageCollection() {
    mConnectionMeta.erase(this->mConnectionMeta.begin(), this->mConnectionMeta.end());
    mProberFetchLog.erase(this->mProberFetchLog.begin(), this->mProberFetchLog.end());
    static auto sProberManger = NamespacedProberManger::GetInstance(this->mBashProcPath);
    sProberManger->GarbageCollection();
    return true;
}

void ConnectionMetaManager::Print() {
    for (const auto& item : this->mConnectionMeta) {
        std::cout << "inode:" << item.first << " info: " << item.second->ToString() << std::endl;
    }
}

NetLinkProber::NetLinkProber(uint32_t pid, uint32_t inode, const std::string& procPath) {
    this->mInode = inode;
    NetLinkBinder binder(pid, procPath);
    // only create netlink socket when bind success, otherwise would get wrong connections.
    if (!binder.Status()) {
        this->mStatus = -1;
        LOG_DEBUG(sLogger, ("bind network ns", "fail"));
        return;
    }
    this->mFd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_SOCK_DIAG);
    if (mFd < 0) {
        LOG_DEBUG(sLogger, ("open netlink socket", "fail"));
        this->mStatus = -2;
        return;
    }
    LOG_DEBUG(sLogger, ("open netlink socket", "success"));
}

template <typename msgType>
bool NetLinkProber::SendMsg(const msgType& realMsg, std::string& errorMsg) {
    struct sockaddr_nl nladdr = {.nl_family = AF_NETLINK};
    struct {
        struct nlmsghdr nlh;
        msgType msg;
    } req{
                .nlh={
                        .nlmsg_len = sizeof(req),
                        .nlmsg_type=SOCK_DIAG_BY_FAMILY,
                        .nlmsg_flags= NLM_F_REQUEST | NLM_F_DUMP,
                },
                .msg=realMsg,
        };
    struct iovec iov {
        .iov_base = &req, .iov_len = sizeof(req),
    };
    struct msghdr msg = {
        .msg_name = &nladdr,
        .msg_namelen = sizeof(nladdr),
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    ssize_t sentTotal = 0;

    while (sentTotal < (ssize_t)iov.iov_len) {
        ssize_t sendBytes = sendmsg(this->mFd, &msg, 0);
        if (sendBytes < 0) {
            errorMsg = "cannot send msg to netlink, fd:" + std::to_string(this->mFd);
            return false;
        }
        LOG_DEBUG(sLogger, ("send length", sendBytes));
        sentTotal += sendBytes;
    }
    return true;
}

template <typename msgType>
bool NetLinkProber::ReceiveMsg(std::unordered_map<uint32_t, ConnectionInfoPtr>& infos, std::string& errorMsg) {
    static int bufSize = 8192;
    long buffer[bufSize / sizeof(long)];

    struct sockaddr_nl nladdr = {};
    struct iovec iov = {.iov_base = buffer, .iov_len = sizeof(buffer)};

    while (true) {
        msghdr msg = {.msg_name = &nladdr, .msg_namelen = sizeof(nladdr), .msg_iov = &iov, .msg_iovlen = 1};

        ssize_t ret = recvmsg(this->mFd, &msg, 0);
        if (ret < 0) {
            errorMsg = "cannot read msg from netlink" + std::to_string(this->mFd);
            return false;
        }
        if (ret == 0) {
            errorMsg = "read zero bytes msg from netlink" + std::to_string(this->mFd);
            return false;
        }
        if (nladdr.nl_family != AF_NETLINK) {
            errorMsg = "read the wrong msg because nl_family is not AF_NETLINK, which is "
                + std::to_string(nladdr.nl_family);
            return false;
        }
        LOG_DEBUG(sLogger, ("receive length", ret));
        const struct nlmsghdr* header = (struct nlmsghdr*)buffer;

        for (; NLMSG_OK(header, ret); header = NLMSG_NEXT(header, ret)) {
            if (header->nlmsg_type == NLMSG_DONE)
                return true;

            if (header->nlmsg_type == NLMSG_ERROR) {
                errorMsg = "netlink error";
                return false;
            }
            if (header->nlmsg_type != SOCK_DIAG_BY_FAMILY) {
                errorMsg = "not SOCK_DIAG_BY_FAMILY msg";
                return false;
            }
            auto* data = reinterpret_cast<msgType*>(NLMSG_DATA(header));
            if (!ExtractDiagMsg(*data, header->nlmsg_len, infos, errorMsg)) {
                return false;
            }
        }
    }
    return true;
}

void NetLinkProber::FetchInetConnections(std::unordered_map<uint32_t, ConnectionInfoPtr>& infos, int connStat) {
    inet_diag_req_v2 req = {};
    req.sdiag_protocol = IPPROTO_TCP;
    req.idiag_states = connStat;
    std::string errorMsg;

    req.sdiag_family = AF_INET;
    this->SendMsg(req, errorMsg);
    if (!errorMsg.empty()) {
        LOG_DEBUG(sLogger, ("fetch inet connection error", errorMsg));
        return;
    }
    LOG_DEBUG(sLogger, ("send inet msg", "success"));
    this->ReceiveMsg<inet_diag_msg>(infos, errorMsg);
    if (!errorMsg.empty()) {
        LOG_DEBUG(sLogger, ("fetch inet connection error", errorMsg));
        return;
    }
    LOG_DEBUG(sLogger, ("receive inet msg", "success")("size", infos.size()));
    req.sdiag_family = AF_INET6;
    this->SendMsg(req, errorMsg);
    if (!errorMsg.empty()) {
        LOG_DEBUG(sLogger, ("fetch inet connection error", errorMsg));
        return;
    }
    this->ReceiveMsg<inet_diag_msg>(infos, errorMsg);
    if (!errorMsg.empty()) {
        LOG_DEBUG(sLogger, ("fetch inet connection error", errorMsg));
        return;
    }

    std::unordered_set<ConnectionInfoPtr, ConnectionInfoPtrHashFn, ConnectionInfoPtrEqFn> connSet;
    for (const auto& item : infos) {
        if (item.second->stat == TCPConnectionStat::Listening) {
            connSet.insert(item.second);
        }
    }
    ConnectionInfoPtr ip = std::make_shared<ConnectionInfo>();
    ip->localAddr.Addr = {};
    for (const auto& item : infos) {
        ip->localPort = item.second->localPort;
        ip->localAddr.Type = item.second->localAddr.Type;
        if (connSet.find(ip) != connSet.end() || connSet.find(item.second) != connSet.end()) {
            item.second->role = PacketRoleType::Server;
        } else {
            item.second->role = PacketRoleType::Client;
        }
    }
}

void NetLinkProber::FetchUnixConnections(std::unordered_map<uint32_t, ConnectionInfoPtr>& infos, int connStat) {
    unix_diag_req req = {};
    std::string errorMsg;
    req.sdiag_family = AF_UNIX;
    req.udiag_states = connStat;
    req.udiag_show = UDIAG_SHOW_NAME | UDIAG_SHOW_PEER;

    this->SendMsg(req, errorMsg);
    if (!errorMsg.empty()) {
        LOG_DEBUG(sLogger, ("fetch unix connection error", errorMsg));
        return;
    }
    this->ReceiveMsg<unix_diag_msg>(infos, errorMsg);
    if (!errorMsg.empty()) {
        LOG_DEBUG(sLogger, ("fetch unix connection error", errorMsg));
        return;
    }
}

NetLinkProber::~NetLinkProber() {
    if (mFd >= 0) {
        close(mFd);
        LOG_DEBUG(sLogger, ("close netlink socket ", "success"));
    }
}

bool ExtractDiagMsg(const inet_diag_msg& msg,
                    uint32_t len,
                    std::unordered_map<uint32_t, ConnectionInfoPtr>& infos,
                    std::string& errorMsg) {
    if (len < sizeof(msg)) {
        errorMsg = "no enough netlink data";
        return false;
    }
    if (msg.idiag_family != AF_INET && msg.idiag_family != AF_INET6) {
        errorMsg = "unsupported idiag family " + std::to_string(msg.idiag_family);
        return false;
    }

    uint32_t inode = msg.idiag_inode;
    if (inode == 0) {
        return true;
    }
    auto iter = infos.find(inode);
    if (iter != infos.end()) {
        errorMsg = "duplicate inode msg " + std::to_string(inode);
        return false;
    }
    auto info = std::make_shared<ConnectionInfo>();
    info->family = msg.idiag_family;
    info->localPort = ntohs(msg.id.idiag_sport);
    info->remotePort = ntohs(msg.id.idiag_dport);
    info->stat = static_cast<TCPConnectionStat>(msg.idiag_state);

    if (msg.idiag_family == AF_INET) {
        info->localAddr = SockAddress{.Type = SockAddressType_IPV4,
                                      .Addr = SockAddressDetail{
                                          .IPV4 = msg.id.idiag_src[0],
                                      }};
        info->remoteAddr = SockAddress{.Type = SockAddressType_IPV4,
                                       .Addr = SockAddressDetail{
                                           .IPV4 = msg.id.idiag_dst[0],
                                       }};
    } else if (msg.idiag_family == AF_INET6) {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif // __GNUC__
        info->localAddr = SockAddress{.Type = SockAddressType_IPV6,
                                      .Addr = SockAddressDetail{
                                          .IPV6 = {((uint64_t*)msg.id.idiag_src)[0], ((uint64_t*)msg.id.idiag_src)[1]},
                                      }};
        info->remoteAddr = SockAddress{.Type = SockAddressType_IPV6,
                                       .Addr = SockAddressDetail{
                                           .IPV6 = {((uint64_t*)msg.id.idiag_dst)[0], ((uint64_t*)msg.id.idiag_dst)[1]},
                                       }};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif // __GNUC__
    }
    infos.insert(std::make_pair(inode, info));
    return true;
}

bool ExtractDiagMsg(const unix_diag_msg& msg,
                    uint32_t len,
                    std::unordered_map<uint32_t, ConnectionInfoPtr>& infos,
                    std::string& errorMsg) {
    if (len < sizeof(msg)) {
        errorMsg = "no enough netlink data";
        return false;
    }
    if (msg.udiag_family != AF_UNIX) {
        errorMsg = "unsupported idiag family " + std::to_string(msg.udiag_family);
        return false;
    }
    unsigned int rta_len = len - NLMSG_LENGTH(sizeof(msg));
    unsigned int peer = 0;

    for (auto* attr = (struct rtattr*)(&msg + 1); RTA_OK(attr, rta_len); attr = RTA_NEXT(attr, rta_len)) {
        switch (attr->rta_type) {
            case UNIX_DIAG_NAME:
                break;
            case UNIX_DIAG_PEER:
                if (RTA_PAYLOAD(attr) >= sizeof(peer))
                    peer = *(unsigned int*)RTA_DATA(attr);
                break;
        }
    }
    auto iter = infos.find(msg.udiag_ino);
    if (iter != infos.end()) {
        errorMsg = "duplicate inode msg " + std::to_string(msg.udiag_ino);
        return false;
    }
    auto info = std::make_shared<ConnectionInfo>();
    info->family = msg.udiag_family;
    info->localPort = msg.udiag_ino;
    info->remotePort = peer;
    info->stat = static_cast<TCPConnectionStat>(msg.udiag_state);
    info->localAddr = SockAddress{.Type = SockAddressType_IPV4,
                                  .Addr = SockAddressDetail{
                                      .IPV4 = 0,
                                  }};
    info->remoteAddr = info->localAddr;
    infos.insert(std::make_pair(msg.udiag_ino, info));
    return true;
}


NetLinkBinder::NetLinkBinder(uint32_t pid, const std::string& procPath) {
    static const std::string selfPath = "self/ns/net";
    std::string bashPath = procPath;
    if (procPath[procPath.size() - 1] != '/') {
        bashPath.append("/");
    }
    std::string selfNsPath = bashPath + selfPath;
    this->mOrgFd = open((selfNsPath).c_str(), O_RDONLY);
    if (this->mOrgFd < 0) {
        LOG_DEBUG(sLogger, ("netlink status", "fail")("reason", "open self net ns fail")("path", selfNsPath));
        this->Close();
        return;
    } else {
        LOG_DEBUG(sLogger, ("open self net ns", "success")("path", selfNsPath));
    }
    std::string pidNsPath = bashPath;
    pidNsPath.append(std::to_string(pid)).append("/ns/net");
    this->mFd = open(pidNsPath.c_str(), O_RDONLY);
    if (this->mFd < 0) {
        LOG_DEBUG(sLogger, ("netlink status", "fail")("reason", "open pid net ns fail")("path", pidNsPath));
        this->Close();
        return;
    } else {
        LOG_DEBUG(sLogger, ("open pid net ns", "success")("path", pidNsPath));
    }
    int res = glibc::g_setns_func(this->mFd, 0);
    if (res != 0) {
        LOG_DEBUG(sLogger, ("netlink status", "fail")("reason", "set ns fail")("path", pidNsPath));
        this->Close();
        return;
    } else {
        LOG_DEBUG(sLogger, ("set ns status", "success"));
    }
    this->success = true;
}


void NetLinkBinder::Close() {
    if (this->success && mOrgFd >= 0) {
        int res = glibc::g_setns_func(this->mOrgFd, 0);
        if (res != 0) {
            LOG_ERROR(sLogger, ("netlink status", "unknown")("reason", "recover netlink net ns fail"));
        } else {
            LOG_DEBUG(sLogger, ("recover netlink net ns", "success"));
        }
    }
    if (this->mFd >= 0) {
        close(this->mFd);
        this->mFd = -1;
        LOG_DEBUG(sLogger, ("close pid net ns fd", "success"));
    }
    if (this->mOrgFd >= 0) {
        close(this->mOrgFd);
        this->mOrgFd = -1;
        LOG_DEBUG(sLogger, ("close self net ns fd", "success"));
    }
}

std::shared_ptr<NetLinkProber> NamespacedProberManger::GetOrCreateProber(uint32_t pid) {
    std::string nsPath = this->mBaseProcPath;
    nsPath.append(std::to_string(pid)).append("/ns/net");
    std::string fdLink;
    ReadFdLink(nsPath, fdLink);
    if (fdLink.empty()) {
        LOG_DEBUG(sLogger, ("get netlink prober", "fail")("cannot read fdlink path", fdLink));
        return nullptr;
    }
    int8_t errorCode;
    uint32_t inode = ReadNetworkNsInodeNum(fdLink, errorCode);
    if (errorCode < 0) {
        LOG_DEBUG(sLogger, ("get netlink prober", "fail")("cannot read net inode", nsPath)("error", errorCode));
        return nullptr;
    }
    auto item = this->mProbers.find(inode);
    if (item != this->mProbers.end()) {
        return item->second;
    }
    auto prober = std::make_shared<NetLinkProber>(pid, inode, this->mBaseProcPath);
    if (prober->Status() < 0) {
        LOG_DEBUG(sLogger, ("get netlink prober", "fail")("prober create fail", prober->Status()));
        return nullptr;
    }
    this->mProbers.insert(std::make_pair(inode, prober));
    return prober;
}

// Clear all namespaced probers and close their Fd.
void NamespacedProberManger::GarbageCollection() {
    this->mProbers.erase(this->mProbers.begin(), this->mProbers.end());
}

void ReadFdLink(std::string& fdPath, std::string& fdLinkPath) {
    boost::system::error_code ec;
    auto path = boost::filesystem::read_symlink(fdPath, ec);
    if (!ec) {
        fdLinkPath = path.string();
    }
}
} // namespace logtail