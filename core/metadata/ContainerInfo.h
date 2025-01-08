#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <ctime>

namespace logtail {

struct k8sContainerInfo {
    std::unordered_map<std::string, std::string> images;
    std::unordered_map<std::string, std::string> labels;
    std::string k8sNamespace;
    std::string serviceName;
    std::string workloadKind;
    std::string workloadName;
    // ??? 
    std::time_t timestamp;
    std::string appId;
    std::string appName;
    std::string podIp;
    std::string podName;
    int64_t startTime;
    std::vector<std::string> containerIds;
};

}
