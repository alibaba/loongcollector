#pragma once
#include <string>

namespace logtail {
namespace observer {
    // process label keys
    extern std::string kLocalInfo;
    extern std::string kRemoteInfo;
    extern std::string kRole;
    extern std::string kType;

    // connection label keys
    extern std::string kConnId;
    extern std::string kConnType;
    extern std::string kLocalAddr;
    extern std::string kLocalPort;
    extern std::string kRemoteAddr;
    extern std::string kRemotePort;
    extern std::string kInterval;

    // value names
    extern std::string kSendBytes;
    extern std::string kRecvBytes;
    extern std::string kSendpackets;
    extern std::string kRecvPackets;

    // metric names
    extern std::string kQueryCmd;
    extern std::string kQuery;
    extern std::string kStatus;
    extern std::string kReqType;
    extern std::string kReqDomain;
    extern std::string kReqResource;
    extern std::string kRespStatus;
    extern std::string kRespCode;
    extern std::string kLatencyNs;
    extern std::string kReqBytes;
    extern std::string kRespBytes;
    extern std::string kCount;
    extern std::string kProtocol;
    extern std::string kVersion;
    extern std::string kTdigestLatency;

} // namespace observer
} // namespace logtail
