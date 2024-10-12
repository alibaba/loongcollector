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

#include "PCAPWrapper.h"

#include <utility>
#include "common/xxhash/xxhash.h"
#include "common/MachineInfoUtil.h"
#include "logger/Logger.h"
#include "network/protocols/infer.h"
#include "RuntimeUtil.h"
#include "network/protocols/utils.h"
#include "interface/layerfour.h"

static void OnPCAPPacketsCallBack(u_char* user, const struct pcap_pkthdr* packet_header, const u_char* packet_content) {
    logtail::PCAPWrapper* wrapper = (logtail::PCAPWrapper*)user;
    wrapper->PCAPCallBack(packet_header, packet_content);
}

typedef int (*pcap_compile_func)(pcap_t*, struct bpf_program*, const char*, int, bpf_u_int32);
typedef char* (*pcap_lookupdev_func)(char*);
typedef int (*pcap_lookupnet_func)(const char*, bpf_u_int32*, bpf_u_int32*, char*);
typedef pcap_t* (*pcap_open_live_func)(const char*, int, int, int, char*);
typedef int (*pcap_setfilter_func)(pcap_t*, struct bpf_program*);
typedef int (*pcap_dispatch_func)(pcap_t*, int, pcap_handler, u_char*);
typedef void (*pcap_close_func)(pcap_t*);
typedef char* (*pcap_geterr_func)(pcap_t*);

pcap_compile_func g_pcap_compile_func = NULL; // pcap_compile
pcap_lookupdev_func g_pcap_lookupdev_func = NULL; // pcap_lookupdev
pcap_lookupnet_func g_pcap_lookupnet_func = NULL; // pcap_lookupnet
pcap_open_live_func g_pcap_open_live_func = NULL; // pcap_open_live
pcap_setfilter_func g_pcap_setfilter_func = NULL; // pcap_setfilter
pcap_dispatch_func g_pcap_dispatch_func = NULL; // pcap_dispatch
pcap_close_func g_pcap_close_func = NULL; // pcap_close
pcap_geterr_func g_pcap_geterr_func = NULL; // pcap_geterr

static bool PCAPLoadSuccess() {
    return g_pcap_compile_func != NULL && g_pcap_lookupdev_func != NULL && g_pcap_lookupnet_func != NULL
        && g_pcap_open_live_func != NULL && g_pcap_setfilter_func != NULL && g_pcap_dispatch_func != NULL
        && g_pcap_geterr_func != NULL && g_pcap_close_func != NULL;
}

