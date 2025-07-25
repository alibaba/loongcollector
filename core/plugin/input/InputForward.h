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

#pragma once

#include <string>
#include <vector>

#include "collection_pipeline/plugin/interface/Input.h"

namespace logtail {

struct MatchRule {
    std::string key;
    std::string value;

    bool IsMatch(const std::string& inputKey, const std::string& inputValue) const {
        return key == inputKey && value == inputValue;
    }

    bool IsEmpty() const { return key.empty() && value.empty(); }
};

class InputForward : public Input {
public:
    static const std::string sName;
    static const std::vector<std::string> sSupportedProtocols;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool SupportAck() const override { return true; }

    std::string mProtocol = "LoongSuite";
    std::string mEndpoint;
    MatchRule mMatchRule;

private:
    std::string mConfigName;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputForwardUnittest;
#endif
};

} // namespace logtail
