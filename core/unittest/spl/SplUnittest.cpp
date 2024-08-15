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

#include "unittest/Unittest.h"

#include "common/JsonUtil.h"
#include "config/PipelineConfig.h"
#include "processor/ProcessorSPL.h"
#include "models/LogEvent.h"
#include "plugin/instance/ProcessorInstance.h"
#include <iostream>
#include <sstream>

namespace logtail {

static std::atomic_bool running(true);


class SplUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
    }
    PipelineContext mContext;
    Json::Value GetCastConfig(std::string spl);
    void TestInit();
    void TestWhere();
    void TestExtend();
    void TestJsonParse();
    void TestRegexParse();
    void TestRegexKV();
    void TestRegexCSV();

    void TestTag();
    //void TestMultiParse();
};

//APSARA_UNIT_TEST_CASE(SplUnittest, TestInit, 8);
APSARA_UNIT_TEST_CASE(SplUnittest, TestWhere, 0);
APSARA_UNIT_TEST_CASE(SplUnittest, TestExtend, 1);
APSARA_UNIT_TEST_CASE(SplUnittest, TestJsonParse, 2);
APSARA_UNIT_TEST_CASE(SplUnittest, TestRegexParse, 3);
APSARA_UNIT_TEST_CASE(SplUnittest, TestRegexCSV, 4);
APSARA_UNIT_TEST_CASE(SplUnittest, TestRegexKV, 5);
APSARA_UNIT_TEST_CASE(SplUnittest, TestTag, 6);
//APSARA_UNIT_TEST_CASE(SplUnittest, TestMultiParse, 7);

PluginInstance::PluginMeta getPluginMeta(){
    PluginInstance::PluginMeta pluginMeta{"testgetPluginID", "testNodeID", "testNodeChildID"};
    return pluginMeta;
}

Json::Value SplUnittest::GetCastConfig(std::string spl) {
    Json::Value config;
    config["Script"] = Json::Value(spl);
    config["TimeoutMilliSeconds"] = Json::Value(1000);
    config["MaxMemoryBytes"] = Json::Value(50*1024*1024);
    return config;
}


void SplUnittest::TestInit() {
    std::ifstream input("spl.txt");
    std::string line;

    while( std::getline( input, line ) ) {
        Json::Value config = GetCastConfig(line);

        ProcessorSPL& processor = *(new ProcessorSPL);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        
        bool init = processorInstance.Init(config, mContext);
        if (!init) {
            std::cout<<"error:" <<line<<'\n';
        }
        APSARA_TEST_TRUE(init);
    }
}


void SplUnittest::TestWhere() {
    Json::Value config = GetCastConfig("* | where content='value_3_0'");

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value_3_0"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value_4_0"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    
    std::vector<PipelineEventGroup> logGroupList;
    logGroupList.emplace_back(std::move(eventGroup));
    // run function
    ProcessorSPL& processor = *(new ProcessorSPL);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(logGroupList);

    APSARA_TEST_EQUAL((u_int)1, logGroupList.size());
    if (logGroupList.size() > 0) {
        for (auto& logGroup : logGroupList) {
            for (auto& event : logGroup.GetEvents()) {
                const LogEvent* log = event.Get<LogEvent>();
                StringView content = log->GetContent("content");
                APSARA_TEST_EQUAL("value_3_0", content);
            }
            //std::string outJson = logGroup.ToJsonString();
            //std::cout << "outJson: " << outJson << std::endl;
        }
    }

    return;
}


void SplUnittest::TestExtend() {
    // make config
    Json::Value config = GetCastConfig(R"(* | extend a=json_extract(content, '$.body.a'), b=json_extract(content, '$.body.b'))");

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "{\"body\": {\"a\": 1, \"b\": 2}}"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    
    std::vector<PipelineEventGroup> logGroupList;
    logGroupList.emplace_back(std::move(eventGroup));
    // run function
    ProcessorSPL& processor = *(new ProcessorSPL);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(logGroupList);

    APSARA_TEST_EQUAL((u_int)1, logGroupList.size());
    if (logGroupList.size() == 1) {
        auto& logGroup = logGroupList[0];
        for (auto& event : logGroup.GetEvents()) {
            const LogEvent* log = event.Get<LogEvent>();
            StringView content = log->GetContent("a");
            APSARA_TEST_EQUAL("1", content);
            content = log->GetContent("b");
            APSARA_TEST_EQUAL("2", content);
        }        
    }
    return;
}


