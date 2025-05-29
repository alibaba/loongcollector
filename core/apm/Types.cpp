/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apm/Types.h"

namespace logtail::apm {

bool InitObserverNetworkOption(const Json::Value& config, std::unique_ptr<AttachConfig>& attachConfig) {
    std::string errorMsg;
    // AppId
    // Project
    // if (!GetMandatoryStringParam(config, "AppId", attachConfig->mAppId, errorMsg)) {
    //     PARAM_ERROR_RETURN(mContext->GetLogger(),
    //                        mContext->GetAlarm(),
    //                        errorMsg,
    //                        sName,
    //                        mContext->GetConfigName(),
    //                        mContext->GetProjectName(),
    //                        mContext->GetLogstoreName(),
    //                        mContext->GetRegion());
    // }

    // // EnableSpan (Optional)
    // if (!GetOptionalBoolParam(probeConfig, "EnableSpan", thisObserverNetworkOption.mEnableSpan, errorMsg)) {
    //     PARAM_WARNING_IGNORE(mContext->GetLogger(),
    //                          mContext->GetAlarm(),
    //                          errorMsg,
    //                          sName,
    //                          mContext->GetConfigName(),
    //                          mContext->GetProjectName(),
    //                          mContext->GetLogstoreName(),
    //                          mContext->GetRegion());
    // }
    return true;
}

}