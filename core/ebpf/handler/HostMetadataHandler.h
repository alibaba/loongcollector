// // Copyright 2023 iLogtail Authors
// //
// // Licensed under the Apache License, Version 2.0 (the "License");
// // you may not use this file except in compliance with the License.
// // You may obtain a copy of the License at
// //
// //     http://www.apache.org/licenses/LICENSE-2.0
// //
// // Unless required by applicable law or agreed to in writing, software
// // distributed under the License is distributed on an "AS IS" BASIS,
// // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// // See the License for the specific language governing permissions and
// // limitations under the License.

// #pragma once

// #include "ebpf/Config.h"
// #include "ebpf/handler/AbstractHandler.h"
// #include "ebpf/include/export.h"

// namespace logtail {
// namespace ebpf {


// class SimplePodInfo {
// public:
//     SimplePodInfo(uint64_t startTime,
//                   const std::string& appId,
//                   const std::string& appName,
//                   const std::string& podIp,
//                   const std::string& podName,
//                   std::vector<std::string>& cids)
//         : mStartTime(startTime), mAppId(appId), mAppName(appName), mPodIp(podIp), mPodName(podName) {}

//     uint64_t mStartTime;
//     std::string mAppId;
//     std::string mAppName;
//     std::string mPodIp;
//     std::string mPodName;
//     std::vector<std::string> mContainerIds;
// };

// class HostMetadataHandler : public AbstractHandler {
// public:
//     using UpdatePluginCallbackFunc = std::function<bool(
//         PluginType, /*UpdataType updateType, */ const std::variant<SecurityOptions*, ObserverNetworkOption*>)>;
//     HostMetadataHandler(const CollectionPipelineContext* ctx, QueueKey key, uint32_t idx, int intervalSec = 60);
//     ~HostMetadataHandler();
//     void RegisterUpdatePluginCallback(UpdatePluginCallbackFunc&& fn) { mUpdateFunc = fn; }
//     void DegisterUpdatePluginCallback() { mUpdateFunc = nullptr; }
//     bool handle(uint32_t pluginIndex, std::vector<std::string>& podIpVec);
//     void ReportAgentInfo();

// private:
//     // key is podIp, value is cids
//     std::unordered_map<std::string, std::unique_ptr<SimplePodInfo>> mHostPods;
//     std::unordered_set<std::string> mCids;
//     std::thread mReporter;
//     std::atomic_bool mFlag;
//     ReadWriteLock mLock;
//     int mIntervalSec;
//     UpdatePluginCallbackFunc mUpdateFunc = nullptr;
// };

// } // namespace ebpf
// } // namespace logtail
