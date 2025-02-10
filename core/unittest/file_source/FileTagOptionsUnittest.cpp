// Copyright 2024 iLogtail Authors
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

#include <json/json.h>

#include <memory>
#include <string>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "file_server/FileTagOptions.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class FileTagOptionsUnittest : public testing::Test {
public:
    void OnSuccessfulInit() const;
    void OnInvalidInit() const;

private:
    const string pluginType = "test";
    CollectionPipelineContext ctx;
};

void FileTagOptionsUnittest::OnSuccessfulInit() const {
    unique_ptr<FileTagOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // default
    configStr = R"(
        {}
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
    APSARA_TEST_EQUAL(1, config->mFileTags.size());
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_PATH_TAG_KEY], GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY));

    // AppendingLogPositionMeta
    configStr = R"(
        {
            "AppendingLogPositionMeta": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
    APSARA_TEST_EQUAL(3, config->mFileTags.size());
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_PATH_TAG_KEY], GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_OFFSET_KEY], GetDefaultTagKeyString(TagKey::FILE_OFFSET_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_INODE_TAG_KEY],
                      GetDefaultTagKeyString(TagKey::FILE_INODE_TAG_KEY));

    configStr = R"(
        {
            "AppendingLogPositionMeta": false,
            "FileOffsetKey": "test_offset",
            "Tags": {
                "FileInodeTagKey": "test_inode"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
    APSARA_TEST_EQUAL(3, config->mFileTags.size());
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_PATH_TAG_KEY], GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_OFFSET_KEY], "test_offset");
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_INODE_TAG_KEY], "test_inode");

    configStr = R"(
        {
            "AppendingLogPositionMeta": true,
            "FileOffsetKey": "test_offset",
            "Tags": {
                "FileInodeTagKey": "test_inode"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
    APSARA_TEST_EQUAL(3, config->mFileTags.size());
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_PATH_TAG_KEY], GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_OFFSET_KEY], "test_offset");
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_INODE_TAG_KEY], "test_inode");

    // container discovery
    configStr = R"(
        {
            "EnableContainerDiscovery": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, true));
    APSARA_TEST_EQUAL(7, config->mFileTags.size());
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_PATH_TAG_KEY], GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::K8S_NAMESPACE_TAG_KEY],
                      GetDefaultTagKeyString(TagKey::K8S_NAMESPACE_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::K8S_POD_NAME_TAG_KEY],
                      GetDefaultTagKeyString(TagKey::K8S_POD_NAME_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::K8S_POD_UID_TAG_KEY],
                      GetDefaultTagKeyString(TagKey::K8S_POD_UID_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::CONTAINER_NAME_TAG_KEY],
                      GetDefaultTagKeyString(TagKey::CONTAINER_NAME_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::CONTAINER_IP_TAG_KEY],
                      GetDefaultTagKeyString(TagKey::CONTAINER_IP_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::CONTAINER_IMAGE_NAME_TAG_KEY],
                      GetDefaultTagKeyString(TagKey::CONTAINER_IMAGE_NAME_TAG_KEY));

    configStr = R"(
        {
            "EnableContainerDiscovery": true,
            "Tags": {
                "K8sNamespaceTagKey": "test_namespace",
                "K8sPodNameTagKey": "test_pod_name",
                "K8sPodUidTagKey": "test_pod_uid",
                "ContainerNameTagKey": "test_container_name",
                "ContainerIpTagKey": "test_container_ip",
                "ContainerImageNameTagKey": "test_container_image_name"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, true));
    APSARA_TEST_EQUAL(7, config->mFileTags.size());
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_PATH_TAG_KEY], GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY));
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::K8S_NAMESPACE_TAG_KEY], "test_namespace");
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::K8S_POD_NAME_TAG_KEY], "test_pod_name");
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::K8S_POD_UID_TAG_KEY], "test_pod_uid");
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::CONTAINER_NAME_TAG_KEY], "test_container_name");
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::CONTAINER_IP_TAG_KEY], "test_container_ip");
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::CONTAINER_IMAGE_NAME_TAG_KEY], "test_container_image_name");
}

void FileTagOptionsUnittest::OnInvalidInit() const {
    unique_ptr<FileTagOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    configStr = R"(
        {
            "AppendingLogPositionMeta": "test"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
    APSARA_TEST_EQUAL(1, config->mFileTags.size());

    configStr = R"(
        {
            "Tags": "test"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
    APSARA_TEST_EQUAL(1, config->mFileTags.size());

    configStr = R"(
        {
            "EnableContainerDiscovery": "test"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
    APSARA_TEST_EQUAL(1, config->mFileTags.size());

    configStr = R"(
        {
            "Tags": {
                "FilePathTagKey": 1
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileTagOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
    APSARA_TEST_EQUAL(1, config->mFileTags.size());
    APSARA_TEST_EQUAL(config->mFileTags[TagKey::FILE_PATH_TAG_KEY], GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY));
}

UNIT_TEST_CASE(FileTagOptionsUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FileTagOptionsUnittest, OnInvalidInit)

} // namespace logtail

UNIT_TEST_MAIN
