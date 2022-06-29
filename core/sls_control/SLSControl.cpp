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

#include "SLSControl.h"
#include "app_config/AppConfig.h"
#include "config_manager/ConfigManager.h"
#include "common/LogtailCommonFlags.h"
#include "common/version.h"
#include "logger/Logger.h"

using namespace std;
using namespace sls_logs;

namespace logtail {

SLSControl::SLSControl() {
}

SLSControl* SLSControl::Instance() {
    static SLSControl* slsControl = new SLSControl();
    slsControl->user_agent = std::string("ilogtail/") + ILOGTAIL_VERSION;
    return slsControl;
}

void SLSControl::SetSlsSendClientCommonParam(sdk::Client* sendClient) {
    sendClient->SetUserAgent(user_agent);
    sendClient->SetPort(AppConfig::GetInstance()->GetDataServerPort());
}

bool SLSControl::SetSlsSendClientAuth(const std::string aliuid,
                                      const bool init,
                                      sdk::Client* sendClient,
                                      int32_t& lastUpdateTime) {
    sendClient->SetAccessKeyId(STRING_FLAG(default_access_key_id));
    sendClient->SetAccessKey(STRING_FLAG(default_access_key));
    LOG_INFO(sLogger, ("SetAccessKeyId", STRING_FLAG(default_access_key_id)));
    return true;
}

} // namespace logtail