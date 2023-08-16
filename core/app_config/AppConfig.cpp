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
#include "common/StringTools.h"
#include "random"

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

    // configserver path
    mConfigServerAvailable = false;
    if (confJson.isMember("ilogtail_configserver_address") && confJson["ilogtail_configserver_address"].isArray()) {
        for (Json::Value::ArrayIndex i = 0; i < confJson["ilogtail_configserver_address"].size(); ++i) {
            vector<string> configServerAddress
                = SplitString(TrimString(confJson["ilogtail_configserver_address"][i].asString()), ":");

            if (configServerAddress.size() != 2) {
                LOG_WARNING(sLogger,
                            ("ilogtail_configserver_address", "format error")(
                                "wrong address", TrimString(confJson["ilogtail_configserver_address"][i].asString())));
                continue;
            }

            string host = configServerAddress[0];
            int32_t port = atoi(configServerAddress[1].c_str());

            if (port < 1 || port > 65535)
                LOG_WARNING(sLogger, ("ilogtail_configserver_address", "illegal port")("port", port));
            else
                mConfigServerAddresses.push_back(ConfigServerAddress(host, port));
        }

        mConfigServerAvailable = true;
        LOG_INFO(sLogger,
                 ("ilogtail_configserver_address", confJson["ilogtail_configserver_address"].toStyledString()));
    }

    // tags for configserver
    if (confJson.isMember("ilogtail_tags") && confJson["ilogtail_tags"].isObject()) {
        Json::Value::Members members = confJson["ilogtail_tags"].getMemberNames();
        for (Json::Value::Members::iterator it = members.begin(); it != members.end(); it++) {
            mConfigServerTags.push_back(confJson["ilogtail_tags"][*it].asString());
        }

        LOG_INFO(sLogger, ("ilogtail_configserver_tags", confJson["ilogtail_tags"].toStyledString()));
    }
}

AppConfig::ConfigServerAddress AppConfig::GetOneConfigServerAddress(bool changeConfigServer) {
    if (0 == mConfigServerAddresses.size()) {
        return AppConfig::ConfigServerAddress("", -1); // No address available
    }

    // Return a random address
    if (changeConfigServer) {
        std::random_device rd;
        int tmpId = rd() % mConfigServerAddresses.size();
        while (mConfigServerAddresses.size() > 1 && tmpId == mConfigServerAddressId) {
            tmpId = rd() % mConfigServerAddresses.size();
        }
        mConfigServerAddressId = tmpId;
    }
    return AppConfig::ConfigServerAddress(mConfigServerAddresses[mConfigServerAddressId].host,
                                          mConfigServerAddresses[mConfigServerAddressId].port);
}

} // namespace logtail
