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

#include "ProfileSender.h"

#include "common/LogtailCommonFlags.h"

using namespace std;

DEFINE_FLAG_STRING(profile_project_name, "profile project_name for logtail", "");

namespace logtail {

ProfileSender::ProfileSender()
    : mDefaultProfileProjectName(STRING_FLAG(profile_project_name)),
      mDefaultProfileRegion(STRING_FLAG(default_region_name)) {
}

ProfileSender* ProfileSender::GetInstance() {
    static auto* sInstance = new ProfileSender();
    return sInstance;
}

string ProfileSender::GetDefaultProfileRegion() {
    ScopedSpinLock lock(mProfileLock);
    return mDefaultProfileRegion;
}

void ProfileSender::SetDefaultProfileRegion(const string& profileRegion) {
    ScopedSpinLock lock(mProfileLock);
    mDefaultProfileRegion = profileRegion;
}

string ProfileSender::GetDefaultProfileProjectName() {
    ScopedSpinLock lock(mProfileLock);
    return mDefaultProfileProjectName;
}

void ProfileSender::SetDefaultProfileProjectName(const string& profileProjectName) {
    ScopedSpinLock lock(mProfileLock);
    mDefaultProfileProjectName = profileProjectName;
}

string ProfileSender::GetProfileProjectName(const string& region) {
    ScopedSpinLock lock(mProfileLock);
    if (region.empty()) {
        return mDefaultProfileProjectName;
    }
    auto iter = mRegionProjectNameMap.find(region);
    if (iter == mRegionProjectNameMap.end()) {
        return mDefaultProfileProjectName;
    }
    return iter->second;
}

void ProfileSender::GetAllProfileRegion(vector<string>& allRegion) {
    ScopedSpinLock lock(mProfileLock);
    if (mRegionProjectNameMap.find(mDefaultProfileRegion) == mRegionProjectNameMap.end()) {
        allRegion.push_back(mDefaultProfileRegion);
    }
    for (auto iter = mRegionProjectNameMap.begin(); iter != mRegionProjectNameMap.end(); ++iter) {
        allRegion.push_back(iter->first);
    }
}

void ProfileSender::SetProfileProjectName(const string& region, const string& profileProject) {
    ScopedSpinLock lock(mProfileLock);
    mRegionProjectNameMap[region] = profileProject;
}

bool ProfileSender::IsProfileData(const string& region, const string& project, const string& logstore) {
// TODO: temporarily used, profile should work in unit test
#ifndef APSARA_UNIT_TEST_MAIN
    if ((logstore == "shennong_log_profile" || logstore == "logtail_alarm" || logstore == "logtail_status_profile"
         || logstore == "logtail_suicide_profile")
        && (project == GetProfileProjectName(region) || region == ""))
        return true;
    else
        return false;
#else
    return false;
#endif
}

} // namespace logtail