#define LOAD_PCAP_FUNC(funcName) \
    { \
        void* funcPtr = mPCAPLib->LoadMethod(#funcName, loadErr); \
        if (funcPtr == nullptr) { \
            LOG_ERROR(sLogger, ("load pcap method", "failed")("method", #funcName)("error", loadErr)); \
            LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM, \
                                                   std::string("load pcap method failed: ") + #funcName); \
            return false; \
        } \
        g_##funcName##_func = funcName##_func(funcPtr); \
    }

namespace logtail {
bool PCAPWrapper::Stop() {
    if (mHandle != NULL && g_pcap_close_func != NULL) {
        LOG_INFO(sLogger, ("pcap close", "begin"));
        g_pcap_close_func(mHandle);
        LOG_INFO(sLogger, ("pcap close", "success"));
        mHandle = NULL;
        memset(mErrBuf, 0, sizeof(mErrBuf));
    }
    return true;
}
bool PCAPWrapper::Init(std::function<int(StringPiece)> processor) {
    LOG_INFO(sLogger, ("init pcap", "begin"));
    mHandle = NULL;
    if (mPCAPLib == NULL) {
        LOG_INFO(sLogger, ("load pcap dynamic library", "begin"));
        mPCAPLib = new DynamicLibLoader;
        std::string loadErr;
        // pcap lib load
        if (!mPCAPLib->LoadDynLib("pcap", loadErr, GetProcessExecutionDir())) {
            if (!mPCAPLib->LoadDynLib("pcap", loadErr)) {
                LOG_ERROR(sLogger, ("load pcap dynamic library", "failed")("error", loadErr));
                LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM, "cannot load pcap dynamic library");
                return false;
            }
        }
        LOAD_PCAP_FUNC(pcap_compile);
        LOAD_PCAP_FUNC(pcap_lookupdev);
        LOAD_PCAP_FUNC(pcap_lookupnet);
        LOAD_PCAP_FUNC(pcap_open_live);
        LOAD_PCAP_FUNC(pcap_setfilter);
        LOAD_PCAP_FUNC(pcap_dispatch);
        LOAD_PCAP_FUNC(pcap_close);
        LOAD_PCAP_FUNC(pcap_geterr);

        LOG_INFO(sLogger, ("load pcap dynamic library", "success"));
    }
    if (!PCAPLoadSuccess()) {
        LOG_ERROR(sLogger, ("load pcap dynamic library", "failed")("error", "last load failed"));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM, "cannot load pcap dynamic library");
        return false;
    }
    mPacketProcessor = std::move(processor);
    const char* netInterface;
    if (!mConfig->mPCAPInterface.empty()) {
        netInterface = mConfig->mPCAPInterface.c_str();
    } else {
        netInterface = g_pcap_lookupdev_func(mErrBuf);
    }
    if (netInterface == nullptr) {
        LOG_ERROR(sLogger, ("init pcap wrapper when get net interface error, err", mErrBuf));
        LogtailAlarm::GetInstance()->SendAlarm(
            OBSERVER_INIT_ALARM, "cannot find any net interface in pcap source, err: " + std::string(mErrBuf));
        return false;
    }
    LOG_INFO(sLogger, ("init pcap with net interface", netInterface));
    bpf_u_int32 netp;
    bpf_u_int32 maskp;

    // Get netmask
    if (g_pcap_lookupnet_func(netInterface, &netp, &maskp, mErrBuf) == PCAP_ERROR) {
        LOG_ERROR(sLogger, ("init pcap wrapper when loop up net error, err", mErrBuf));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM,
                                               "cannot find any netmask in pcap source, err: " + std::string(mErrBuf));
        return false;
    }

    // the device that we specified in the previous section
    // snaplen is an integer which defines the maximum number of bytes to be captured by pcap
    // promisc, when set to true, brings the interface into promiscuous mode (however, even if it is set to false, it is
    // possible under specific cases for the interface to be in promiscuous mode, anyway) to_ms is the read time out in
    // milliseconds (a value of 0 sniffs until an error occurs; -1 sniffs indefinitely)
    mHandle = g_pcap_open_live_func(netInterface, BUFSIZ, mConfig->mPCAPPromiscuous, mConfig->mPCAPTimeoutMs, mErrBuf);
    if (mHandle == NULL) {
        LOG_ERROR(sLogger, ("init pcap wrapper when open pcap live error, err", mErrBuf));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM,
                                               "cannot open pcap live, err: " + std::string(mErrBuf));
        return false;
    }
    // optimize controls whether optimization on the
    // resulting code is performed.  netmask specifies the IPv4 netmask
    // of the network on which packets are being captured; it is used
    // only when checking for IPv4 broadcast addresses in the filter
    // program.
    mLocalAddress = GetHostIpValueByInterface(std::string(netInterface));
    mLocalMaskAddress = netp;

    SockAddress addressLocal, addressNet, addressMask;
    addressNet.Type = SockAddressType_IPV4;
    addressNet.Addr.IPV4 = netp;
    addressMask.Type = SockAddressType_IPV4;
    addressMask.Addr.IPV4 = maskp;
    addressLocal.Type = SockAddressType_IPV4;
    addressLocal.Addr.IPV4 = mLocalAddress;

    LOG_INFO(sLogger,
             ("local address", SockAddressToString(addressLocal))("pcap interface ip", SockAddressToString(addressNet))(
                 "mask", SockAddressToString(addressMask)));

    if (g_pcap_compile_func(mHandle, &mBPFFilter, mConfig->mPCAPFilter.c_str(), 0, netp) == PCAP_ERROR) {
        LOG_ERROR(sLogger,
                  ("init pcap wrapper when compile bpf filter error, err", mErrBuf)("filter", mConfig->mPCAPFilter));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM,
                                               "compile pcap bpf filter error, err: " + std::string(mErrBuf));
        return false;
    }
    LOG_INFO(sLogger, ("pcap compile bpf filter success, filter", mConfig->mPCAPFilter));
    if (g_pcap_setfilter_func(mHandle, &mBPFFilter) == PCAP_ERROR) {
        LOG_ERROR(sLogger, ("init pcap wrapper when bind bpf filter error, err", mErrBuf));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM,
                                               "bind pcap bpf filter error, err: " + std::string(mErrBuf));
        return false;
    }
    LOG_INFO(sLogger, ("init pcap", "success"));
    return true;
}