void SplUnittest::TestJsonParse() {
    // make config
    Json::Value config = GetCastConfig("* | parse-json content ");
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "{\"a1\":\"bbbb\",\"c\":\"d\"}"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "{\"a1\":\"ccc\",\"c1\":\"d1\"}"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "tags" : {
            "__tag__": "123"
        }
    })";
    eventGroup.FromJsonString(inJson);
    
    std::vector<PipelineEventGroup> logGroupList;
    logGroupList.emplace_back(std::move(eventGroup));
    // run function
    ProcessorSPL& processor = *(new ProcessorSPL);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(logGroupList);
    APSARA_TEST_EQUAL(logGroupList.size(), 1);

    if (logGroupList.size() > 0) {
        for (auto& logGroup : logGroupList) {
            APSARA_TEST_EQUAL(2, logGroup.GetEvents().size());
            if (logGroup.GetEvents().size() == 2) {
                {
                    const LogEvent* log = logGroup.GetEvents()[0].Get<LogEvent>();
                    StringView content = log->GetContent("a1");
                    APSARA_TEST_EQUAL("bbbb", content);
                    content = log->GetContent("c");
                    APSARA_TEST_EQUAL("d", content);
                }
                {
                    const LogEvent* log = logGroup.GetEvents()[1].Get<LogEvent>();
                    StringView content = log->GetContent("a1");
                    APSARA_TEST_EQUAL("ccc", content);
                    content = log->GetContent("c1");
                    APSARA_TEST_EQUAL("d1", content);
                }
            }
        }
    }
    return;
}


void SplUnittest::TestRegexParse() {
    // make config
    Json::Value config = GetCastConfig(R"(* | parse-regexp content, '(\S+)\s+(\w+)' as ip, method)");

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "10.0.0.0 GET /index.html 15824 0.043"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "10.0.0.1 POST /index.html 15824 0.043"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "tags" : {
            "__tag__": "123"
        }
    })";
    eventGroup.FromJsonString(inJson);
    
    std::vector<PipelineEventGroup> logGroupList;
    logGroupList.emplace_back(std::move(eventGroup));
    // run function
    ProcessorSPL& processor = *(new ProcessorSPL);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(logGroupList);

    APSARA_TEST_EQUAL(logGroupList.size(), 1);


    if (logGroupList.size() > 0) {
        for (auto& logGroup : logGroupList) {
            APSARA_TEST_EQUAL(2, logGroup.GetEvents().size());
            if (logGroup.GetEvents().size() == 2) {
                {
                    const LogEvent* log = logGroup.GetEvents()[0].Get<LogEvent>();
                    StringView content = log->GetContent("ip");
                    APSARA_TEST_EQUAL("10.0.0.0", content);
                    content = log->GetContent("method");
                    APSARA_TEST_EQUAL("GET", content);
                }
                {
                    const LogEvent* log = logGroup.GetEvents()[1].Get<LogEvent>();
                    StringView content = log->GetContent("ip");
                    APSARA_TEST_EQUAL("10.0.0.1", content);
                    content = log->GetContent("method");
                    APSARA_TEST_EQUAL("POST", content);
                }
            }
        }
    }
    return;
}

void SplUnittest::TestRegexCSV() {
    // make config
    Json::Value config = GetCastConfig(R"(* | parse-csv content as x, y, z)");

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "a,b,c"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "e,f,g"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "tags" : {
            "__tag__": "123"
        }
    })";
    eventGroup.FromJsonString(inJson);
    
    std::vector<PipelineEventGroup> logGroupList;
    logGroupList.emplace_back(std::move(eventGroup));
    // run function
    ProcessorSPL& processor = *(new ProcessorSPL);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(logGroupList);

    APSARA_TEST_EQUAL(logGroupList.size(), 1);

    if (logGroupList.size() > 0) {
        for (auto& logGroup : logGroupList) {
            APSARA_TEST_EQUAL(2, logGroup.GetEvents().size());
            if (logGroup.GetEvents().size() == 2) {
                {
                    const LogEvent* log = logGroup.GetEvents()[0].Get<LogEvent>();
                    StringView content = log->GetContent("x");
                    APSARA_TEST_EQUAL("a", content);
                    content = log->GetContent("y");
                    APSARA_TEST_EQUAL("b", content);
                    content = log->GetContent("z");
                    APSARA_TEST_EQUAL("c", content);
                }
                {
                    const LogEvent* log = logGroup.GetEvents()[1].Get<LogEvent>();
                    StringView content = log->GetContent("x");
                    APSARA_TEST_EQUAL("e", content);
                    content = log->GetContent("y");
                    APSARA_TEST_EQUAL("f", content);
                    content = log->GetContent("z");
                    APSARA_TEST_EQUAL("g", content);
                }
            }
        }
    }

    return;
}



