/*
 * Copyright 2024 iLogtail Authors
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

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "pipeline/queue/SenderQueueItem.h"
#include "plugin/flusher/sls/SLSResponse.h"

namespace logtail {

class SLSClientManager {
public:
    enum class AuthType { ANONYMOUS, AK };

    virtual ~SLSClientManager() = default;
    SLSClientManager(const SLSClientManager&) = delete;
    SLSClientManager& operator=(const SLSClientManager&) = delete;

    static SLSClientManager* GetInstance();

    virtual void Init();
    virtual void Stop() {};

    const std::string& GetUserAgent() const { return mUserAgent; }

    virtual bool
    GetAccessKey(const std::string& aliuid, AuthType& type, std::string& accessKeyId, std::string& accessKeySecret);

    virtual bool UsingHttps(const std::string& region) const { return true; }

protected:
    SLSClientManager() = default;

    virtual std::string GetRunningEnvironment();
    bool PingEndpoint(const std::string& host, const std::string& path);

    std::string mUserAgent;

private:
    virtual void GenerateUserAgent();

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SLSClientManagerUnittest;
#endif
};

void PreparePostLogStoreLogsRequest(const std::string& accessKeyId,
                                    const std::string& accessKeySecret,
                                    SLSClientManager::AuthType type,
                                    const std::string& host,
                                    bool isHostIp,
                                    const std::string& project,
                                    const std::string& logstore,
                                    const std::string& compressType,
                                    RawDataType dataType,
                                    const std::string& body,
                                    size_t rawSize,
                                    const std::string& shardHashKey,
                                    std::optional<uint64_t> seqId,
                                    std::string& path,
                                    std::string& query,
                                    std::map<std::string, std::string>& header);
void PreparePostMetricStoreLogsRequest(const std::string& accessKeyId,
                                       const std::string& accessKeySecret,
                                       SLSClientManager::AuthType type,
                                       const std::string& host,
                                       bool isHostIp,
                                       const std::string& project,
                                       const std::string& logstore,
                                       const std::string& compressType,
                                       const std::string& body,
                                       size_t rawSize,
                                       std::string& path,
                                       std::map<std::string, std::string>& header);
SLSResponse PostLogStoreLogs(const std::string& accessKeyId,
                             const std::string& accessKeySecret,
                             SLSClientManager::AuthType type,
                             const std::string& host,
                             bool httpsFlag,
                             const std::string& project,
                             const std::string& logstore,
                             const std::string& compressType,
                             RawDataType dataType,
                             const std::string& body,
                             size_t rawSize,
                             const std::string& shardHashKey);
SLSResponse PostMetricStoreLogs(const std::string& accessKeyId,
                                const std::string& accessKeySecret,
                                SLSClientManager::AuthType type,
                                const std::string& host,
                                bool httpsFlag,
                                const std::string& project,
                                const std::string& logstore,
                                const std::string& compressType,
                                const std::string& body,
                                size_t rawSize);
SLSResponse PutWebTracking(const std::string& host,
                           bool httpsFlag,
                           const std::string& logstore,
                           const std::string& compressType,
                           const std::string& body,
                           size_t rawSize);

} // namespace logtail
