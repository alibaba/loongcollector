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

#include "AppConfig.h"
#include "common/StringTools.h"
#include "logger/Logger.h"
#include "common/util.h"

using namespace std;

namespace logtail {

AppConfig::AppConfig() {
}

AppConfig::~AppConfig() {
}

void AppConfig::LoadAddrConfig(const Json::Value& confJson) {
    if (confJson.isMember("bind_interface") && confJson["bind_interface"].isString()) {
        mBindInterface = TrimString(confJson["bind_interface"].asString());
        if (ToLowerCaseString(mBindInterface) == "default")
            mBindInterface.clear();
        LOG_INFO(sLogger, ("bind_interface", mBindInterface));
    }
}

} // namespace logtail