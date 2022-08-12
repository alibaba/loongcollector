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

#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include <fstream>
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/error/en.h"
#include "observer/network/NetworkObserver.h"
#include "observer/interface/helper.h"
#include <arpa/inet.h>

namespace logtail {


class JsonNetPacketReader {
public:
    JsonNetPacketReader(const std::string& filePath,
                        const std::string& localAddress,
                        bool isServer,
                        ProtocolType type) {
        std::ifstream ifs(filePath);
        rapidjson::IStreamWrapper isw(ifs);
        mParseRst = mDoc.ParseStream(isw);
        mLocalAddress = localAddress;
        mIsServer = isServer;
        mProtocol = type;
    }

    bool OK() { return !mDoc.HasParseError(); }

    std::string Error() { return rapidjson::GetParseError_En(mParseRst.Code()); }

    uint8_t hexToValue(const char val) {
        if (val >= '0' && val <= '9') {
            return val - '0';
        }
        if (val >= 'A' && val <= 'F') {
            return 10 + (val - 'A');
        }
        if (val >= 'a' && val <= 'f') {
            return 10 + (val - 'a');
        }
        return 0;
    }

    char hexToChar(const char* data) { return (hexToValue(data[0]) << 4) | hexToValue(data[1]); }

    /**
     * @brief
     *51:31:79:53:70:58:4a:44:4c:69:36:48:4a:48:30:6b:47:66:4d:50:77:57:47:71:51:3d:22:2c:22:72:73:61:5f:6b:65:79:5f:76:65:72:73:69:6f:6e:22:3a:22:33:22:2c:22:61:65:73:5f:6b:65:79:5f:76:65:72:73:69:6f:6e:22:3a:22:33:22:7d:7d"
     * @param payload
     * @param dataBuffer
     * @return true
     * @return false
     */
    bool ParseData(const std::string& payload, char* dataBuffer) {
        size_t lastIndex = 0;
        size_t sep = 0;
        size_t i = 0;
        while (true) {
            sep = payload.find(':', lastIndex);
            if (sep == std::string::npos) {
                dataBuffer[i++] = hexToChar(&payload.at(lastIndex));
                break;
            }

            dataBuffer[i++] = hexToChar(&payload.at(lastIndex));
            lastIndex += 3;
        }
        if (i == (payload.size() / 3 + 1)) {
            return true;
        }
        return false;
    }

    void GetAllNetPackets(std::vector<std::string>& packets) {
        for (auto iter = mDoc.Begin(); iter != mDoc.End(); ++iter) {
            rapidjson::Value& layersValue = iter->operator[]("_source")["layers"];
            if (!layersValue.HasMember("ip")) {
                continue;
            }
            rapidjson::Value& ipValue = layersValue["ip"];
            std::string src(ipValue["ip.src"].GetString(), ipValue["ip.src"].GetStringLength());
            std::string dst(ipValue["ip.dst"].GetString(), ipValue["ip.dst"].GetStringLength());

            if (layersValue.HasMember("tcp") && layersValue["tcp"].HasMember("tcp.payload")) {
                rapidjson::Value& tcpValue = layersValue["tcp"];
                std::string payload(tcpValue["tcp.payload"].GetString(), tcpValue["tcp.payload"].GetStringLength());
                std::string srcPort(tcpValue["tcp.srcport"].GetString(), tcpValue["tcp.srcport"].GetStringLength());
                std::string dstPort(tcpValue["tcp.dstport"].GetString(), tcpValue["tcp.dstport"].GetStringLength());

                int32_t dataLen = int32_t(payload.size() / 3 + 1);

                std::string packetData;
                packetData.resize(dataLen + sizeof(PacketEventHeader) + sizeof(PacketEventData));
                char* beginData = &packetData.at(0);

                PacketEventHeader* header = (PacketEventHeader*)beginData;
                PacketEventData* data = (PacketEventData*)(beginData + sizeof(PacketEventHeader));
                data->Buffer = beginData + sizeof(PacketEventHeader) + sizeof(PacketEventData);
                char* dataBuffer = data->Buffer;


                data->PtlType = mProtocol;
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


                header->EventType = PacketEventType_Data;
                header->PID = 0;
                header->SockHash = uint32_t(HashString(src + srcPort + dst + dstPort));


                header->SrcAddr = SockAddressFromString(src);


                header->SrcPort = atoi(srcPort.c_str());

                header->DstAddr = SockAddressFromString(dst);
                header->DstPort = atoi(dstPort.c_str());

                data->BufferLen = dataLen;
                data->RealLen = dataLen;

                ParseData(payload, dataBuffer);

                header->TimeNano = GetCurrentTimeInNanoSeconds();


                // {
                //     const char * rData = packetData.c_str();
                //     char * bData = &packetData.at(0);
                //     PacketEventData * pData = (PacketEventData *)(bData + sizeof(PacketEventHeader));
                //     printf("%p %p %p %p %d %d \n", data->Buffer, rData, bData, pData->Buffer, pData->Buffer - bData,
                //     pData->Buffer - rData);
                // }

                packets.push_back(packetData);
                // fix data buffers
                std::string& lastPacketData = packets.back();
                {
                    // const char * rData = lastPacketData.c_str();
                    char* bData = &lastPacketData.at(0);
                    PacketEventData* pData = (PacketEventData*)(bData + sizeof(PacketEventHeader));
                    pData->Buffer = bData + sizeof(PacketEventHeader) + sizeof(PacketEventData);
                    // printf("%p %p %p %p %d %d \n", data->Buffer, rData, bData, pData->Buffer, pData->Buffer - bData,
                    // pData->Buffer - rData);
                }
            }
        }
    }

private:
    rapidjson::Document mDoc;
    rapidjson::ParseResult mParseRst;
    std::string mLocalAddress;
    ProtocolType mProtocol;
    bool mIsServer;
};


} // namespace logtail