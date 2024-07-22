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
#include "config/watcher/ConfigWatcher.h"

using namespace std;

namespace logtail {

void ConfigProvider::Init(const string& dir) {
    // default path: /etc/ilogtail/config/${dir}
    mPipelineSourceDir.assign(AppConfig::GetInstance()->GetLogtailSysConfDir());
    mPipelineSourceDir /= "config";
    mPipelineSourceDir /= dir;

    mProcessSourceDir.assign(AppConfig::GetInstance()->GetLogtailSysConfDir());
    mProcessSourceDir /= "processconfig";
    mProcessSourceDir /= dir;

    error_code ec;
    filesystem::create_directories(mPipelineSourceDir, ec);
    ConfigWatcher::GetInstance()->AddPipelineSource(mPipelineSourceDir, &mPipelineMux);

    ec.clear();
    filesystem::create_directories(mProcessSourceDir, ec);
    ConfigWatcher::GetInstance()->AddProcessSource(mProcessSourceDir, &mProcessMux);
}

} // namespace logtail
