#include <vector>
#include <memory>
#include <unordered_map>

#include "ProtocolParser.h"
#include "logger/Logger.h"
#include "common/magic_enum.hpp"

namespace logtail {
namespace ebpf {

void ProtocolParserManager::Init() {
    for (auto type : {ProtocolType::HTTP, ProtocolType::DNS, ProtocolType::KAFKA}) {
    auto parser = ProtocolParserRegistry::instance().createParser(type);
    if (parser) {
        parsers_[type] = std::move(parser);
    } else {
        LOG_WARNING(sLogger, ("No parser available for type ", static_cast<int>(type)));
    }
    }
}

std::vector<std::unique_ptr<AbstractRecord>> ProtocolParserManager::Parse(ProtocolType type, std::unique_ptr<NetDataEvent> data) {
    if (parsers_.find(type) != parsers_.end()) {
    return parsers_[type]->Parse(std::move(data));
    // do coverge
    // do aggregate
    } else {
        LOG_WARNING(sLogger, ("No parser found for given protocol type", std::string(magic_enum::enum_name(type))));
        return std::vector<std::unique_ptr<AbstractRecord>>();
    }
}

}
}
