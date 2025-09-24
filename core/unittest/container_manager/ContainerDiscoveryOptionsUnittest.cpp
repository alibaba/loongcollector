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

#include "json/json.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ContainerDiscoveryOptionsUnittest : public testing::Test {
public:
    void OnSuccessfulInit() const;
    void TestInitWithInvalidConfig() const;
    void TestContainerFilterConfigInit() const;
    void TestRegexCompilation() const;
    void TestEmptyConfig() const;
    void TestComplexFilterCombination() const;
    void TestExternalTagMapping() const;
    void TestGetCustomExternalTags() const;

private:
    const string pluginType = "test";
    CollectionPipelineContext ctx;
};

void ContainerDiscoveryOptionsUnittest::OnSuccessfulInit() const {
    unique_ptr<ContainerDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // only mandatory param
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_EQUAL("", config->mContainerFilterConfig.mK8sNamespaceRegex);
    APSARA_TEST_EQUAL("", config->mContainerFilterConfig.mK8sPodRegex);
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mIncludeK8sLabel.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mExcludeK8sLabel.size());
    APSARA_TEST_EQUAL("", config->mContainerFilterConfig.mK8sContainerRegex);
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mIncludeEnv.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mExcludeEnv.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mIncludeContainerLabel.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mExcludeContainerLabel.size());
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
    APSARA_TEST_EQUAL("default", config->mContainerFilterConfig.mK8sNamespaceRegex);
    APSARA_TEST_EQUAL("pod", config->mContainerFilterConfig.mK8sPodRegex);
    APSARA_TEST_EQUAL(1U, config->mContainerFilterConfig.mIncludeK8sLabel.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilterConfig.mExcludeK8sLabel.size());
    APSARA_TEST_EQUAL("container", config->mContainerFilterConfig.mK8sContainerRegex);
    APSARA_TEST_EQUAL(1U, config->mContainerFilterConfig.mIncludeEnv.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilterConfig.mExcludeEnv.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilterConfig.mIncludeContainerLabel.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilterConfig.mExcludeContainerLabel.size());
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
    APSARA_TEST_EQUAL("", config->mContainerFilterConfig.mK8sNamespaceRegex);
    APSARA_TEST_EQUAL("", config->mContainerFilterConfig.mK8sPodRegex);
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mIncludeK8sLabel.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mExcludeK8sLabel.size());
    APSARA_TEST_EQUAL("", config->mContainerFilterConfig.mK8sContainerRegex);
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mIncludeEnv.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mExcludeEnv.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mIncludeContainerLabel.size());
    APSARA_TEST_EQUAL(0U, config->mContainerFilterConfig.mExcludeContainerLabel.size());
    APSARA_TEST_EQUAL(0U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(0U, config->mExternalEnvTag.size());
    APSARA_TEST_FALSE(config->mCollectingContainersMeta);
}

