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

#include <json/json.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "file_server/FileServer.h"
#include "input/InputContainerStdio.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "plugin/PluginRegistry.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(default_plugin_log_queue_size);
DECLARE_FLAG_STRING(default_container_host_path);

using namespace std;

namespace logtail {

class InputContainerStdioUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnEnableContainerDiscovery();
    void OnPipelineUpdate();
    void TestTryGetRealPath();

protected:
    static void SetUpTestCase() {
        AppConfig::GetInstance()->mPurageContainerMode = true;
        PluginRegistry::GetInstance()->LoadPlugins();
    }

    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

    void SetUp() override {
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
    }

private:
    Pipeline p;
    PipelineContext ctx;
};

void create_directory(const std::string& path) {
    size_t pos = 0;
    std::string dir;
    std::string dir_path = path;
    int mkdir_status;

    // Add trailing slash if missing
    if (dir_path[dir_path.size() - 1] != '/') {
        dir_path += '/';
    }

    while ((pos = dir_path.find_first_of('/', pos)) != std::string::npos) {
        dir = dir_path.substr(0, pos++);
        if (dir.size() == 0)
            continue; // if leading / first time is 0 length
        if ((mkdir_status = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) && errno != EEXIST) {
            std::cerr << "Error creating directory " << dir << ": " << strerror(errno) << std::endl;
            return;
        }
    }
}

void InputContainerStdioUnittest::TestTryGetRealPath() {
    std::string rootDirectory = "/tmp/home/admin";
    std::string path = "/test/test/test";
    STRING_FLAG(default_container_host_path) = "/tmp/home/admin";

    // 删除 rootDirectory 目录
    std::filesystem::remove_all(rootDirectory);
    // 删除 STRING_FLAG(default_container_host_path)
    std::filesystem::remove_all(STRING_FLAG(default_container_host_path));

    // 创建 /tmp/home/admin/test/test/test 目录
    create_directory(rootDirectory + path);
    // 创建 /tmp/home/admin/test/test/test/test.log 文件
    std::ofstream outfile(rootDirectory + path + "/test.log");
    outfile.close();

    symlink((path + "/test.log").c_str(), (rootDirectory + "/a.log").c_str());

    std::string result = InputContainerStdio::TryGetRealPath(STRING_FLAG(default_container_host_path) + "/a.log");
    APSARA_TEST_EQUAL(result, rootDirectory + path + "/test.log");
}

void InputContainerStdioUnittest::OnSuccessfulInit() {
    unique_ptr<InputContainerStdio> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    uint32_t pluginIdx = 0;

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_container_stdio",
            "IgnoringStderr": false,
            "IgnoringStdout": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputContainerStdio());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputContainerStdio::sName, "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

    // valid optional param
    configStr = R"(
        {
            "Type": "input_container_stdio",
            "EnableContainerDiscovery": true,
            "IgnoringStderr": false,
            "IgnoringStdout": true        
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputContainerStdio());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputContainerStdio::sName, "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_container_stdio",
            "EnableContainerDiscovery": "true"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputContainerStdio());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputContainerStdio::sName, "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

    // TailingAllMatchedFiles
    configStr = R"(
        {
            "Type": "input_container_stdio",
            "TailingAllMatchedFiles": true,
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputContainerStdio());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputContainerStdio::sName, "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->mFileReader.mTailingAllMatchedFiles);

    configStr = R"(
        {
            "Type": "input_container_stdio"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputContainerStdio());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputContainerStdio::sName, "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
}

void InputContainerStdioUnittest::OnEnableContainerDiscovery() {
    unique_ptr<InputContainerStdio> input;
    Json::Value configJson, optionalGoPipelineJson, optionalGoPipeline;
    string configStr, optionalGoPipelineStr, errorMsg;
    uint32_t pluginIdx = 0;

    configStr = R"(
        {
            "Type": "input_container_stdio",
            "EnableContainerDiscovery": true,
            "ContainerFilters": {
                "K8sNamespaceRegex": "default"
            }
        }
    )";
    optionalGoPipelineStr = R"(
        {
            "global": {
                "AlwaysOnline": true
            },
            "inputs": [
                {                
                    "type": "metric_container_info",
                    "detail": {
                        "K8sNamespaceRegex": "default"
                    }
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(optionalGoPipelineStr, optionalGoPipelineJson, errorMsg));
    optionalGoPipelineJson["global"]["DefaultLogQueueSize"] = Json::Value(INT32_FLAG(default_plugin_log_queue_size));
    input.reset(new InputContainerStdio());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputContainerStdio::sName, "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(optionalGoPipelineJson == optionalGoPipeline);
}

void InputContainerStdioUnittest::OnPipelineUpdate() {
    Json::Value configJson, optionalGoPipeline;
    InputContainerStdio input;
    string configStr, errorMsg;
    uint32_t pluginIdx = 0;

    configStr = R"(
        {
            "Type": "input_container_stdio"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.SetContext(ctx);
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));

    APSARA_TEST_TRUE(input.Start());
    APSARA_TEST_NOT_EQUAL(nullptr, FileServer::GetInstance()->GetFileReaderConfig("test_config").first);
    APSARA_TEST_NOT_EQUAL(nullptr, FileServer::GetInstance()->GetMultilineConfig("test_config").first);

    APSARA_TEST_TRUE(input.Stop(true));
    APSARA_TEST_EQUAL(nullptr, FileServer::GetInstance()->GetFileReaderConfig("test_config").first);
    APSARA_TEST_EQUAL(nullptr, FileServer::GetInstance()->GetMultilineConfig("test_config").first);
}

UNIT_TEST_CASE(InputContainerStdioUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputContainerStdioUnittest, OnEnableContainerDiscovery)
UNIT_TEST_CASE(InputContainerStdioUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(InputContainerStdioUnittest, TestTryGetRealPath)

} // namespace logtail

UNIT_TEST_MAIN
