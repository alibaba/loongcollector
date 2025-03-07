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

#include <vector>

#include "Constants.h"
#include "TagConstants.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "file_server/reader/LogFileReader.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class FileTagUnittest : public testing::Test {
public:
    void TestDefaultTag();
    void TestRenameTag();
    void TestDeleteTag();

protected:
    void SetUp() override {}

    void TearDown() override {}

private:
    vector<pair<TagKey, string>> GenerateFakeContainerMetadatas() {
        vector<pair<TagKey, string>> metadata;
        metadata.emplace_back(TagKey::K8S_NAMESPACE_TAG_KEY, "test_namespace");
        metadata.emplace_back(TagKey::K8S_POD_NAME_TAG_KEY, "test_pod");
        metadata.emplace_back(TagKey::K8S_POD_UID_TAG_KEY, "test_pod_uid");
        metadata.emplace_back(TagKey::CONTAINER_IMAGE_NAME_TAG_KEY, "test_image");
        metadata.emplace_back(TagKey::CONTAINER_NAME_TAG_KEY, "test_container");
        metadata.emplace_back(TagKey::CONTAINER_IP_TAG_KEY, "test_container_ip");
        return metadata;
    }

    vector<pair<string, string>> GenerateFakeContainerExtraTags() {
        vector<pair<string, string>> extraTags;
        extraTags.emplace_back("_test_tag_", "test_value");
        return extraTags;
    }

    FileReaderOptions readerOpts;
    MultilineOptions multilineOpts;
    CollectionPipelineContext ctx;
    string hostLogPathDir = ".";
    string hostLogPathFile = "FileTagUnittest.txt";
    const string pluginType = "test";
};

void FileTagUnittest::TestDefaultTag() {
    unique_ptr<FileTagOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;
    {
        configStr = R"(
            {}
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 1);
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY)),
                          hostLogPathDir + PATH_SEPARATOR + hostLogPathFile);
    }
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": true,
                "EnableContainerDiscovery": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, true));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};
        reader.mContainerMetadatas = GenerateFakeContainerMetadatas();
        reader.mContainerExtraTags = GenerateFakeContainerExtraTags();

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY),
                          GetDefaultTagKeyString(TagKey::FILE_OFFSET_KEY));

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 12);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY)),
                          hostLogPathDir + PATH_SEPARATOR + hostLogPathFile);
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_INODE_TAG_KEY)), "0");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::K8S_POD_NAME_TAG_KEY)), "test_pod");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::K8S_NAMESPACE_TAG_KEY)), "test_namespace");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::K8S_POD_UID_TAG_KEY)), "test_pod_uid");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::CONTAINER_IMAGE_NAME_TAG_KEY)),
                          "test_image");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::CONTAINER_NAME_TAG_KEY)), "test_container");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::CONTAINER_IP_TAG_KEY)), "test_container_ip");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
        APSARA_TEST_EQUAL(eventGroup.GetTag("_test_tag_"), "test_value");
    }
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": false,
                "FileOffsetKey": "__default__",
                "Tags": {
                    "FilePathTagKey": "__default__",
                    "FileInodeTagKey": "__default__"
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY),
                          GetDefaultTagKeyString(TagKey::FILE_OFFSET_KEY));

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 5);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY)),
                          hostLogPathDir + PATH_SEPARATOR + hostLogPathFile);
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_INODE_TAG_KEY)), "0");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
    }
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": true,
                "FileOffsetKey": "__default__",
                "Tags": {
                    "FilePathTagKey": "__default__",
                    "FileInodeTagKey": "__default__"
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY),
                          GetDefaultTagKeyString(TagKey::FILE_OFFSET_KEY));

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 5);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY)),
                          hostLogPathDir + PATH_SEPARATOR + hostLogPathFile);
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_INODE_TAG_KEY)), "0");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
    }
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": false,
                "EnableContainerDiscovery": true,
                "FileOffsetKey": "__default__",
                "Tags": {
                    "FilePathTagKey": "__default__",
                    "FileInodeTagKey": "__default__",
                    "K8sNamespaceTagKey": "__default__",
                    "K8sPodNameTagKey": "__default__",
                    "K8sPodUidTagKey": "__default__",
                    "ContainerNameTagKey": "__default__",
                    "ContainerIpTagKey": "__default__",
                    "ContainerImageNameTagKey": "__default__"
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, true));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};
        reader.mContainerMetadatas = GenerateFakeContainerMetadatas();
        reader.mContainerExtraTags = GenerateFakeContainerExtraTags();

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY),
                          GetDefaultTagKeyString(TagKey::FILE_OFFSET_KEY));

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 12);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_PATH_TAG_KEY)),
                          hostLogPathDir + PATH_SEPARATOR + hostLogPathFile);
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::FILE_INODE_TAG_KEY)), "0");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::CONTAINER_IMAGE_NAME_TAG_KEY)),
                          "test_image");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::CONTAINER_NAME_TAG_KEY)), "test_container");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::CONTAINER_IP_TAG_KEY)), "test_container_ip");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::K8S_POD_NAME_TAG_KEY)), "test_pod");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::K8S_NAMESPACE_TAG_KEY)), "test_namespace");
        APSARA_TEST_EQUAL(eventGroup.GetTag(GetDefaultTagKeyString(TagKey::K8S_POD_UID_TAG_KEY)), "test_pod_uid");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
        APSARA_TEST_EQUAL(eventGroup.GetTag("_test_tag_"), "test_value");
    }
}

