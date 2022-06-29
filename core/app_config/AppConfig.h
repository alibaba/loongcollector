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
#include "AppConfigBase.h"
#include <vector>
#include "common/util.h"

namespace logtail {
class AppConfig : public AppConfigBase {
private:
    void LoadAddrConfig(const Json::Value& confJson) override;

public:
    AppConfig();
    ~AppConfig();

    static AppConfig* GetInstance() {
        static AppConfig singleton;
        return &singleton;
    }

    bool IsDebugMode() const { return false; }

    /**
     * @brief is in private cloud mode
     * @return
     */
    bool IsDataServerPrivateCloud() const { return false; }

    const std::string& GetBindInterface() const { return mBindInterface; }

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderUnittest;
    friend class ConfigUpdatorUnittest;
    friend class MultiServerConfigUpdatorUnitest;
    friend class UtilUnittest;
    friend class AppConfigUnittest;
#endif
};
} // namespace logtail
