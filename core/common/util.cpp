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

#include "util.h"
#include <string>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <stdio.h>
#include <exception>
#if defined(__linux__)
#include <dirent.h>
#include <uuid/uuid.h>
#include <execinfo.h>
#elif defined(_MSC_VER)
#include <Rpc.h>
#include "WinUuid.h"
#endif
#include "logger/Logger.h"
#include "StringTools.h"
#include "LogtailCommonFlags.h"
#include "FileSystemUtil.h"
#include "JsonUtil.h"
using namespace std;

namespace logtail {

std::string CheckAddress(const std::string& definedAddress, const std::string& defaultAdderss) {
    return definedAddress;
}

std::string CalculateDmiUUID() {
    return CalculateRandomUUID();
}

std::string CalculateRandomUUID() {
#if defined(__linux__)
    uuid_t in;
    uuid_generate_time(in);
    char out[128];
    uuid_unparse_upper(in, out);
    return std::string(out);
#elif defined(_MSC_VER)
    UUID uuid;
    UuidCreate(&uuid);
    unsigned char* str;
    UuidToString(&uuid, &str);
    std::string s((char*)str);
    RpcStringFree(&str);
    return s;
#endif
}

namespace util {

    const std::string kConfigPrefix = "/etc/ilogtail/conf/";
    const std::string kConfigSuffix = "/ilogtail_config.json";
    const std::string kConfigTemplate
        = std::string("{\n"
                      "    \"config_server_address\" : \"http://logtail.${configEndpoint}\",\n"
                      "    \"data_server_list\" : \n"
                      "    [\n"
                      "        {\n"
                      "            \"cluster\" : \"${region}\",\n"
                      "            \"endpoint\" : \"${dataEndpoint}\"\n"
                      "        }\n"
                      "    ]\n"
                      "}\n");
    const std::string kHangzhouCorp = "cn-hangzhou-corp";
    const std::string kLogEndpointSuffix = ".log.aliyuncs.com";
    const std::string kAccelerationDataEndpoint = "log-global.aliyuncs.com";

} // namespace util

bool GenerateAPPConfigByConfigPath(const std::string& filePath, Json::Value& confJson) {
    return false;
}

std::string GenerateLogtailConfigByInstallParam(const std::string& installParam) {
    auto region = installParam;
    std::string configEndpoint;
    std::string dataEndpoint;
    if (EndWith(region, "-internet") || EndWith(region, "_internet")) {
        region = region.substr(0, region.size() - string("-internet").size());
        dataEndpoint = configEndpoint = region + util::kLogEndpointSuffix;
    } else if (EndWith(region, "-inner") || EndWith(region, "_inner")) {
        region = region.substr(0, region.size() - string("-inner").size());
        dataEndpoint = configEndpoint = region + "-share" + util::kLogEndpointSuffix;
    } else if (EndWith(region, "-acceleration") || EndWith(region, "_acceleration")) {
        region = region.substr(0, region.size() - string("-acceleration").size());
        configEndpoint = region + util::kLogEndpointSuffix;
        dataEndpoint = util::kAccelerationDataEndpoint;
    } else {
        dataEndpoint = configEndpoint = region + "-intranet" + util::kLogEndpointSuffix;
    }

    if (region == util::kHangzhouCorp || region == "cn-shanghai-corp") {
        region = util::kHangzhouCorp;
        dataEndpoint = configEndpoint = region + ".sls.aliyuncs.com";
    }

    std::string configStr = util::kConfigTemplate;
    ReplaceString(configStr, "${configEndpoint}", configEndpoint);
    ReplaceString(configStr, "${dataEndpoint}", dataEndpoint);
    ReplaceString(configStr, "${region}", region);
    LOG_INFO(sLogger, ("param", installParam)("generated config", configStr));

    if (IsValidJson(configStr.c_str(), configStr.size())) {
        return configStr;
    }
    LOG_ERROR(sLogger, ("generated config is invalid", configStr));
    return "";
}

} // namespace logtail
