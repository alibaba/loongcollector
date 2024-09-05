// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/input/InputObserverNetwork.h"

#include "observer/network/NetworkConfig.h"

using namespace std;

namespace logtail {

const std::string InputObserverNetwork::sName = "input_observer_network";

bool InputObserverNetwork::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    mDetail = config.toStyledString();
    return true;
}

bool InputObserverNetwork::Start() {
    NetworkConfig::GetInstance()->mAllNetworkConfigs[mContext->GetConfigName()] = &mContext->GetPipeline();
    return true;
}

bool InputObserverNetwork::Stop(bool isPipelineRemoving) {
    NetworkConfig::GetInstance()->mAllNetworkConfigs.erase(mContext->GetConfigName());
    return true;
}

} // namespace logtail
