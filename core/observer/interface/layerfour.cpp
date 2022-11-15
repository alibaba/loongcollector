//
// Created by evan on 2022/10/17.
//

#include "layerfour.h"
#include "helper.h"
#include "MachineInfoUtil.h"

namespace logtail {

void NetStatisticsTCP::ToPB(sls_logs::Log* log) const {
    this->Base.ToPB(log);
}

void NetStatisticsBase::ToPB(sls_logs::Log* log) const {
    AddAnyLogContent(log, logtail::observer::kSendBytes, this->SendBytes);
    AddAnyLogContent(log, logtail::observer::kRecvBytes, this->RecvBytes);
    AddAnyLogContent(log, logtail::observer::kSendpackets, this->SendPackets);
    AddAnyLogContent(log, logtail::observer::kRecvPackets, this->RecvPackets);
}


void NetStatisticsKey::ToPB(sls_logs::Log* log) const {
    static ServiceMetaManager* sHostnameManager = logtail::ServiceMetaManager::GetInstance();
    ConnectionAddrInfoToPB(this->AddrInfo, log);
    const std::string& remoteAddr = SockAddressToString(this->AddrInfo.RemoteAddr);
    const ServiceMeta& meta = sHostnameManager->GetServiceMeta(this->PID, remoteAddr);
    auto remoteInfo
        = std::string(kRemoteInfoPrefix).append(meta.Empty() ? remoteAddr : meta.Host).append(kRemoteInfoSuffix);
    AddAnyLogContent(log, logtail::observer::kRemoteInfo, std::move(remoteInfo));
    AddAnyLogContent(log, logtail::observer::kRole, PacketRoleTypeToString(this->RoleType));
    AddAnyLogContent(log, logtail::observer::kConnId, GenConnectionID(this->PID, this->SockHash));
    AddAnyLogContent(log, logtail::observer::kConnType, std::string("tcp")); // todo replace with real type
    AddAnyLogContent(log, observer::kType, ObserverMetricsTypeToString(ObserverMetricsType::L4_METRICS));
}

logtail::NetStatisticsTCP& NetStaticticsMap::GetStatisticsItem(const logtail::NetStatisticsKey& key) {
    auto findRst = mHashMap.find(key);
    if (findRst != mHashMap.end()) {
        return findRst->second;
    }
    static NetStatisticsTCP sNewTcp;
    auto insertIter = mHashMap.insert(std::make_pair(key, sNewTcp));
    return insertIter.first->second;
}

} // namespace logtail
