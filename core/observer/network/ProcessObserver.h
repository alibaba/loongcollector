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

#include "interface/network.h"
#include "ConnectionObserver.h"
#include <unordered_map>
#include "network/protocols/ProtocolEventAggregators.h"
#include <vector>
#include "metas/ProcessMeta.h"
#include "metas/ContainerProcessGroup.h"
#include "NetworkConfig.h"
#include "NetworkObserver.h"


namespace logtail {
class ProcessObserver {
public:
    explicit ProcessObserver(uint64_t time);

    ~ProcessObserver() {
        for (auto iter = mAllConnections.begin(); iter != mAllConnections.end(); ++iter) {
            delete iter->second;
        }
    }

    bool HasConnection(uint32_t sockHash) const { return mAllConnections.find(sockHash) != mAllConnections.end(); }

    ConnectionObserver* GetOrCreateConnection(PacketEventHeader* header) {
        auto findIter = mAllConnections.find(header->SockHash);
        if (findIter != mAllConnections.end()) {
            return findIter->second;
        }
        ConnectionObserver* newConn = new ConnectionObserver(header, *mAllAggregator);
        mAllConnections.insert(std::make_pair(header->SockHash, newConn));
        return newConn;
    }

    void OnData(PacketEventHeader* header, PacketEventData* data) {
        auto conn = GetOrCreateConnection(header);
        mLastDataTimeNs = header->TimeNano;
        conn->OnData(header, data);
    }

    void ConnectionMarkDeleted(PacketEventHeader* header) {
        auto findIter = mAllConnections.find(header->SockHash);
        if (findIter != mAllConnections.end()) {
            findIter->second->MarkDeleted();
        }
    }

    void MarkDeleted() { mMarkDeleted = true; }

    const ProcessMetaPtr& GetProcessMeta() const { return mAllAggregator->GetProcessMeta(); }

    ProtocolEventAggregators* GetAggregator() { return mAllAggregator; }

    void SetProcessGroup(ContainerProcessGroupPtr& groupPtr) {
        mProcessGroupPtr = groupPtr;
        mAllAggregator = &mProcessGroupPtr->mAggregator;
    }

    /**
     * @brief GarbageCollection
     * @param size_limit_bytes
     * @param nowTimeNs
     * @return if we need delete this ProcessObserver
     */
    bool GarbageCollection(size_t size_limit_bytes, uint64_t nowTimeNs);

protected:
    std::unordered_map<uint32_t, ConnectionObserver*> mAllConnections;
    uint64_t mLastDataTimeNs = 0;
    ContainerProcessGroupPtr mProcessGroupPtr;
    ProtocolEventAggregators* mAllAggregator = NULL;
    bool mMarkDeleted = false;
    friend class NetworkObserverUnittest;
};

} // namespace logtail
