#pragma once

#include <unordered_map>

#include "logger/Logger.h"

using namespace std;

namespace logtail::prom {
class StreamCounter {
public:
    void Add(const std::string& streamID) { mStreamCache[streamID].first++; }
    void SetTotal(const std::string& streamID, uint64_t total) { mStreamCache[streamID].second = total; }
    bool IsLast(const std::string& streamID) {
        auto stream = mStreamCache.find(streamID);
        if (stream == mStreamCache.end()) {
            LOG_WARNING(sLogger, ("streamID not found", streamID));
            return false;
        }
        return stream->second.first == stream->second.second;
    }
    void Erase(const std::string& streamID) { mStreamCache.erase(streamID); }

private:
    // std::pair<streamCount,streamTotal>
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> mStreamCache;
};
} // namespace logtail::prom
