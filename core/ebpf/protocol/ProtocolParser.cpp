#include "ProtocolParser.h"

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "common/magic_enum.hpp"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

std::set<ProtocolType> ProtocolParserManager::AvaliableProtocolTypes() const {
    return {ProtocolType::HTTP};
}

ProtocolType ProtocolStringToEnum(std::string protocol) {
    std::transform(protocol.begin(), protocol.end(), protocol.begin(), [](unsigned char c) { return std::toupper(c); });
    auto pro = magic_enum::enum_cast<ProtocolType>(protocol).value_or(ProtocolType::UNKNOWN);
    return pro;
}

bool ProtocolParserManager::AddParser(const std::string& protocol) {
    auto pro = ProtocolStringToEnum(protocol);
    if (pro == ProtocolType::UNKNOWN) {
        return false;
    }
    return AddParser(pro);
}

bool ProtocolParserManager::RemoveParser(const std::string& protocol) {
    auto pro = ProtocolStringToEnum(protocol);
    if (pro == ProtocolType::UNKNOWN) {
        return false;
    }
    return RemoveParser(pro);
}

bool ProtocolParserManager::AddParser(ProtocolType type) {
    if (!AvaliableProtocolTypes().count(type)) {
        LOG_WARNING(sLogger, ("protocol not supported", magic_enum::enum_name(type)));
        return false;
    }
    WriteLock lock(mLock);
    auto parser = ProtocolParserRegistry::instance().createParser(type);
    if (parser) {
        LOG_DEBUG(sLogger, ("add protocol parser", std::string(magic_enum::enum_name(type))));
        mParsers[type] = std::move(parser);
        return true;
    } else {
        LOG_ERROR(sLogger, ("No parser available for type ", magic_enum::enum_name(type)));
    }
    return false;
}

bool ProtocolParserManager::RemoveParser(ProtocolType type) {
    WriteLock lock(mLock);
    if (mParsers.count(type)) {
        LOG_DEBUG(sLogger, ("remove protocol parser", std::string(magic_enum::enum_name(type))));
        mParsers.erase(type);
    } else {
        LOG_INFO(sLogger, ("No parser for type ", magic_enum::enum_name(type)));
    }

    return true;
}


std::vector<std::unique_ptr<AbstractRecord>> ProtocolParserManager::Parse(ProtocolType type,
                                                                          std::unique_ptr<NetDataEvent> data) {
    ReadLock lock(mLock);
    if (mParsers.find(type) != mParsers.end()) {
        return mParsers[type]->Parse(std::move(data));
    } else {
        LOG_ERROR(sLogger, ("No parser found for given protocol type", std::string(magic_enum::enum_name(type))));
        return std::vector<std::unique_ptr<AbstractRecord>>();
    }
}

} // namespace ebpf
} // namespace logtail
