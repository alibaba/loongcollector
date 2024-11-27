/*
 * Copyright 2024 iLogtail Authors
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

#include "plugin/input/InputInternalMetrics.h"

namespace logtail {

const std::string InputInternalMetrics::sName = "input_internal_metrics";

bool GetEnabled(const Json::Value& rule) {
    if (rule.isMember("Enable") && rule["Enable"].isBool())
        return rule["Enable"].asBool();
    return true;
}

int GetInterval(const Json::Value& rule) {
    if (rule.isMember("Interval") && rule["Interval"].isInt())
        return rule["Interval"].asInt();
    return 10;
}

void ParseSelfMonitorMetricRule(std::string&& ruleKey, const Json::Value& ruleJson, SelfMonitorMetricRule& rule) {
    if (ruleJson.isMember(ruleKey) && ruleJson[ruleKey].isObject()) {
        rule.mEnable = GetEnabled(ruleJson[ruleKey]);
        rule.mInterval = GetInterval(ruleJson[ruleKey]);
    }
}

bool InputInternalMetrics::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    ParseSelfMonitorMetricRule("Agent", config, mSelfMonitorMetricRules.mAgentMetricsRule);
    ParseSelfMonitorMetricRule("Runner", config, mSelfMonitorMetricRules.mRunnerMetricsRule);
    ParseSelfMonitorMetricRule("Pipeline", config, mSelfMonitorMetricRules.mPipelineMetricsRule);
    ParseSelfMonitorMetricRule("PluginSource", config, mSelfMonitorMetricRules.mPluginSourceMetricsRule);
    ParseSelfMonitorMetricRule("Plugin", config, mSelfMonitorMetricRules.mPluginMetricsRule);
    ParseSelfMonitorMetricRule("Component", config, mSelfMonitorMetricRules.mComponentMetricsRule);
    return true;
}

bool InputInternalMetrics::Start() {
    SelfMonitorServer::GetInstance()->UpdateMetricPipeline(mContext, &mSelfMonitorMetricRules);
    return true;
}

bool InputInternalMetrics::Stop(bool isPipelineRemoving) {
    if (isPipelineRemoving) {
        SelfMonitorServer::GetInstance()->RemoveMetricPipeline();
    }
    return true;
}

} // namespace logtail