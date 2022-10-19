#pragma once
#include <string>

namespace logtail {
namespace observer {
    // process label keys
    extern std::string kLocalInfo;
    extern std::string kRemoteInfo;
    extern std::string kRole;

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
} // namespace observer
} // namespace logtail
