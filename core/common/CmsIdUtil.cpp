/*
 * Copyright 2025 iLogtail Authors
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

#include "common/CmsIdUtil.h"
#include "common/HashUtil.h"
#include "common/StringTools.h"
#include "logger/Logger.h"

namespace logtail {

static const char* kAPMProjectPrefix = "proj-xtrace-";
static const char* kDefaultCMSPrefix = "default-cms-";
static const char* kTokenSuffix = "token";
static const char* kAppTypeTrace = "TRACE";
static const char kAt = '@';
static const char kHyphen = '-';
static const char* kFinanece = "finance";

inline bool IsFinanceRegion(const std::string& regionId) {
    return regionId.find(kFinanece) != std::string::npos;
}

int EncodeUserId(const std::string& userId, std::string& res) {
    int64_t uid = 0;
    auto ret = From<int64_t>(userId, 10, uid);
    if (ret) {
        LOG_ERROR(sLogger, ("failed to parse string to int64", userId));
        return 1;
    }
    LOG_WARNING(sLogger, ("after from 10, uid", uid)("userId", userId));
    return Parse<int64_t>(uid, 36, res);
}

int GenAPMPid(const std::string& userId, const std::string& appName, std::string& res) {
    if (userId.empty() || appName.empty()) {
        return 1;
    }
    auto s = appName + kTokenSuffix;
    auto siteNameOri = CalcMD5(s).substr(0, 15);
    auto siteName = ToLowerCaseString(siteNameOri);
    std::string userIdEncoded;
    int status = EncodeUserId(userId, userIdEncoded);
    if (status) {
        LOG_ERROR(sLogger, ("failed to encode userId", userId));
        return 1;
    }
    res = userIdEncoded + kAt + siteName;
    return 0;
}

std::string GetDefaultWorkspace(const std::string& userId, const std::string& regionId) {
    return kDefaultCMSPrefix + userId + kHyphen + regionId;
}

bool IsDefaultWorkspace(const std::string& userId, const std::string& regionId, const std::string& workspace) {
    return workspace.empty() || workspace == GetDefaultWorkspace(userId, regionId);
}

int GenerateSiteName(const std::string& workspace, const std::string& pid, const std::string& regionId, std::string& res) {
    auto str = pid + workspace + kAppTypeTrace + regionId + kTokenSuffix;
    res = CalcMD5(str).substr(0, 21);
    res = ToLowerCaseString(res);
    return 0;
}

int AdaptMD5(std::string& input) {
    if (input.size() & 0x1) {
        return 1;
    }
    static char sZero = '0';
    int j = 0;
    for (size_t i = 0; i < input.size(); i ++) {
        if (!(i & 0x1) && input[i] == sZero) {
            continue;
        }
        input[j++] = input[i];
    }
    input = input.substr(0, j);
    return 0;
}

int GenerateServiceId(const std::string& userId, const std::string& regionId, const std::string& pid, const std::string& workspace, std::string& res) {
    /**
     * String siteNameCode = ArmsMD5.getMD5String(pid + workspace + "TRACE" + regionId + "token")
        .substring(0, 21);
    return encodeUserId(userId) + "@" + siteNameCode;
     */
    std::string site;
    auto ret = GenerateSiteName(workspace, pid, regionId, site);
    if (ret) {
        return 1;
    }
    std::string userIdEncoded;
    ret = EncodeUserId(userId, userIdEncoded);
    if (ret) {
        return 1;
    }
    res = userIdEncoded + kAt + site;

    return 0;
}

int GenerateWorkspaceAPMProject(const std::string& workspace, const std::string& userId, const std::string& regionId, std::string& res) {
    if (IsDefaultWorkspace(userId, regionId, workspace)) {
        if (IsFinanceRegion(regionId)) {
            auto s = userId + regionId;
            auto md5 = CalcMD5(s);
            AdaptMD5(md5);
            res = kAPMProjectPrefix + md5;
            res = ToLowerCaseString(res);
            return 0;
        }
        auto md5 = CalcMD5(userId);
        AdaptMD5(md5);
        res = kAPMProjectPrefix + md5 + kHyphen + regionId;
        res = ToLowerCaseString(res);
        return 0;
    }

    if (IsFinanceRegion(regionId) || regionId.size() > 18) {
        auto s = userId + workspace;
        auto md5 = CalcMD5(s);
        AdaptMD5(md5);
        res = kAPMProjectPrefix + md5 + kHyphen + regionId;
        res = ToLowerCaseString(res);
        return 0;
    }

    auto s = userId + workspace;
    auto md5 = CalcMD5(s);
    AdaptMD5(md5);
    res = kAPMProjectPrefix + md5 + kHyphen + regionId;
    res = ToLowerCaseString(res);
    return 0;
}

} // namespace logtail
