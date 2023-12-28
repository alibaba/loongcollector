/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
