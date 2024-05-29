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

#include "unittest/Unittest.h"

#include <string>

#include "common/YamlUtil.h"

namespace logtail {

class YamlUtilUnittest : public ::testing::Test {
public:
    void TestEmptyYaml();
    void TestInvalidYaml();
    void TestNestedYaml();
    void TestDifferentTypesYaml();
    void TestMultiLevelListYaml();
    void TestNestedNonScalarYaml();
    void TestCommentYaml();
    void TestYamlSpecialMeaningChars();
    void TestMultiLineStringYaml();
    void TestDifferentStyleStringsYaml();
    void TestSpecialKeyValueYaml();
    void TestSpecialBooleanYaml();
    void TestVariousNumberYaml();
    void TestTimeAndDateYaml();
    void TestUnicodeYaml();
    void TestAnchorAndAliasYaml();
    void TestCycleDependenciesYaml();
};

void YamlUtilUnittest::TestEmptyYaml() {
    {
        std::string yaml = "";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_TRUE_FATAL(json.isNull());
    }
    {
        std::string yaml = R"(
            a: []
            b: {}
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_TRUE_FATAL(json["a"].empty());
        APSARA_TEST_TRUE_FATAL(json["a"].isArray());
        APSARA_TEST_TRUE_FATAL(json["b"].empty());
        APSARA_TEST_TRUE_FATAL(json["b"].isObject());
    }
}

void YamlUtilUnittest::TestInvalidYaml() {
    std::string yaml = R"(
            a: 1
            b: 2
            c: 3
            d: e: f
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    EXPECT_FALSE(ret);
    APSARA_TEST_EQUAL_FATAL(errorMsg, "parse yaml failed: yaml-cpp: error at line 5, column 17: illegal map value");
}

void YamlUtilUnittest::TestNestedYaml() {
    {
        std::string yaml = R"(
            a: 
                b: 2
                c: 3
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_EQUAL_FATAL(json["a"]["b"].asInt(), 2);
        APSARA_TEST_EQUAL_FATAL(json["a"]["c"].asInt(), 3);
    }
    {
        std::string yaml = R"(
            a: 
                b: 
                    c: 3
                    d: 4
                e: 5
            f:
                g: 6
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_EQUAL_FATAL(json["a"]["b"]["c"].asInt(), 3);
        APSARA_TEST_EQUAL_FATAL(json["a"]["b"]["d"].asInt(), 4);
        APSARA_TEST_EQUAL_FATAL(json["a"]["e"].asInt(), 5);
        APSARA_TEST_EQUAL_FATAL(json["f"]["g"].asInt(), 6);
    }
}

void YamlUtilUnittest::TestDifferentTypesYaml() {
    {
        std::string yaml = R"(
            a: 1
            b: 2.2
            c: "3"
            d: true
            e: [1, 2, 3]
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_EQUAL_FATAL(json["a"].asInt(), 1);
        APSARA_TEST_EQUAL_FATAL(json["b"].asDouble(), 2.2);
        APSARA_TEST_EQUAL_FATAL(json["c"].asString(), "3");
        APSARA_TEST_EQUAL_FATAL(json["d"].asBool(), true);
        APSARA_TEST_TRUE_FATAL(json["e"].isArray());
        APSARA_TEST_EQUAL_FATAL(json["e"][0].asInt(), 1);
        APSARA_TEST_EQUAL_FATAL(json["e"][1].asInt(), 2);
        APSARA_TEST_EQUAL_FATAL(json["e"][2].asInt(), 3);
    }
    {
        std::string yaml = R"(
            a: null
            b: .NaN
            c: .inf
            d: -.inf
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_TRUE_FATAL(json["a"].isNull());
        APSARA_TEST_TRUE_FATAL(std::isnan(json["b"].asDouble()));
        APSARA_TEST_TRUE_FATAL(std::isinf(json["c"].asDouble()) && json["c"].asDouble() > 0);
        APSARA_TEST_TRUE_FATAL(std::isinf(json["d"].asDouble()) && json["d"].asDouble() < 0);
    }
}

void YamlUtilUnittest::TestMultiLevelListYaml() {
    std::string yaml = R"(
            - a
            - b
            - 
                - c
                - d
            - e
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json[0].asString(), "a");
    APSARA_TEST_EQUAL_FATAL(json[1].asString(), "b");
    APSARA_TEST_EQUAL_FATAL(json[2][0].asString(), "c");
    APSARA_TEST_EQUAL_FATAL(json[2][1].asString(), "d");
    APSARA_TEST_EQUAL_FATAL(json[3].asString(), "e");
}

