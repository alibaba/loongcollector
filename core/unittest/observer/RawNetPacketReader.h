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

#include <vector>
#include <string>
#include "observer/network/NetworkObserver.h"
#include "observer/interface/helper.h"
#include "observer/network/protocols/utils.h"
#include "HashUtil.h"
#include <iostream>
#include <arpa/inet.h>

namespace logtail {

enum L4ProtoType { L4ProtoUDP, L4ProtoTCP };

class RawNetPacketReader {
private:
    bool parseSuccess = true;
    std::string parserFailMsg;
    std::string mLocalAddress;
    bool mIsServer;
    ProtocolType mProtocolType;
    std::vector<std::string> rawHexs;

public:
    RawNetPacketReader(std::string mLocalAddress,
                       bool mIsServer,
                       ProtocolType mProtocolType,
                       std::vector<std::string> rawHexs)
        : mLocalAddress(mLocalAddress), mIsServer(mIsServer), mProtocolType(mProtocolType), rawHexs(rawHexs) {}

    std::string& GetParseFailMsg() { return parserFailMsg; }

    bool OK() { return parseSuccess; }

    void GetAllNetPackets(std::vector<std::string>& packets) {
        for (auto& rawHex : rawHexs) {
            std::vector<uint8_t> rawData;
            hexstring_to_bin(rawHex, rawData);
            const char* rawPkt = (const char*)rawData.data();
            size_t rawPktSize = rawData.size();
            parse(rawPkt, rawPktSize, packets);
        }
    }

    void assemble(const char* payload,
                  const int32_t dataLen,
                  uint16_t srcPort,
                  uint16_t dstPort,
                  SockAddress& srcIP,
                  SockAddress& dstIP,
                  std::vector<std::string>& packets) {
        // assemble data
        std::string packetData;
        // int32_t dataLen = udpPayloadLength;
        packetData.resize(dataLen + sizeof(PacketEventHeader) + sizeof(PacketEventData));
        char* beginData = &packetData.at(0);

        PacketEventHeader* header = (PacketEventHeader*)beginData;
        PacketEventData* data = (PacketEventData*)(beginData + sizeof(PacketEventHeader));
        std::string src = SockAddressToString(srcIP);
        std::string dst = SockAddressToString(dstIP);
        data->Buffer = beginData + sizeof(PacketEventHeader) + sizeof(PacketEventData);

        // check packet direction
        if (src == mLocalAddress) {
            data->PktType = PacketType_Out;
            if (mIsServer) {
                data->MsgType = MessageType_Response;
            } else {
                data->MsgType = MessageType_Request;
            }
        } else {
            data->PktType = PacketType_In;
            std::swap(src, dst);
            std::swap(srcPort, dstPort);
            if (mIsServer) {
                data->MsgType = MessageType_Request;
            } else {
                data->MsgType = MessageType_Response;
            }
        }

        header->PID = 0;
        header->EventType = PacketEventType_Data;
        header->TimeNano = GetCurrentTimeInNanoSeconds();
        header->SockHash = uint32_t(HashString(src + std::to_string(srcPort) + dst + std::to_string(dstPort)));

        header->SrcAddr = SockAddressFromString(src);
        header->DstAddr = SockAddressFromString(dst);

        header->DstPort = dstPort;
        header->SrcPort = srcPort;
        header->RoleType = PacketRoleType::Server;

        data->BufferLen = dataLen;
        data->RealLen = dataLen;
        char* dataBuffer = data->Buffer;
        for (int i = 0; i < dataLen; i++) {
            dataBuffer[i] = payload[i];
        }

        data->PtlType = mProtocolType;
        packets.push_back(packetData);
        // fix data buffers
        std::string& lastPacketData = packets.back();
        char* bData = &lastPacketData.at(0);
        PacketEventData* pData = (PacketEventData*)(bData + sizeof(PacketEventHeader));
        pData->Buffer = bData + sizeof(PacketEventHeader) + sizeof(PacketEventData);
    }

    void parse(const char* rawPkt, const size_t rawPktSize, std::vector<std::string>& packets) {
        uint32_t position = 0;
        // read ethernet layer
        if (rawPktSize < 15) { // 以太网+ip的第一个字段
            parserFailMsg = "bad ethernet packet length";
            parseSuccess = false;
            return;
        }
        position = 14;

        // read ip layer
        uint8_t* ipheader = (uint8_t*)(rawPkt + position);

        if ((ipheader[0] >> 4) != 4) {
            parserFailMsg = "not ipv4 packet";
            parseSuccess = false;
            return;
        }

        size_t ipheaderLen = (ipheader[0] & 0x0f) * 4; // 32bit 一个单位，即 4个字节一个计量
        if (ipheaderLen < 20 || (position + ipheaderLen) > rawPktSize) {
            parserFailMsg = "bad ip packet length";
            parseSuccess = false;
            return;
        }

        uint8_t protocolCode = ipheader[9]; // protocol
        SockAddress srcIP;
        srcIP.Type = SockAddressType_IPV4;
        srcIP.Addr.IPV4 = *((uint32_t*)(ipheader + 12));
        std::cout << SockAddressToString(srcIP) << std::endl;

        SockAddress dstIP;
        dstIP.Type = SockAddressType_IPV4;
        dstIP.Addr.IPV4 = *((uint32_t*)(ipheader + 16));
        std::cout << SockAddressToString(dstIP) << std::endl;

        if (protocolCode == 17) {
            // udp packet
            position += ipheaderLen;
            if ((position + 8) > rawPktSize) {
                parserFailMsg = "bad udp packet length";
                parseSuccess = false;
                return;
            }
            uint16_t* udpHeader = (uint16_t*)(rawPkt + position);

            uint16_t srcPort = ntohs(udpHeader[0]);
            uint16_t dstPort = ntohs(udpHeader[1]);

            uint16_t udpTotalLength = ntohs(udpHeader[2]);

            uint16_t udpPayloadLength = udpTotalLength - 8; // UDP 总长度减去 Header固定长度8
            // std::cout << udpPayloadLength << std::endl;

            position += 8;

            if ((position + udpPayloadLength) > rawPktSize) {
                parserFailMsg = "corrupt udp payload";
                parseSuccess = false;
                return;
            }

            const char* udpPayload = (char*)(rawPkt + position);

            assemble(udpPayload, udpPayloadLength, srcPort, dstPort, srcIP, dstIP, packets);
        } else if (protocolCode == 6) {
            // TCP Packet
            position += ipheaderLen;
            if ((position + 20) > rawPktSize) { // tcp报文头至少20个字节
                parserFailMsg = "bad tcp packet length";
                parseSuccess = false;
                return;
            }
            uint16_t* tcpHeader = (uint16_t*)(rawPkt + position);

            uint16_t srcPort = ntohs(tcpHeader[0]);
            uint16_t dstPort = ntohs(tcpHeader[1]);

            uint8_t tcpHeaderLen = (ntohs(tcpHeader[6]) >> 12) * 4;

            position += tcpHeaderLen;
            const char* payload = (char*)(rawPkt + position);
            int32_t payloadLen = rawPktSize - position;

            // std::cout << "srcPort: " << std::to_string(srcPort) << " dstPort: " << std::to_string(dstPort)<<
            // std::endl;
            assemble(payload, payloadLen, srcPort, dstPort, srcIP, dstIP, packets);

        } else {
            parserFailMsg = "bad tcp packet length";
            parseSuccess = false;
            return;
        }
    }
};
} // namespace logtail