void ContainerDiscoveryOptionsUnittest::TestInitWithInvalidConfig() const {
    unique_ptr<ContainerDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // Test with null config
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(Json::Value(), ctx, pluginType));
    // Should initialize with defaults
    APSARA_TEST_FALSE(config->mCollectingContainersMeta);

    // Test with invalid JSON structure
    configStr = R"(
        {
            "ContainerFilters": "invalid_string_instead_of_object",
            "ExternalK8sLabelTag": [],
            "ExternalEnvTag": 123,
            "CollectingContainersMeta": "not_a_boolean"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    // Should handle invalid types gracefully with defaults
    APSARA_TEST_EQUAL(0U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(0U, config->mExternalEnvTag.size());
    APSARA_TEST_FALSE(config->mCollectingContainersMeta);

    // Test with invalid regex patterns
    configStr = R"(
        {
            "ContainerFilters": {
                "K8sNamespaceRegex": "[invalid regex",
                "IncludeK8sLabel": {
                    "key": "[another invalid regex"
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_FALSE(config->Init(configJson, ctx, pluginType));
}

void ContainerDiscoveryOptionsUnittest::TestContainerFilterConfigInit() const {
    ContainerFilterConfig filterConfig;
    Json::Value configJson;
    string configStr, errorMsg;

    // Test empty config
    APSARA_TEST_TRUE(filterConfig.Init(Json::Value(), ctx, pluginType));
    APSARA_TEST_EQUAL("", filterConfig.mK8sNamespaceRegex);
    APSARA_TEST_EQUAL(0U, filterConfig.mIncludeK8sLabel.size());

    // Test valid config
    configStr = R"(
        {
            "K8sNamespaceRegex": "default",
            "K8sPodRegex": "app-.*",
            "K8sContainerRegex": "nginx",
            "IncludeK8sLabel": {
                "app": "web",
                "version": "^v[0-9]+$"
            },
            "ExcludeK8sLabel": {
                "env": "test"
            },
            "IncludeEnv": {
                "SERVICE_NAME": "nginx"
            },
            "ExcludeEnv": {
                "DEBUG": "true"
            },
            "IncludeContainerLabel": {
                "maintainer": "team"
            },
            "ExcludeContainerLabel": {
                "deprecated": "true"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(filterConfig.Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL("default", filterConfig.mK8sNamespaceRegex);
    APSARA_TEST_EQUAL("app-.*", filterConfig.mK8sPodRegex);
    APSARA_TEST_EQUAL("nginx", filterConfig.mK8sContainerRegex);
    APSARA_TEST_EQUAL(2U, filterConfig.mIncludeK8sLabel.size());
    APSARA_TEST_EQUAL(1U, filterConfig.mExcludeK8sLabel.size());
    APSARA_TEST_EQUAL(1U, filterConfig.mIncludeEnv.size());
    APSARA_TEST_EQUAL(1U, filterConfig.mExcludeEnv.size());
    APSARA_TEST_EQUAL(1U, filterConfig.mIncludeContainerLabel.size());
    APSARA_TEST_EQUAL(1U, filterConfig.mExcludeContainerLabel.size());

    // Test GetContainerFilters
    ContainerFilters filters;
    std::string exception;
    APSARA_TEST_TRUE(filters.Init(filterConfig, exception));
    APSARA_TEST_TRUE(filters.mK8SFilter.mNamespaceReg != nullptr);
    APSARA_TEST_TRUE(filters.mK8SFilter.mPodReg != nullptr);
    APSARA_TEST_TRUE(filters.mK8SFilter.mContainerReg != nullptr);
}

void ContainerDiscoveryOptionsUnittest::TestRegexCompilation() const {
    ContainerFilterConfig filterConfig;
    Json::Value configJson;
    string configStr, errorMsg;

    // Test regex patterns - only strings starting with ^ and ending with $ are treated as regex
    configStr = R"(
        {
            "IncludeK8sLabel": {
                "valid_regex": "^web.*$",
                "valid_regex1": "^web.*",
                "invalid_regex2": "web.*$",
                "static_match": "exact-match"
            },
            "ExcludeEnv": {
                "temp": ".*temp.*"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(filterConfig.Init(configJson, ctx, pluginType));

    ContainerFilters filters;
    std::string exception;
    APSARA_TEST_TRUE(filters.Init(filterConfig, exception));

    // Verify regex compilation - only valid regex (^...) should be in mFieldsRegMap
    APSARA_TEST_TRUE(filters.mK8SFilter.mK8sLabelFilter.mIncludeFields.mFieldsRegMap.count("valid_regex"));
    APSARA_TEST_TRUE(filters.mK8SFilter.mK8sLabelFilter.mIncludeFields.mFieldsRegMap.count("valid_regex1"));
    APSARA_TEST_TRUE(filters.mK8SFilter.mK8sLabelFilter.mIncludeFields.mFieldsMap.count("invalid_regex2"));
    APSARA_TEST_TRUE(filters.mK8SFilter.mK8sLabelFilter.mIncludeFields.mFieldsMap.count("static_match"));
    APSARA_TEST_TRUE(filters.mEnvFilter.mExcludeFields.mFieldsMap.count("temp"));
}

void ContainerDiscoveryOptionsUnittest::TestEmptyConfig() const {
    unique_ptr<ContainerDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // Test completely empty config
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(Json::Value(), ctx, pluginType));
    APSARA_TEST_EQUAL(0U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(0U, config->mExternalEnvTag.size());
    APSARA_TEST_FALSE(config->mCollectingContainersMeta);

    // Test config with empty objects
    configStr = R"(
        {
            "ContainerFilters": {},
            "ExternalK8sLabelTag": {},
            "ExternalEnvTag": {},
            "CollectingContainersMeta": false
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(0U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(0U, config->mExternalEnvTag.size());
    APSARA_TEST_FALSE(config->mCollectingContainersMeta);
}

void ContainerDiscoveryOptionsUnittest::TestComplexFilterCombination() const {
    unique_ptr<ContainerDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // Test complex combination of all filter types
    configStr = R"(
        {
            "ContainerFilters": {
                "K8sNamespaceRegex": "production|staging",
                "K8sPodRegex": "web-.*|api-.*",
                "K8sContainerRegex": "nginx|tomcat",
                "IncludeK8sLabel": {
                    "app": "web",
                    "tier": "^(frontend|backend)$",
                    "version": "v2"
                },
                "ExcludeK8sLabel": {
                    "env": "test",
                    "deprecated": "true"
                },
                "IncludeEnv": {
                    "SERVICE_TYPE": "http",
                    "PORT": "^[0-9]+$"
                },
                "ExcludeEnv": {
                    "DEBUG": ".*",
                    "TEMP": ".*"
                },
                "IncludeContainerLabel": {
                    "maintainer": "platform-team",
                    "build": ".*"
                },
                "ExcludeContainerLabel": {
                    "experimental": ".*"
                }
            },
            "CollectingContainersMeta": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_TRUE(config->mCollectingContainersMeta);

    // Verify all filters are properly initialized
    APSARA_TEST_EQUAL(3U, config->mContainerFilterConfig.mIncludeK8sLabel.size());
    APSARA_TEST_EQUAL(2U, config->mContainerFilterConfig.mExcludeK8sLabel.size());
    APSARA_TEST_EQUAL(2U, config->mContainerFilterConfig.mIncludeEnv.size());
    APSARA_TEST_EQUAL(2U, config->mContainerFilterConfig.mExcludeEnv.size());
    APSARA_TEST_EQUAL(2U, config->mContainerFilterConfig.mIncludeContainerLabel.size());
    APSARA_TEST_EQUAL(1U, config->mContainerFilterConfig.mExcludeContainerLabel.size());

    // Verify regex compilation
    ContainerFilters filters;
    std::string exception;
    APSARA_TEST_TRUE(filters.Init(config->mContainerFilterConfig, exception));
    APSARA_TEST_TRUE(filters.mK8SFilter.mNamespaceReg != nullptr);
    APSARA_TEST_TRUE(filters.mK8SFilter.mPodReg != nullptr);
    APSARA_TEST_TRUE(filters.mK8SFilter.mContainerReg != nullptr);
}


void ContainerDiscoveryOptionsUnittest::TestExternalTagMapping() const {
    unique_ptr<ContainerDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // Test external tag mapping
    configStr = R"(
        {
            "ExternalK8sLabelTag": {
                "app.kubernetes.io/name": "app_name",
                "app.kubernetes.io/version": "app_version"
            },
            "ExternalEnvTag": {
                "SERVICE_NAME": "service",
                "SERVICE_PORT": "port"
            },
            "CollectingContainersMeta": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));

    APSARA_TEST_EQUAL(2U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(2U, config->mExternalEnvTag.size());
    APSARA_TEST_TRUE(config->mCollectingContainersMeta);

    // Verify tag mappings
    APSARA_TEST_EQUAL("app_name", config->mExternalK8sLabelTag["app.kubernetes.io/name"]);
    APSARA_TEST_EQUAL("app_version", config->mExternalK8sLabelTag["app.kubernetes.io/version"]);
    APSARA_TEST_EQUAL("service", config->mExternalEnvTag["SERVICE_NAME"]);
    APSARA_TEST_EQUAL("port", config->mExternalEnvTag["SERVICE_PORT"]);

    // Test invalid external tag mapping
    configStr = R"(
        {
            "ExternalK8sLabelTag": "invalid_string",
            "ExternalEnvTag": 123
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));

    // Should handle invalid types gracefully
    APSARA_TEST_EQUAL(0U, config->mExternalK8sLabelTag.size());
    APSARA_TEST_EQUAL(0U, config->mExternalEnvTag.size());
}

void ContainerDiscoveryOptionsUnittest::TestGetCustomExternalTags() const {
    unique_ptr<ContainerDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // Setup config with external tag mappings
    configStr = R"(
        {
            "ExternalK8sLabelTag": {
                "app": "application_name",
                "version": "app_version"
            },
            "ExternalEnvTag": {
                "SERVICE_NAME": "service_name",
                "PORT": "service_port"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new ContainerDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));

    // Test container info
    std::unordered_map<std::string, std::string> containerEnvs
        = {{"SERVICE_NAME", "nginx"}, {"PORT", "8080"}, {"DEBUG", "true"}};

    std::unordered_map<std::string, std::string> containerK8sLabels
        = {{"app", "web-server"}, {"version", "1.0.0"}, {"tier", "frontend"}};

    // Test GetCustomExternalTags with existing tags
    std::vector<std::pair<std::string, std::string>> existingTags = {{"existing_tag", "existing_value"}};
    config->GetCustomExternalTags(containerEnvs, containerK8sLabels, existingTags);
    APSARA_TEST_EQUAL(5U, existingTags.size());
    // Verify existing tag is preserved
    APSARA_TEST_EQUAL("existing_tag", existingTags[0].first);
    APSARA_TEST_EQUAL("existing_value", existingTags[0].second);
    // Verify new tags are added
    bool foundServiceName = false, foundServicePort = false, foundAppName = false, foundAppVersion = false;
    for (const auto& tag : existingTags) {
        if (tag.first == "service_name" && tag.second == "nginx")
            foundServiceName = true;
        else if (tag.first == "service_port" && tag.second == "8080")
            foundServicePort = true;
        else if (tag.first == "application_name" && tag.second == "web-server")
            foundAppName = true;
        else if (tag.first == "app_version" && tag.second == "1.0.0")
            foundAppVersion = true;
    }
    APSARA_TEST_TRUE(foundServiceName);
    APSARA_TEST_TRUE(foundServicePort);
    APSARA_TEST_TRUE(foundAppName);
    APSARA_TEST_TRUE(foundAppVersion);

    // Test with empty mappings
    ContainerDiscoveryOptions emptyConfig;
    std::vector<std::pair<std::string, std::string>> testTags;
    emptyConfig.GetCustomExternalTags(containerEnvs, containerK8sLabels, testTags);
    APSARA_TEST_EQUAL(0U, testTags.size());

    // Test with empty container info
    std::unordered_map<std::string, std::string> emptyEnvs;
    std::unordered_map<std::string, std::string> emptyLabels;
    std::vector<std::pair<std::string, std::string>> resultTags;
    config->GetCustomExternalTags(emptyEnvs, emptyLabels, resultTags);
    APSARA_TEST_EQUAL(0U, resultTags.size());
}

UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, TestInitWithInvalidConfig)
UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, TestContainerFilterConfigInit)
UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, TestRegexCompilation)
UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, TestEmptyConfig)
UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, TestComplexFilterCombination)
UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, TestExternalTagMapping)
UNIT_TEST_CASE(ContainerDiscoveryOptionsUnittest, TestGetCustomExternalTags)

} // namespace logtail

UNIT_TEST_MAIN
