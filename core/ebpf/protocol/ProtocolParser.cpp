#include <vector>
#include <memory>
#include <unordered_map>

#include "ProtocolParser.h"
#include "logger/Logger.h"
#include "common/magic_enum.hpp"

namespace logtail {
namespace ebpf {

bool ProtocolParserManager::AddParser(ProtocolType type) {
    auto parser = ProtocolParserRegistry::instance().createParser(type);
    if (parser) {
        parsers_[type] = std::move(parser);
        return true;
    } else {
        LOG_ERROR(sLogger, ("No parser available for type ", magic_enum::enum_name(type)));
    }
    return false;
}

bool ProtocolParserManager::RemoveParser(ProtocolType type) {
    auto parser = parsers_[type];
    if (!parser) {
        LOG_ERROR(sLogger, ("No parser for type ", magic_enum::enum_name(type)));
        return true;
    }

    parsers_[type].reset();
    return true;
}


std::vector<std::unique_ptr<AbstractRecord>> ProtocolParserManager::Parse(ProtocolType type, std::unique_ptr<NetDataEvent> data) {
    if (parsers_.find(type) != parsers_.end()) {
    return parsers_[type]->Parse(std::move(data));
    } else {
        LOG_ERROR(sLogger, ("No parser found for given protocol type", std::string(magic_enum::enum_name(type))));
        return std::vector<std::unique_ptr<AbstractRecord>>();
    }
}

}
}
