// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "app_config/AppConfig.h"
#include "config_manager/ConfigManager.h"
#include "logger/Logger.h"
#include "ProfileSender.h"
#include "sender/Sender.h"
#include "sdk/Common.h"
#include "sdk/Client.h"
#include "sdk/Exception.h"
#include "common/CompressTools.h"
#include "common/LogtailCommonFlags.h"
#include "sls_control/SLSControl.h"

using namespace std;

namespace logtail {

void ProfileSender::SendToProfileProject(const std::string& region, sls_logs::LogGroup& logGroup) {
    if (0 == logGroup.category().compare("logtail_status_profile")) {
        SendRunningStatus(logGroup);
    }

    // Opensource is not necessary to synchronize data with SLS
    Sender::Instance()->RestLastSenderTime();
    ConfigManager::GetInstance()->RestLastConfigTime();
    return;
}

void ProfileSender::SendRunningStatus(sls_logs::LogGroup& logGroup) {
    static int controlFeq = 0;

    // every 12 hours
    if (0 == logGroup.logs_size() || 0 != controlFeq++ % (60 * 12)) {
        return;
    }

    std::string region = "cn-shanghai";
    std::string project = "ilogtail-community-edition";
    std::string logstore = "ilogtail-online";
    std::string endpoint = region + ".log.aliyuncs.com";

    Json::Value logtailStatus;
    logtailStatus["__topic__"] = "logtail_status_profile";
    Json::Value status;
    const sls_logs::Log& log = logGroup.logs(0);
    for (int32_t conIdx = 0; conIdx < log.contents_size(); ++conIdx) {
        const sls_logs::Log_Content& content = log.contents(conIdx);
        const string& key = content.key();
        const string& value = content.value();
        status[key] = value;
    }
    logtailStatus["__logs__"][0] = status;
    std::string logBody = logtailStatus.toStyledString();
    sdk::Client client(endpoint, "", "", INT32_FLAG(sls_client_send_timeout), "", true, "");
    SLSControl::Instance()->SetSlsSendClientCommonParam(&client);
    try {
        time_t curTime = time(NULL);
        LoggroupTimeValue* data = new LoggroupTimeValue(
            project, logstore, "", "", false, "", region, LOGGROUP_LZ4_COMPRESSED, 1, logBody.size(), curTime, "", 0);

        if (!CompressLz4(logBody, data->mLogData)) {
            LOG_ERROR(sLogger, ("lz4 compress data", "fail"));
            return;
        }

        sdk::PostLogStoreLogsResponse resp
            = client.PostLogUsingWebTracking(data->mProjectName, data->mLogstore, data->mLogData, data->mRawSize);

        LOG_DEBUG(sLogger,
                  ("SendToProfileProject",
                   "success")("logBody", logBody)("requestId", resp.requestId)("statusCode", resp.statusCode));
    } catch (const sdk::LOGException& e) {
        LOG_DEBUG(sLogger,
                  ("SendToProfileProject", "fail")("logBody", logBody)("errCode", e.GetErrorCode())("errMsg",
                                                                                                    e.GetMessage()));
    }
}

bool ProfileSender::SendInstantly(sls_logs::LogGroup& logGroup,
                                  const std::string& aliuid,
                                  const std::string& region,
                                  const std::string& projectName,
                                  const std::string& logstore) {
    return true;
}

void ProfileSender::SendToLineCountProject(const std::string& region,
                                           const std::string& projectName,
                                           sls_logs::LogGroup& logGroup) {
    return;
}


} // namespace logtail