void SplUnittest::TestRegexKV() {
    // make config
    Json::Value config = GetCastConfig(R"(* | parse-kv -delims='&?' content)");

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "k1=v1&k2=v2?k3=v3"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "k11=v11&k22=v22?k33=v33"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "tags" : {
            "__tag__": "123"
        }
    })";
    eventGroup.FromJsonString(inJson);
    
    std::vector<PipelineEventGroup> logGroupList;
    logGroupList.emplace_back(std::move(eventGroup));
    // run function
    ProcessorSPL& processor = *(new ProcessorSPL);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(logGroupList);

    APSARA_TEST_EQUAL(logGroupList.size(), 1);

    if (logGroupList.size() > 0) {
        for (auto& logGroup : logGroupList) {
            APSARA_TEST_EQUAL(2, logGroup.GetEvents().size());
            if (logGroup.GetEvents().size() == 2) {
                {
                    const LogEvent* log = logGroup.GetEvents()[0].Get<LogEvent>();
                    StringView content = log->GetContent("k1");
                    APSARA_TEST_EQUAL("v1", content);
                    content = log->GetContent("k2");
                    APSARA_TEST_EQUAL("v2", content);
                    content = log->GetContent("k3");
                    APSARA_TEST_EQUAL("v3", content);
                }
                {
                    const LogEvent* log = logGroup.GetEvents()[1].Get<LogEvent>();
                    StringView content = log->GetContent("k11");
                    APSARA_TEST_EQUAL("v11", content);
                    content = log->GetContent("k22");
                    APSARA_TEST_EQUAL("v22", content);
                    content = log->GetContent("k33");
                    APSARA_TEST_EQUAL("v33", content);
                }
            }
        }
    }
    return;
}



void SplUnittest::TestTag() {
    // make config
    Json::Value config = GetCastConfig(R"(* | parse-json content | project-rename __tag__:taiye2=a1)");

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "{\"a1\":\"bbbb\",\"c\":\"d\"}"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "{\"a1\":\"cccc\",\"c\":\"d\"}"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "tags" : {
            "taiye": "123"
        }
    })";
    eventGroup.FromJsonString(inJson);
    
    std::vector<PipelineEventGroup> logGroupList;
    logGroupList.emplace_back(std::move(eventGroup));
    // run function
    ProcessorSPL& processor = *(new ProcessorSPL);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(logGroupList);

    APSARA_TEST_EQUAL(logGroupList.size(), 2);
    if (logGroupList.size() == 2) {
        {
            auto& logGroup = logGroupList[0];
            StringView tag = logGroup.GetTag("taiye2");
            APSARA_TEST_EQUAL("bbbb", tag);
            tag = logGroup.GetTag("taiye");
            APSARA_TEST_EQUAL("123", tag);

            //std::string outJson = logGroup.ToJsonString();
            //std::cout << "outJson: " << outJson << std::endl;
        }
        {
            auto& logGroup = logGroupList[1];
            StringView tag = logGroup.GetTag("taiye2");
            APSARA_TEST_EQUAL("cccc", tag);
            tag = logGroup.GetTag("taiye");
            APSARA_TEST_EQUAL("123", tag);

            //std::string outJson = logGroup.ToJsonString();
            //std::cout << "outJson: " << outJson << std::endl;
        }
    }
    
    return;
}



/*
void SplUnittest::TestMultiParse() {
    // make config
    Json::Value config = GetCastConfig(R"(.let src = * 
| parse-json content;
.let ds1 = $src
| where type = 'kv'
| parse-kv -delims='&?' message;
$ds1;
.let ds2 = $src
| where type = 'csv'
| parse-csv message as x, y, z;
$ds2;
)");

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "{\"type\":\"kv\",\"message\":\"k1=v1&k2=v2?k3=v3\"}"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "{\"type\":\"csv\",\"message\":\"a,b,c\"}"
                },
                "timestamp" : 1234567890,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "tags" : {
            "taiye": "123"
        }
    })";
    eventGroup.FromJsonString(inJson);
    
    std::vector<PipelineEventGroup> logGroupList;
    logGroupList.emplace_back(std::move(eventGroup));
    // run function
    ProcessorSPL& processor = *(new ProcessorSPL);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(logGroupList);

    APSARA_TEST_EQUAL(logGroupList.size(), 2);

    if (logGroupList.size() == 2) {
        {
            auto& logGroup = logGroupList[0];
            StringView tag = logGroup.GetTag("taiye");
            APSARA_TEST_EQUAL("123", tag);

            APSARA_TEST_EQUAL(1, logGroup.GetEvents().size());
            if (logGroup.GetEvents().size() == 1) {
                const LogEvent* log = logGroup.GetEvents()[0].Get<LogEvent>();
                StringView content = log->GetContent("k1");
                APSARA_TEST_EQUAL("v1", content);
                content = log->GetContent("k2");
                APSARA_TEST_EQUAL("v2", content);
                content = log->GetContent("k3");
                APSARA_TEST_EQUAL("v3", content);
            }

        }
        {
            auto& logGroup = logGroupList[1];
            StringView tag = logGroup.GetTag("taiye");
            APSARA_TEST_EQUAL("123", tag);

            APSARA_TEST_EQUAL(1, logGroup.GetEvents().size());
            if (logGroup.GetEvents().size() == 1) {
                const LogEvent* log = logGroup.GetEvents()[0].Get<LogEvent>();
                StringView content = log->GetContent("x");
                APSARA_TEST_EQUAL("a", content);
                content = log->GetContent("y");
                APSARA_TEST_EQUAL("b", content);
                content = log->GetContent("z");
                APSARA_TEST_EQUAL("c", content);
            }
        }
    }    
    return;
}
*/


} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}