void YamlUtilUnittest::TestNestedNonScalarYaml() {
    std::string yaml = R"(
            a:
                - b
                - c:
                    - d
                    - e
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"][0].asString(), "b");
    APSARA_TEST_EQUAL_FATAL(json["a"][1]["c"][0].asString(), "d");
    APSARA_TEST_EQUAL_FATAL(json["a"][1]["c"][1].asString(), "e");
}

void YamlUtilUnittest::TestCommentYaml() {
    std::string yaml = R"(
            a: 1 # this is a comment
            b: 2
            # this is another comment
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"].asInt(), 1);
    APSARA_TEST_EQUAL_FATAL(json["b"].asInt(), 2);
}

void YamlUtilUnittest::TestYamlSpecialMeaningChars() {
    std::string yaml = R"(
            a: "---"
            b: "..."
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "---");
    APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "...");
}

void YamlUtilUnittest::TestMultiLineStringYaml() {
    std::string yaml = R"(
            a: |
                This is a
                multi-line std::string.
            b: >
                This is another
                multi-line std::string.
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "This is a\nmulti-line std::string.\n");
    APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "This is another multi-line std::string.\n");
}

void YamlUtilUnittest::TestDifferentStyleStringsYaml() {
    {
        std::string yaml = R"(
            a: "\t\n\r\b\f"
            b: "\"\'\\"
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "\t\n\r\b\f");
        APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "\"\'\\");
    }
    {
        std::string yaml = R"(
            a: bare std::string \t
            b: 'single quoted std::string \t'
            c: "double quoted std::string \t"
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "bare std::string \\t");
        APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "single quoted std::string \\t");
        APSARA_TEST_EQUAL_FATAL(json["c"].asString(), "double quoted std::string \t");
    }
    {
        std::string yaml = R"(
            a: "1\n2"
            b: "3\t4"
            c: "\u0001"
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_TRUE_FATAL(ret);
        APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "1\n2");
        APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "3\t4");
        APSARA_TEST_EQUAL_FATAL(json["c"].asString(), "\u0001");
    }
    {
        std::string yaml = R"(
            a: 1\n2
            b: 3\t4
            c: \u0001
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "1\\n2");
        APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "3\\t4");
        APSARA_TEST_EQUAL_FATAL(json["c"].asString(), "\\u0001");
    }
    {
        std::string yaml = R"(
            a: '1\n2'
            b: '3\t4'
            c: '\u0001'
        )";
        Json::Value json;
        std::string errorMsg;
        YAML::Node yamlRoot;
        bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
        APSARA_TEST_TRUE_FATAL(ret);
        json = ConvertYamlToJson(yamlRoot);
        APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "1\\n2");
        APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "3\\t4");
        APSARA_TEST_EQUAL_FATAL(json["c"].asString(), "\\u0001");
    }
}

void YamlUtilUnittest::TestSpecialKeyValueYaml() {
    std::string yaml = R"(
            "a:b": "c,d"
            "[e]": "{f}"
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a:b"].asString(), "c,d");
    APSARA_TEST_EQUAL_FATAL(json["[e]"].asString(), "{f}");
}

