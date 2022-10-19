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
    auto content = log->add_contents();
    content->set_key(logtail::observer::kSendBytes);
    content->set_value(std::to_string(this->SendBytes));
    content = log->add_contents();
    content->set_key(logtail::observer::kRecvBytes);
    content->set_value(std::to_string(this->RecvBytes));

    content = log->add_contents();
    content->set_key(logtail::observer::kSendpackets);
    content->set_value(std::to_string(this->SendPackets));
    content = log->add_contents();
    content->set_key(logtail::observer::kRecvPackets);
    content->set_value(std::to_string(this->RecvPackets));
}


void NetStatisticsKey::ToPB(sls_logs::Log* log) const {
    static ServiceMetaManager* sHostnameManager = logtail::ServiceMetaManager::GetInstance();
    ConnectionAddrInfoToPB(this->AddrInfo, log);
    sls_logs::Log_Content* content = log->add_contents();
    content->set_key(logtail::observer::kRemoteInfo);
    const std::string& remoteAddr = SockAddressToString(this->AddrInfo.RemoteAddr);
    const ServiceMeta& meta = sHostnameManager->GetServiceMeta(this->PID, remoteAddr);
    content->set_value(
        std::string(kRemoteInfoPrefix).append(meta.Empty() ? remoteAddr : meta.Host).append(kRemoteInfoSuffix));

    content = log->add_contents();
    content->set_key(logtail::observer::kRole);
    content->set_value(PacketRoleTypeToString(this->RoleType));


    content = log->add_contents();
    content->set_key(logtail::observer::kConnId);
    content->set_value(std::to_string(GenConnectionID(this->PID, this->SockHash)));

    // todo replace with real type
    content = log->add_contents();
    content->set_key(logtail::observer::kConnType);
    content->set_value("tcp");
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
