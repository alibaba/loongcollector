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

#include <memory>
#include <string>

#include <json/json.h>

#include "common/JsonUtil.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "pipeline/PipelineContext.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ContainerDiscoveryOptionsUnittest : public testing::Test {
public:
    void OnSuccessfulInit() const;

private:
    const string pluginType = "test";
    PipelineContext ctx;
};

void ContainerDiscoveryOptionsUnittest::OnSuccessfulInit() const {
    unique_ptr<ContainerDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // only mandatory param
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_EQUAL("", config->mContainerFilters.mK8sNamespaceRegex);
    APSARA_TEST_EQUAL("", config->mContainerFilters.mK8sPodRegex);
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mIncludeK8sLabel.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mExcludeK8sLabel.size());
    APSARA_TEST_EQUAL("", config->mContainerFilters.mK8sContainerRegex);
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mIncludeEnv.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mExcludeEnv.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mIncludeContainerLabel.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mExcludeContainerLabel.size());
    APSARA_TEST_EQUAL(0U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(0U, config->mExternalEnvTag.size());
    APSARA_TEST_FALSE(config->mCollectingContainersMeta);

    // valid optional param
    configStr = R"(
        {
            "ContainerFilters": {
                "K8sNamespaceRegex": "default",
                "K8sPodRegex": "pod",
                "IncludeK8sLabel": {
                    "key": "v1"
                },
                "ExcludeK8sLabel": {
                    "key": "v2"
                },
                "K8sContainerRegex": "container",
                "IncludeEnv": {
                    "key": "v3"
                },
                "ExcludeEnv": {
                    "key": "v4"
                },
                "IncludeContainerLabel": {
                    "key": "v5"
                },
                "ExcludeContainerLabel": {
                    "key": "v6"
                }
            },
            "ExternalK8sLabelTag": {
                "a": "b"
            },
            "ExternalEnvTag": {
                "c": "d"
            },
            "CollectingContainersMeta": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL("default", config->mContainerFilters.mK8sNamespaceRegex);
    APSARA_TEST_EQUAL("pod", config->mContainerFilters.mK8sPodRegex);
    APSARA_TEST_EQUAL(1U, config->mContainerFilters.mIncludeK8sLabel.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilters.mExcludeK8sLabel.size());
    APSARA_TEST_EQUAL("container", config->mContainerFilters.mK8sContainerRegex);
    APSARA_TEST_EQUAL(1U, config->mContainerFilters.mIncludeEnv.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilters.mExcludeEnv.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilters.mIncludeContainerLabel.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilters.mExcludeContainerLabel.size());
    APSARA_TEST_EQUAL(1U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(1U, config->mExternalEnvTag.size());
    APSARA_TEST_TRUE(config->mCollectingContainersMeta);

    // invalid optional param
    configStr = R"(
        {
            "ContainerFilters": {
                "K8sNamespaceRegex": true,
                "K8sPodRegex": true,
                "IncludeK8sLabel": true,
                "ExcludeK8sLabel": true,
                "K8sContainerRegex": true,
                "IncludeEnv": true,
                "ExcludeEnv": true,
                "IncludeContainerLabel": true,
                "ExcludeContainerLabel": true
            },
            "ExternalK8sLabelTag": true,
            "ExternalEnvTag": true,
            "CollectingContainersMeta": "true"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL("", config->mContainerFilters.mK8sNamespaceRegex);
    APSARA_TEST_EQUAL("", config->mContainerFilters.mK8sPodRegex);
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mIncludeK8sLabel.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mExcludeK8sLabel.size());
    APSARA_TEST_EQUAL("", config->mContainerFilters.mK8sContainerRegex);
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mIncludeEnv.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mExcludeEnv.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mIncludeContainerLabel.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilters.mExcludeContainerLabel.size());
    APSARA_TEST_EQUAL(0U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(0U, config->mExternalEnvTag.size());
    APSARA_TEST_FALSE(config->mCollectingContainersMeta);
}

UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, OnSuccessfulInit)

} // namespace logtail

UNIT_TEST_MAIN
