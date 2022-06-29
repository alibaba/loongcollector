/*
 * Copyright 2022 iLogtail Authors
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
#include "log_pb/sls_logs.pb.h"
#include <string>
#include <vector>

namespace logtail {

class ProfileSender {
public:
    ProfileSender() {}
    void SendToProfileProject(const std::string& region, sls_logs::LogGroup& logGroup);
    bool SendInstantly(sls_logs::LogGroup& logGroup,
                       const std::string& aliuid,
                       const std::string& region,
                       const std::string& projectName,
                       const std::string& logstore);

    void
    SendToLineCountProject(const std::string& region, const std::string& projectName, sls_logs::LogGroup& logGroup);

private:
    void SendRunningStatus(sls_logs::LogGroup& logGroup);
};

} // namespace logtail