int32_t PCAPWrapper::ProcessPackets(int32_t maxProcessPackets, int32_t maxProcessDurationMs) {
    if (g_pcap_dispatch_func == NULL || g_pcap_geterr_func == NULL || mHandle == NULL) {
        return -2;
    }
    assert(mHandle != NULL);
    int32_t processedPackets = 0;
    for (; processedPackets < maxProcessPackets; processedPackets += INT32_FLAG(sls_observer_network_pcap_loop_count)) {
        int dispathRst = g_pcap_dispatch_func(
            mHandle, INT32_FLAG(sls_observer_network_pcap_loop_count), OnPCAPPacketsCallBack, (u_char*)this);
        if (dispathRst >= int(INT32_FLAG(sls_observer_network_pcap_loop_count))) {
            continue;
        }
        if (dispathRst >= 0) {
            // reach end
            LOG_DEBUG(sLogger, ("pcap dispath success, total", processedPackets + dispathRst)("last", dispathRst));
            return processedPackets + dispathRst;
        }
        LOG_WARNING(sLogger, ("pcap dispath error, code", dispathRst)("error", g_pcap_geterr_func(mHandle)));
        return -1;
    }
    return maxProcessPackets;
}
void PCAPWrapper::PCAPCallBack(const struct pcap_pkthdr* header, const u_char* packet) {
    assert(mPacketProcessor);
    /* First, lets make sure we have an IP packet */
    struct ether_header* eth_header;
    eth_header = (struct ether_header*)packet;
    if (ntohs(eth_header->ether_type) != ETHERTYPE_IP) {
        // printf("Not an IP packet. Skipping...\n\n");
        return;
    }

    /* The total packet length, including all headers
    and the data payload is stored in
    header->len and header->caplen. Caplen is
    the amount actually available, and len is the
    total packet length even if it is larger
    than what we currently have captured. If the snapshot
    length set with pcap_open_live() is too small, you may
    not have the whole packet. */
    // printf("Total packet available: %d bytes\n", header->caplen);
    // printf("Expected packet size: %d bytes\n", header->len);

    /* Pointers to start point of various headers */
    const u_char* ip_header = NULL;
    const u_char* payload = NULL;

    /* Header lengths in bytes */
    int ethernet_header_length = 14; /* Doesn't change */
    int ip_header_length = 0;
    int payload_length = 0; // valid payload length, calc use  pcap_pkthdr->caplen
    int payload_raw_length = 0; // raw payload length, calc use  pcap_pkthdr->len

    /* Find start of IP header */
    ip_header = packet + ethernet_header_length;

    struct iphdr* ipHeaderStruct = (struct iphdr*)ip_header;


    /* The second-half of the first byte in ip_header
    contains the IP header length (IHL). */
    ip_header_length = ipHeaderStruct->ihl;
    /* The IHL is number of 32-bit segments. Multiply
    by four to get a byte count for pointer arithmetic */
    ip_header_length = ip_header_length << 2;
    // printf("IP header length (IHL) in bytes: %d\n", ip_header_length);

    /* Now that we know where the IP header is, we can
    inspect the IP header for a protocol number to
    make sure it is TCP before going any further.
    Protocol is always the 10th byte of the IP header */
    u_char protocol = ipHeaderStruct->protocol;
    uint16_t srcPort = 0;
    uint16_t dstPort = 0;

    char packetBuffer[sizeof(PacketEventHeader) + sizeof(PacketEventData)] = {0};
    PacketEventHeader* eventHeader = (PacketEventHeader*)packetBuffer;
    // @todo
    bool retran = false;
    bool zeroWindow = false;
    if (protocol == IPPROTO_UDP) {
        if (ethernet_header_length + ip_header_length + sizeof(udphdr) >= header->caplen) {
            // invalid length
            return;
        }
        const u_char* udp_header = packet + ethernet_header_length + ip_header_length;
        struct udphdr* udpHeaderSturct = (struct udphdr*)udp_header;
        srcPort = udpHeaderSturct->source;
        dstPort = udpHeaderSturct->dest;
        uint64_t udpLength = ntohs(udpHeaderSturct->len); // include udphdr
        if (udpLength < sizeof(udphdr)) {
            return;
        }
        payload_raw_length = payload_length = udpLength - sizeof(udphdr);
        if (udpLength + ethernet_header_length + ip_header_length > header->caplen) {
            payload_length = header->caplen - ethernet_header_length + ip_header_length - sizeof(udphdr);
        }
        payload = udp_header + sizeof(udphdr);
    } else if (protocol == IPPROTO_TCP) {
        const u_char* tcp_header = NULL;
        int tcp_header_length = 0;
        /* Add the ethernet and ip header length to the start of the packet
        to find the beginning of the TCP header */
        tcp_header = packet + ethernet_header_length + ip_header_length;
        struct tcphdr* tcpHeaderSturct = (struct tcphdr*)tcp_header;
        if (ethernet_header_length + ip_header_length + sizeof(tcphdr) >= header->caplen) {
            // invalid length
            return;
        }
        /* TCP header length is stored in the first half
        of the 12th byte in the TCP header. Because we only want
        the value of the top half of the byte, we have to shift it
        down to the bottom half otherwise it is using the most
        significant bits instead of the least significant bits */
        tcp_header_length = tcpHeaderSturct->doff;
        /* The TCP header length stored in those 4 bits represents
        how many 32-bit words there are in the header, just like
        the IP header length. We multiply by four again to get a
        byte count. */
        tcp_header_length = tcp_header_length << 2;
        srcPort = tcpHeaderSturct->source;
        dstPort = tcpHeaderSturct->dest;
        // printf("TCP header length in bytes: %d\n", tcp_header_length);

        /* Add up all the header sizes to find the payload offset */
        int total_headers_size = ethernet_header_length + ip_header_length + tcp_header_length;
        // printf("Size of all headers combined: %d bytes\n", total_headers_size);
        payload_length = header->caplen - total_headers_size;
        payload_raw_length = header->len - total_headers_size;
        // printf("Payload size: %d bytes\n", payload_length);
        payload = packet + total_headers_size;
    } else {
        return;
    }
    if (payload_length <= 0 || payload_raw_length <= 0) {
        return;
    }
    eventHeader->DstAddr.Type = SockAddressType_IPV4;
    eventHeader->SrcAddr.Type = SockAddressType_IPV4;
    PacketType packetType = PacketType_In;
    if (ipHeaderStruct->saddr == mLocalAddress) {
        packetType = PacketType_Out;
    } else if (ipHeaderStruct->daddr == mLocalAddress) {
        packetType = PacketType_In;
    } else {
        char* srcAddr = (char*)&ipHeaderStruct->saddr;
        char* dstAddr = (char*)&ipHeaderStruct->daddr;
        char* localMaskAddr = (char*)&mLocalMaskAddress;
        for (int i = 0; i < 4; ++i) {
            if (srcAddr[i] == localMaskAddr[i] && dstAddr[i] != localMaskAddr[i]) {
                packetType = PacketType_Out;
                break;
            }
            if (srcAddr[i] != localMaskAddr[i] && dstAddr[i] == localMaskAddr[i]) {
                packetType = PacketType_In;
                break;
            }
        }
    }

    if (packetType == PacketType_Out) {
        eventHeader->DstAddr.Addr.IPV4 = ipHeaderStruct->daddr;
        eventHeader->SrcAddr.Addr.IPV4 = ipHeaderStruct->saddr;
        eventHeader->SrcPort = ntohs(srcPort);
        eventHeader->DstPort = ntohs(dstPort);
    } else {
        eventHeader->DstAddr.Addr.IPV4 = ipHeaderStruct->saddr;
        eventHeader->SrcAddr.Addr.IPV4 = ipHeaderStruct->daddr;
        eventHeader->SrcPort = ntohs(dstPort);
        eventHeader->DstPort = ntohs(srcPort);
    }
    // filter host process connections
    if (mConfig->mDropLocalConnections
        && (eventHeader->DstAddr.Addr.IPV4 == htonl(INADDR_LOOPBACK) || eventHeader->DstAddr.Addr.IPV4 == INADDR_ANY)
        && (eventHeader->SrcAddr.Addr.IPV4 == htonl(INADDR_LOOPBACK) || eventHeader->SrcAddr.Addr.IPV4 == INADDR_ANY)) {
        return;
    }
    eventHeader->SockHash
        = XXH32((void*)(&eventHeader->SrcAddr), (char*)(&eventHeader->DstPort) - (char*)(&eventHeader->SrcAddr) + 2, 0);
    eventHeader->EventType = PacketEventType_Data;
    eventHeader->PID = 0;
    eventHeader->TimeNano = uint64_t(header->ts.tv_sec) * 1000000000LL + header->ts.tv_usec * 1000LL;
    PacketEventData* eventData = (PacketEventData*)(packetBuffer + sizeof(PacketEventHeader));
    eventData->Buffer = (char*)payload;
    eventData->BufferLen = payload_length;
    eventData->RealLen = payload_raw_length;
    eventData->PktType = packetType;

    auto res = caches.Get(eventHeader->SockHash, nullptr);
    if (res == nullptr) {
        std::tuple<ProtocolType, MessageType> inferRst
            = infer_protocol(eventHeader, packetType, (char*)payload, payload_length, payload_raw_length);
        eventData->PtlType = std::get<0>(inferRst);
        eventData->MsgType = std::get<1>(inferRst);
        eventHeader->RoleType = InferServerOrClient(packetType, eventData->MsgType);
        if (eventHeader->RoleType != PacketRoleType::Unknown) {
            caches.Put(
                eventHeader->SockHash, std::move(std::make_pair(eventHeader->RoleType, eventData->PtlType)), nullptr);
        }
        LOG_DEBUG(sLogger,
                  ("receive data event:new conn, addr",
                   SockAddressToString(eventHeader->DstAddr))("role", PacketRoleTypeToString(eventHeader->RoleType))(
                      "port", eventHeader->DstPort)("protocol", eventData->PtlType)(
                      "msg", MessageTypeToString(eventData->MsgType))("hash", eventHeader->SockHash));
        LOG_TRACE(sLogger, ("data", charToHexString(eventData->Buffer, eventData->RealLen, eventData->RealLen)));
    } else {
        eventData->PtlType = res->second;
        eventHeader->RoleType = res->first;
        if (eventData->PktType == PacketType_In) {
            eventData->MsgType
                = eventHeader->RoleType == PacketRoleType::Client ? MessageType_Response : MessageType_Request;
        } else {
            eventData->MsgType
                = eventHeader->RoleType == PacketRoleType::Client ? MessageType_Request : MessageType_Response;
        }
        LOG_DEBUG(sLogger,
                  ("receive data event:history conn, addr",
                   SockAddressToString(eventHeader->DstAddr))("role", PacketRoleTypeToString(eventHeader->RoleType))(
                      "port", eventHeader->DstPort)("protocol", eventData->PtlType)(
                      "msg", MessageTypeToString(eventData->MsgType))("hash", eventHeader->SockHash));
        LOG_TRACE(sLogger, ("data", charToHexString(eventData->Buffer, eventData->RealLen, eventData->RealLen)));
    }
    LOG_DEBUG(sLogger, ("tag3", "==="));

    logtail::NetStatisticsKey key;
    key.PID = eventHeader->PID;
    key.SockHash = eventHeader->SockHash;
    key.AddrInfo.RemoteAddr = eventHeader->DstAddr;
    key.AddrInfo.RemotePort = eventHeader->DstPort;
    key.RoleType = eventHeader->RoleType;
    key.SockCategory = SocketCategory::InetSocket;
    bool needRebuildHash = false;
    if (eventHeader->RoleType == PacketRoleType::Server) {
        key.AddrInfo.RemotePort = 0;
        needRebuildHash = true;
    }
    key.AddrInfo.LocalAddr = eventHeader->SrcAddr;
    key.AddrInfo.LocalPort = eventHeader->SrcPort;
    if (eventHeader->RoleType == PacketRoleType::Client) {
        key.AddrInfo.LocalPort = 0;
        needRebuildHash = true;
    }
    if (needRebuildHash) {
        key.SockHash = 0;
        key.SockHash = XXH32(&key, sizeof(NetStatisticsKey), 0);
    }

    NetStatisticsTCP& statisticsItem = mStatistics.GetStatisticsItem(key);

    if (packetType == PacketType_Out) {
        ++statisticsItem.Base.SendPackets;
        statisticsItem.Base.SendBytes += payload_raw_length;
        if (retran) {
            ++statisticsItem.SendRetranCount;
        }
        if (zeroWindow) {
            ++statisticsItem.SendZeroWinCount;
        }
    } else {
        ++statisticsItem.Base.RecvPackets;
        statisticsItem.Base.RecvBytes += payload_raw_length;
        if (retran) {
            ++statisticsItem.RecvRetranCount;
        }
        if (zeroWindow) {
            ++statisticsItem.RecvZeroWinCount;
        }
    }
    if (mConfig->IsLegalProtocol(eventData->PtlType)) {
        mPacketProcessor(StringPiece(packetBuffer, sizeof(PacketEventHeader) + sizeof(PacketEventData)));
    }


    // printf("Memory address where payload begins: %p\n\n", payload);

    /* Print payload in ASCII */
    /*
    if (payload_length > 0) {
        const u_char *temp_pointer = payload;
        int byte_count = 0;
        while (byte_count++ < payload_length) {
            printf("%c", *temp_pointer);
            temp_pointer++;
        }
        printf("\n");
    }
    */

    return;
}
}; // namespace logtail