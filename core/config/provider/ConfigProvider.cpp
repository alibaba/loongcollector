// Copyright 2023 iLogtail Authors
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

#include "config/provider/ConfigProvider.h"

#include "app_config/AppConfig.h"
#include "config/watcher/InstanceConfigWatcher.h"
#include "config/watcher/PipelineConfigWatcher.h"

using namespace std;

namespace logtail {

void ConfigProvider::Init(const string& dir) {
    // default path: /etc/ilogtail/config/${dir}
    mContinuousPipelineConfigDir.assign(AppConfig::GetInstance()->GetLoongcollectorConfDir());
    mContinuousPipelineConfigDir /= GetContinuousPipelineConfigDir();
    mContinuousPipelineConfigDir /= dir;

    mInstanceSourceDir.assign(AppConfig::GetInstance()->GetLoongcollectorConfDir());
    mInstanceSourceDir /= "instance_config";
    mInstanceSourceDir /= dir;

    error_code ec;
    filesystem::create_directories(mContinuousPipelineConfigDir, ec);
    PipelineConfigWatcher::GetInstance()->AddSource(mContinuousPipelineConfigDir, &mContinuousPipelineMux);

    ec.clear();
    filesystem::create_directories(mInstanceSourceDir, ec);
    InstanceConfigWatcher::GetInstance()->AddSource(mInstanceSourceDir, &mInstanceMux);
}

} // namespace logtail
