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

#include "observer/network/NetworkConfig.h"
#include "observer/interface/network.h"
#include "observer/interface/helper.h"
#include "common/StringPiece.h"
#include <pcap.h>
#include <functional>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include "common/DynamicLibHelper.h"


namespace logtail {

class PCAPWrapper {
public:
    PCAPWrapper(NetworkConfig* config) : mConfig(config), caches(config->mPCAPCacheConnSize) {}
    ~PCAPWrapper() { Stop(); }

    static PCAPWrapper* GetInstance() {
        static PCAPWrapper* sWrapper = new PCAPWrapper(NetworkConfig::GetInstance());
        return sWrapper;
    }

    bool Init(std::function<int(StringPiece)> processor);

    std::string GetErrorMessage() {
        if (strlen(mErrBuf) > 0) {
            return std::string(mErrBuf);
        }
        return "";
    }

    bool Start() {
        // do nonthing
        return true;
    }

    bool Stop();

    /**
     * @brief
     *
     * @param maxProcessPackets
     * @param maxProcessDurationMs
     * @return int32_t packets processed, 0 no packets, < 0 error
     */
    int32_t ProcessPackets(int32_t maxProcessPackets, int32_t maxProcessDurationMs);

    void PCAPCallBack(const struct pcap_pkthdr* packet_header, const u_char* packet_content);

    NetStaticticsMap& GetStatistics() { return mStatistics; }

    friend class PCAPWrapperUnittest;

private:
    NetworkConfig* mConfig;
    std::function<int(StringPiece)> mPacketProcessor;
    char mErrBuf[PCAP_ERRBUF_SIZE] = {'\0'};
    pcap_t* mHandle = NULL;
    struct bpf_program mBPFFilter;
    bpf_u_int32 mLocalAddress = 0;
    bpf_u_int32 mLocalMaskAddress = 0;
    DynamicLibLoader* mPCAPLib = NULL;
    NetStaticticsMap mStatistics;
    LRUCache<uint32_t, std::pair<PacketRoleType,ProtocolType>> caches;
};

} // namespace logtail