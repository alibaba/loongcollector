#pragma once
#include <ctime>

#include <string>
#include <unordered_map>
#include <vector>

namespace logtail {

struct K8sPodInfo {
    std::unordered_map<std::string, std::string> mImages;
    std::unordered_map<std::string, std::string> mLabels;
    std::string mNamespace;
    std::string mServiceName;
    std::string mWorkloadKind;
    std::string mWorkloadName;
    std::time_t mTimestamp;
    std::string mAppId;
    std::string mAppName;
    std::string mPodIp;
    std::string mPodName;
    int64_t mStartTime;
    std::vector<std::string> mContainerIds;
};

} // namespace logtail