void YamlUtilUnittest::TestSpecialBooleanYaml() {
    std::string yaml = R"(
            a: yes
            b: no
            c: on
            d: off
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"].asBool(), true);
    APSARA_TEST_EQUAL_FATAL(json["b"].asBool(), false);
    APSARA_TEST_EQUAL_FATAL(json["c"].asBool(), true);
    APSARA_TEST_EQUAL_FATAL(json["d"].asBool(), false);
}

void YamlUtilUnittest::TestVariousNumberYaml() {
    std::string yaml = R"(
            a: 12345
            b: 0x12345
            c: 012345
            d: 1.2345e+3
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"].asInt(), 12345);
    APSARA_TEST_EQUAL_FATAL(json["b"].asInt(), 0x12345);
    APSARA_TEST_EQUAL_FATAL(json["c"].asInt(), 012345);
    APSARA_TEST_EQUAL_FATAL(json["d"].asDouble(), 1.2345e+3);
}

void YamlUtilUnittest::TestTimeAndDateYaml() {
    std::string yaml = R"(
            a: 2001-12-15T02:59:43.1Z
            b: 2002-12-14
            c: 23:59:59.999
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "2001-12-15T02:59:43.1Z");
    APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "2002-12-14");
    APSARA_TEST_EQUAL_FATAL(json["c"].asString(), "23:59:59.999");
}

void YamlUtilUnittest::TestUnicodeYaml() {
    std::string yaml = R"(
            a: "Schrödinger's cat"
            b: "\u4E2D\u6587"
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"].asString(), "Schrödinger's cat");
    APSARA_TEST_EQUAL_FATAL(json["b"].asString(), "中文");
}

void YamlUtilUnittest::TestAnchorAndAliasYaml() {
    std::string yaml = R"(
            a: &anchor
                b: 1
                c: 2
            d: *anchor
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    APSARA_TEST_TRUE_FATAL(ret);
    json = ConvertYamlToJson(yamlRoot);
    APSARA_TEST_EQUAL_FATAL(json["a"]["b"].asInt(), 1);
    APSARA_TEST_EQUAL_FATAL(json["a"]["c"].asInt(), 2);
    APSARA_TEST_EQUAL_FATAL(json["d"]["b"].asInt(), 1);
    APSARA_TEST_EQUAL_FATAL(json["d"]["c"].asInt(), 2);
}

void YamlUtilUnittest::TestCycleDependenciesYaml() {
    std::string yaml = R"(
            global:
              EnableTimestampNanosecond: false
            inputs:
            - Type: input_file
              FilePaths:
              - /root/zhl/als/testjson.log
              MaxDirSearchDepth: 5
              ExcludeFilePaths: []
              TailSizeKB: 0
              AllowingIncludedByMultiConfigs: &id004
                enable: *id004
                global:
                  EnableTimestampNanosecond: false
                  inputs:
                  - Type: input_file
                    FilePaths:
                    - /root/zhl/als/duohangzhengze.log
                    MaxDirSearchDepth: 5
                    ExcludeFilePaths: []
                    TailSizeKB: 0
                    AllowingIncludedByMultiConfigs: *id004
        )";
    Json::Value json;
    std::string errorMsg;
    YAML::Node yamlRoot;
    bool ret = ParseYamlTable(yaml, yamlRoot, errorMsg);
    EXPECT_FALSE(ret);
    APSARA_TEST_EQUAL_FATAL(errorMsg, "yaml file contains cycle dependencies.");
}

UNIT_TEST_CASE(YamlUtilUnittest, TestEmptyYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestInvalidYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestNestedYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestDifferentTypesYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestMultiLevelListYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestNestedNonScalarYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestCommentYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestYamlSpecialMeaningChars);
UNIT_TEST_CASE(YamlUtilUnittest, TestMultiLineStringYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestDifferentStyleStringsYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestSpecialKeyValueYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestSpecialBooleanYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestVariousNumberYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestTimeAndDateYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestUnicodeYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestAnchorAndAliasYaml);
UNIT_TEST_CASE(YamlUtilUnittest, TestCycleDependenciesYaml);
} // namespace logtail

UNIT_TEST_MAIN