void FileTagUnittest::TestRenameTag() {
    unique_ptr<FileTagOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": false,
                "FileOffsetKey": "test_offset",
                "Tags": {
                    "FilePathTagKey": "test_path",
                    "FileInodeTagKey": "test_inode"
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY), "test_offset");

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 5);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_path"), hostLogPathDir + PATH_SEPARATOR + hostLogPathFile);
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_inode"), "0");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
    }
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": true,
                "FileOffsetKey": "test_offset",
                "Tags": {
                    "FilePathTagKey": "test_path",
                    "FileInodeTagKey": "test_inode"
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY), "test_offset");

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 5);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_path"), hostLogPathDir + PATH_SEPARATOR + hostLogPathFile);
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_inode"), "0");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
    }
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": false,
                "EnableContainerDiscovery": true,
                "FileOffsetKey": "test_offset",
                "Tags": {
                    "FilePathTagKey": "test_path",
                    "FileInodeTagKey": "test_inode",
                    "K8sNamespaceTagKey": "test_namespace",
                    "K8sPodNameTagKey": "test_pod",
                    "K8sPodUidTagKey": "test_pod_uid",
                    "ContainerNameTagKey": "test_container",
                    "ContainerIpTagKey": "test_container_ip",
                    "ContainerImageNameTagKey": "test_image"
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, true));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};
        reader.mContainerMetadatas = GenerateFakeContainerMetadatas();
        reader.mContainerExtraTags = GenerateFakeContainerExtraTags();

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY), "test_offset");

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 12);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_path"), hostLogPathDir + PATH_SEPARATOR + hostLogPathFile);
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_inode"), "0");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_namespace"), "test_namespace");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_pod"), "test_pod");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_pod_uid"), "test_pod_uid");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_image"), "test_image");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_container"), "test_container");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_container_ip"), "test_container_ip");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
        APSARA_TEST_EQUAL(eventGroup.GetTag("_test_tag_"), "test_value");
    }
}

void FileTagUnittest::TestDeleteTag() {
    unique_ptr<FileTagOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": false,
                "FileOffsetKey": "",
                "Tags": {
                    "FilePathTagKey": "",
                    "FileInodeTagKey": ""
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 3);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
    }
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": true,
                "FileOffsetKey": "",
                "Tags": {
                    "FilePathTagKey": "",
                    "FileInodeTagKey": ""
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, false));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 3);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
    }
    {
        configStr = R"(
            {
                "AppendingLogPositionMeta": false,
                "EnableContainerDiscovery": true,
                "Tags": {
                    "FilePathTagKey": "",
                    "K8sNamespaceTagKey": "",
                    "K8sPodNameTagKey": "",
                    "K8sPodUidTagKey": "",
                    "ContainerNameTagKey": "",
                    "ContainerIpTagKey": "",
                    "ContainerImageNameTagKey": ""
                }
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        config.reset(new FileTagOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType, true));
        LogFileReader reader = LogFileReader(hostLogPathDir,
                                             hostLogPathFile,
                                             DevInode(),
                                             make_pair(&readerOpts, &ctx),
                                             make_pair(&multilineOpts, &ctx),
                                             make_pair(config.get(), &ctx));
        reader.mTopicName = "test_topic";
        reader.mTopicExtraTags = {{"test_topic_1", "test_topic_value_1"}, {"test_topic_2", "test_topic_value_2"}};
        reader.mContainerMetadatas = GenerateFakeContainerMetadatas();
        reader.mContainerExtraTags = GenerateFakeContainerExtraTags();

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        reader.SetEventGroupMetaAndTag(eventGroup);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");

        APSARA_TEST_EQUAL(eventGroup.GetTags().size(), 4);
        APSARA_TEST_EQUAL(eventGroup.GetTag(LOG_RESERVED_KEY_TOPIC), "test_topic");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_1"), "test_topic_value_1");
        APSARA_TEST_EQUAL(eventGroup.GetTag("test_topic_2"), "test_topic_value_2");
        APSARA_TEST_EQUAL(eventGroup.GetTag("_test_tag_"), "test_value");
    }
}

UNIT_TEST_CASE(FileTagUnittest, TestDefaultTag)
UNIT_TEST_CASE(FileTagUnittest, TestRenameTag)
UNIT_TEST_CASE(FileTagUnittest, TestDeleteTag)


} // namespace logtail

UNIT_TEST_MAIN
