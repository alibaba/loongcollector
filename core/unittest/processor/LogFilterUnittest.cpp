// Copyright 2022 iLogtail Authors
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
#if defined(__linux__)
#include <unistd.h>
#endif
#include "app_config/AppConfig.h"
#include "processor/LogFilter.h"
#include "common/util.h"
#include "common/Flags.h"
#include "common/StringTools.h"
#include "logger/Logger.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <functional>
#include "config_manager/ConfigManager.h"
#include "controller/EventDispatcher.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "sender/Sender.h"
#include "log_pb/sls_logs.pb.h"
#include "common/FileSystemUtil.h"
#include <re2/re2.h>
#include "config/Config.h"
#include "config/UserLogConfigParser.h"
#include "common/LogGroupContext.h"


using namespace std;
using namespace sls_logs;
using boost::regex;

DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_STRING(user_log_config);

namespace logtail {

class LogFilterUnittest : public ::testing::Test {
    // SetConfigFilePattern sets the file_pattern field for @configJson.
    // On Windows, bracket pattern is not supported.
    static void SetConfigFilePattern(Json::Value& configJson) {
#if defined(__linux__)
        configJson["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
#elif defined(_MSC_VER)
        configJson["file_pattern"] = Json::Value("*.log");
#endif
    }

public:
    static void SetUpTestCase() //void Setup()
    {
        srand(time(NULL));
    }

    void* RunningDispatcher() {
        EventDispatcher::GetInstance()->Dispatch();
        return NULL;
    }

    Config* GetCastSensWordConfig(string key = string("cast1"),
                                  string regex = string("(pwd=)[^,]+"),
                                  int type = SensitiveWordCastOption::CONST_OPTION,
                                  bool replaceAll = false,
                                  string constVal = string("\\1********")) {
        Config* oneConfig = new Config;
        vector<SensitiveWordCastOption>& optVec = oneConfig->mSensitiveWordCastOptions[key];

        optVec.resize(1);
        optVec[0].option = SensitiveWordCastOption::CONST_OPTION;
        optVec[0].key = key;
        optVec[0].constValue = constVal;
        optVec[0].replaceAll = replaceAll;
        optVec[0].mRegex.reset(new re2::RE2(regex));
        return oneConfig;
    }

    void TestParseCastSensWordConfig() {
        LOG_INFO(sLogger, ("TestParseCastSensWordConfig() begin", time(NULL)));

        Json::Value constVal;
        constVal["key"] = Json::Value("param1");
        constVal["type"] = Json::Value("const");
        constVal["regex_begin"] = Json::Value("pwd=");
        constVal["regex_content"] = Json::Value("[^,]+");
        constVal["const"] = Json::Value("********");
        constVal["all"] = Json::Value(false);
        Json::Value md5Val;
        md5Val["key"] = Json::Value("param2");
        md5Val["type"] = Json::Value("md5");
        md5Val["regex_begin"] = Json::Value("\\d{6}");
        md5Val["regex_content"] = Json::Value("\\d{8}");
        md5Val["all"] = Json::Value(true);
        Json::Value allVal;
        allVal.append(constVal);
        allVal.append(md5Val);


        Config* pConfig = new Config;
        ConfigManager::GetInstance()->GetSensitiveKeys(allVal, pConfig);

        APSARA_TEST_EQUAL((size_t)2, pConfig->mSensitiveWordCastOptions.size());

        std::vector<SensitiveWordCastOption>& param1Vec = pConfig->mSensitiveWordCastOptions["param1"];
        std::vector<SensitiveWordCastOption>& param2Vec = pConfig->mSensitiveWordCastOptions["param2"];

        APSARA_TEST_EQUAL(param1Vec[0].key, "param1");
        APSARA_TEST_EQUAL(param1Vec[0].constValue, "\\1********");
        auto CONST_OPTION = SensitiveWordCastOption::CONST_OPTION;
        APSARA_TEST_EQUAL(param1Vec[0].option, CONST_OPTION);
        APSARA_TEST_EQUAL(param1Vec[0].replaceAll, false);
        APSARA_TEST_EQUAL(param1Vec[0].mRegex->ok(), true);


        APSARA_TEST_EQUAL(param2Vec[0].key, "param2");
        auto MD5_OPTION = SensitiveWordCastOption::MD5_OPTION;
        APSARA_TEST_EQUAL(param2Vec[0].option, MD5_OPTION);
        APSARA_TEST_EQUAL(param2Vec[0].replaceAll, true);
        APSARA_TEST_EQUAL(param2Vec[0].mRegex->ok(), true);

        LOG_INFO(sLogger, ("TestParseCastSensWordConfig() end", time(NULL)));
    }

    void TestCastSensWordConst() {
        LOG_INFO(sLogger, ("TestCastSensWordConst() begin", time(NULL)));
        Config* pConfig = GetCastSensWordConfig();
        Log_Content content;
        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,", content.value());


        content.set_key("cast1");
        content.set_value("\r\n\r\nasf@@\n\n@324 FS2$%pwd,pwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "\r\n\r\nasf@@\n\n@324 FS2$%pwd,pwd=********,,", content.value());

        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,pwd=12341,df", content.value());


        pConfig->mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;

        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,pwd=********,df", content.value());


        pConfig->mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;

        content.set_key("cast1");
        // psw has psw
        content.set_value("asf@@@324 FS2$%pwd,pwd=sdfpsw=543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,pwd=********,df", content.value());


        delete pConfig;
        LOG_INFO(sLogger, ("TestCastSensWordConst() end", time(NULL)));
    }


    void TestCastSensWordMD5() {
        LOG_INFO(sLogger, ("TestCastSensWordMD5() begin", time(NULL)));
        Config* pConfig = GetCastSensWordConfig();
        pConfig->mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;

        Log_Content content;
        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(
            content.value(), string("asf@@@324 FS2$%pwd,pwd=") + sdk::CalcMD5("saf543#$@") + ",,", content.value());

        content.set_key("cast1");
        content.set_value("pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(
            content.value(), string("pwd=") + sdk::CalcMD5("saf543#$@") + ",,pwd=12341,df", content.value());

        content.set_key("cast1");
        content.set_value("pwdsaf543#$@,,pwd=12341");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), string("pwdsaf543#$@,,pwd=") + sdk::CalcMD5("12341"), content.value());

        content.set_key("cast1");
        content.set_value("pwd=12341");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), string("pwd=") + sdk::CalcMD5("12341"), content.value());

        pConfig->mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;

        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(),
                               string("asf@@@324 FS2$%pwd,pwd=") + sdk::CalcMD5("saf543#$@")
                                   + ",,pwd=" + sdk::CalcMD5("12341") + ",df",
                               content.value());

        content.set_key("cast1");
        content.set_value("pwd=saf543#$@,,pwd=12341f");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(),
                               string("pwd=") + sdk::CalcMD5("saf543#$@") + ",,pwd=" + sdk::CalcMD5("12341f"),
                               content.value());

        content.set_key("cast1");
        content.set_value("pwd=saf543#$@,,pwd=12341f,asfasf");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(),
                               string("pwd=") + sdk::CalcMD5("saf543#$@") + ",,pwd=" + sdk::CalcMD5("12341f")
                                   + ",asfasf",
                               content.value());

        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(
            content.value(), string("asf@@@324 FS2$%pwd,pwd=") + sdk::CalcMD5("saf543#$@") + ",,", content.value());


        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,\npwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(
            content.value(), string("asf@@@324 FS2$%pwd,\npwd=") + sdk::CalcMD5("saf543#$@") + ",,", content.value());

        delete pConfig;
        LOG_INFO(sLogger, ("TestCastSensWordMD5() end", time(NULL)));
    }


    void TestCastSensWordFail() {
        LOG_INFO(sLogger, ("TestCastSensWordFail() begin", time(NULL)));
        Config* pConfig = GetCastSensWordConfig();
        Log_Content content;
        // no this key
        content.set_key("cast0");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_NOT_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,", content.value());
        ;

        content.set_key("123214");
        content.set_value("asf@@@324 FS2$%psw,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_NOT_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,pwd=12341,df", content.value());

        pConfig->mSensitiveWordCastOptions.begin()->second[0].constValue = "********";
        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,********,,pwd=12341,df", content.value());


        pConfig->mSensitiveWordCastOptions.begin()->second[0].constValue = "\\2********";
        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,********,,pwd=12341,df", content.value());


        pConfig->mSensitiveWordCastOptions.begin()->second[0].mRegex.reset(new re2::RE2("pwd=[^,]+"));
        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,********,,pwd=12341,df", content.value());

        // this should not happen
        pConfig->mSensitiveWordCastOptions.begin()->second[0].mRegex.reset(new re2::RE2(""));
        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(
            content.value(), "********asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df", content.value());

        delete pConfig;
        LOG_INFO(sLogger, ("TestCastSensWordFail() end", time(NULL)));
    }


    void TestCastSensWordLoggroup() {
        LOG_INFO(sLogger, ("TestCastSensWordLoggroup() begin", time(NULL)));
        Config* pConfig = GetCastSensWordConfig();
        vector<SensitiveWordCastOption>& optVec = pConfig->mSensitiveWordCastOptions["id"];
        vector<SensitiveWordCastOption>& cntVec = pConfig->mSensitiveWordCastOptions["content"];

        optVec.resize(1);
        optVec[0].option = SensitiveWordCastOption::CONST_OPTION;
        optVec[0].key = "id";
        optVec[0].constValue = "\\1********";
        optVec[0].replaceAll = true;
        optVec[0].mRegex.reset(new re2::RE2("(\\d{6})\\d{8}"));

        cntVec.resize(1);
        cntVec[0].option = SensitiveWordCastOption::CONST_OPTION;
        cntVec[0].key = "content";
        cntVec[0].constValue = "\\1********";
        cntVec[0].replaceAll = true;
        cntVec[0].mRegex.reset(new re2::RE2("('password':')[^']*"));

        LogGroup logGroup;

        Log* pLog = logGroup.add_logs();

        Log_Content* pCt1 = pLog->add_contents();
        pCt1->set_key("user");
        pCt1->set_value("ali-sls-logtail");
        Log_Content* pCt2 = pLog->add_contents();
        pCt2->set_key("cast1");
        pCt2->set_value("pwd=donottellanyone!,");
        Log_Content* pCt3 = pLog->add_contents();
        pCt3->set_key("id");
        pCt3->set_value("33032119850506123X");
        Log_Content* pCt7 = pLog->add_contents();
        pCt7->set_key("content");
        pCt7->set_value("{'account':'18122100036969','password':'04adf38'};akProxy=null;");


        Log* pLog2 = logGroup.add_logs();
        Log_Content* pCt4 = pLog2->add_contents();
        pCt4->set_key("user");
        pCt4->set_value("ali-sls-logtail1");
        Log_Content* pCt5 = pLog2->add_contents();
        pCt5->set_key("cast1");
        pCt5->set_value("pwd=dafddasf@@!123!,");
        Log_Content* pCt6 = pLog2->add_contents();
        pCt6->set_key("id");
        pCt6->set_value("33032119891206123X");


        LogFilter::CastSensitiveWords(logGroup, pConfig);

        APSARA_TEST_EQUAL(pCt1->value(), "ali-sls-logtail");
        APSARA_TEST_EQUAL(pCt2->value(), "pwd=********,");
        APSARA_TEST_EQUAL(pCt3->value(), "330321********123X");
        APSARA_TEST_EQUAL(pCt4->value(), "ali-sls-logtail1");
        APSARA_TEST_EQUAL(pCt5->value(), "pwd=********,");
        APSARA_TEST_EQUAL(pCt6->value(), "330321********123X");
        APSARA_TEST_EQUAL(pCt7->value(), "{'account':'18122100036969','password':'********'};akProxy=null;");


        delete pConfig;

        LOG_INFO(sLogger, ("TestCastSensWordLoggroup() end", time(NULL)));
    }


    void TestCastSensWordMulti() {
        LOG_INFO(sLogger, ("TestCastSensWordMulti() begin", time(NULL)));
        Config* pConfig = GetCastSensWordConfig();
        Log_Content content;
        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,", content.value());


        content.set_key("cast1");
        content.set_value("\r\n\r\nasf@@\n\n@324 FS2$%pwd,pwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "\r\n\r\nasf@@\n\n@324 FS2$%pwd,pwd=********,,", content.value());

        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,pwd=12341,df", content.value());


        pConfig->mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;

        content.set_key("cast1");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,pwd=********,df", content.value());


        pConfig->mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;

        content.set_key("cast1");
        // psw has psw
        content.set_value("asf@@@324 FS2$%pwd,pwd=sdfpsw=543#$@,,pwd=12341,df");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "asf@@@324 FS2$%pwd,pwd=********,,pwd=********,df", content.value());


        delete pConfig;
        LOG_INFO(sLogger, ("TestCastSensWordMulti() end", time(NULL)));
    }

    void TestCastWholeKey() {
        LOG_INFO(sLogger, ("TestCastWholeKey() begin", time(NULL)));
        Config* pConfig = GetCastSensWordConfig("pwd", "().*", 1, false, "\\1********");
        Log_Content content;
        content.set_key("pwd");
        content.set_value("asf@@@324 FS2$%pwd,pwd=saf543#$@,,");
        LogFilter::CastOneSensitiveWord(&content, pConfig);
        APSARA_TEST_EQUAL_DESC(content.value(), "********", content.value());
        delete pConfig;
        LOG_INFO(sLogger, ("TestCastWholeKey() end", time(NULL)));
    }

    void TestInitFilter() {
        LOG_INFO(sLogger, ("TestInitFilter() begin", time(NULL)));
        static LogFilter* filterPtr = LogFilter::Instance();
        string config
            = "{\"filters\" : [{\"project_name\" : \"123_proj\", \"category\" : \"test\", \"keys\" : [\"key1\", "
              "\"key2\"], \"regs\" : [\"value1\",\"value2\"]}, {\"project_name\" : \"456_proj\", \"category\" : "
              "\"test_1\", \"keys\" : [\"key1\", \"key2\"], \"regs\" : [\"value1\",\"value2\"]}]}";
        string path = GetProcessExecutionDir() + "logtail_filter.json";
        ofstream file(path.c_str());
        file << config;
        file.close();
        APSARA_TEST_TRUE_DESC(filterPtr->InitFilter("logtail_filter.json"), "Can't parse the filter config");
        APSARA_TEST_EQUAL_DESC(filterPtr->mFilters.size(), 2, "The filter size should be 2");
        APSARA_TEST_TRUE_DESC(filterPtr->mFilters.find("123_proj_test") != filterPtr->mFilters.end(),
                              "The 123_proj_test should be a key of the filter");
        APSARA_TEST_TRUE_DESC(filterPtr->mFilters.find("456_proj_test_1") != filterPtr->mFilters.end(),
                              "The 456_test_1 should be a key of the filter");
        LogFilterRule* filterRulePtr = filterPtr->mFilters["123_proj_test"];
        APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterKeys.size(), 2, "The filter keys size should be 2");
        APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterRegs.size(), 2, "The filter regs size should be 2");
        APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterKeys[0], "key1", "The filter key should be key1");
        APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterKeys[1], "key2", "The filter key should be key2");
        APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterRegs[0], regex("value1"), "The filter reg should be value1");
        APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterRegs[1], regex("value2"), "The filter reg should be value1");
        LOG_INFO(sLogger, ("TestInitFilter() end", time(NULL)));
    }

    void TestInitFilterFail() {
        LOG_INFO(sLogger, ("TestInitFilterFail() begin", time(NULL)));
        static LogFilter* filterPtr = LogFilter::Instance();
        string config = "{\"filters\" : [{\"project_name\" : \"123_proj\", \"category\" : \"test\", \"keys\" : "
                        "[\"key1\", \"key2\"], \"regs\" : [\"value1\",\"value2\", \"value3\"]}]}";
        string path = GetProcessExecutionDir() + "logtail_filter.json";
        ofstream file(path.c_str());
        file << config;
        file.close();
        APSARA_TEST_TRUE_DESC(filterPtr->InitFilter("logtail_filter.json") == false,
                              "false should returned for the invalid config");
        LOG_INFO(sLogger, ("TestInitFilterFail() end", time(NULL)));
    }

    void TestInitFilterMissFieldFail() {
        LOG_INFO(sLogger, ("TestInitFilterMissFieldFail() begin", time(NULL)));
        static LogFilter* filterPtr = LogFilter::Instance();
        string config = "{\"filters\" : [{\"project_name\" : \"123_proj\", \"category\" : \"test\", \"keys\" : "
                        "[\"key1\", \"key2\", \"key3\"], \"regs\" : [\"value1\",\"value2\"]}]}";
        string path = GetProcessExecutionDir() + "logtail_filter.json";
        ofstream file(path.c_str());
        file << config;
        file.close();
        APSARA_TEST_TRUE_DESC(filterPtr->InitFilter("logtail_filter.json") == false,
                              "false should returned for the invalid config");
        LOG_INFO(sLogger, ("TestInitFilterMissFieldFail() end", time(NULL)));
    }

    void TestFilter() {
        LOG_INFO(sLogger, ("TestFilter() begin", time(NULL)));
        static LogFilter* filterPtr = LogFilter::Instance();
        filterPtr->mFilters.clear();
        string config = "{\"filters\" : [{\"project_name\" : \"123_proj\", \"category\" : \"test\", \"keys\" : "
                        "[\"key1\", \"key2\"], \"regs\" : [\".*value1\",\"value2.*\"]}]}";
        string path = GetProcessExecutionDir() + "logtail_filter.json";
        ofstream file(path.c_str());
        file << config;
        file.close();
        filterPtr->InitFilter("logtail_filter.json");
        LogGroup logGroup;
        Log* logPtr = NULL;
        Log_Content* logContentPtr = NULL;

        // case 1 : the field are all provided,  only one matched
        logGroup.set_category("test");
        logPtr = logGroup.add_logs();
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key1");
        logContentPtr->set_value("value1xxxxx");
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key2");
        logContentPtr->set_value("value2xxxxx");

        logPtr = logGroup.add_logs();
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key1");
        logContentPtr->set_value("abcdeavalue1");
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key2");
        logContentPtr->set_value("value2xxxxx");

        vector<int32_t> indexs = filterPtr->Filter("123_proj", "", logGroup);
        APSARA_TEST_TRUE_DESC(indexs.size() == 1, "the filtered size should be 1");
        APSARA_TEST_TRUE_DESC(indexs[0] == 1, "the second metric should be matched");
        logGroup.Clear();

        // case 2 : NOT all fields exist, it
        logGroup.set_category("test");
        logPtr = logGroup.add_logs();
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key1");
        logContentPtr->set_value("abcvalue1");
        indexs = filterPtr->Filter("123_proj", "", logGroup);
        APSARA_TEST_TRUE_DESC(indexs.size() == 0, "the filtered size should be 0");
        logGroup.Clear();

        // case 3 : if the filter don't exist, it should return all the fields;
        logGroup.set_category("test");
        logPtr = logGroup.add_logs();
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key1");
        logContentPtr->set_value("value1xxxxx");
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key2");
        logContentPtr->set_value("value2xxxxx");

        logPtr = logGroup.add_logs();
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key1");
        logContentPtr->set_value("abcdeavalue1");
        logContentPtr = logPtr->add_contents();
        logContentPtr->set_key("key2");
        logContentPtr->set_value("value2xxxxx");

        indexs = filterPtr->Filter("999_proj", "", logGroup);
        APSARA_TEST_EQUAL_DESC(indexs.size(), 2, "the filtered size should be 2");
        APSARA_TEST_EQUAL_DESC(indexs[0], 0, "the first item should be contained");
        APSARA_TEST_EQUAL_DESC(indexs[1], 1, "the second item should be contained");
        logGroup.Clear();
        LOG_INFO(sLogger, ("TestFilter() end", time(NULL)));
    }
    void TestCategoryFilter() {
        LOG_INFO(sLogger, ("TestCategoryFilter() begin", time(NULL)));
        Json::Value rootJson;
        //"slb_aliyun"
        Json::Value slb_aliyun;
        string gRootDir = GetProcessExecutionDir();
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1))
            gRootDir.resize(gRootDir.size() - 1);
        gRootDir += PATH_SEPARATOR + "ilogtailLogFilterUnittest";
        bfs::remove_all(gRootDir);
        bfs::remove_all("log_file_out");
        bfs::remove_all("ilogtail_config.json");
        bfs::create_directories(gRootDir);
        string subDir = gRootDir + logtail::PATH_SEPARATOR + "Job";
        bfs::create_directories(subDir);
        subDir = gRootDir + logtail::PATH_SEPARATOR + "Service";
        bfs::create_directories(subDir);

        Json::Value regs;
        regs.append(Json::Value("(\\S+) (\\S+) (\\S+)"));
        Json::Value keys;
        keys.append(Json::Value("time,key1,key2"));

        slb_aliyun["project_name"] = Json::Value("40000010_proj");
        slb_aliyun["category"] = Json::Value("40000010_category");
        slb_aliyun["log_type"] = Json::Value("common_reg_log");
        slb_aliyun["log_path"] = Json::Value(gRootDir + PATH_SEPARATOR + "Job");
        SetConfigFilePattern(slb_aliyun);
        slb_aliyun["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        slb_aliyun["enable"] = Json::Value(true);
        slb_aliyun["preserve"] = Json::Value(false);
        slb_aliyun["preserve_depth"] = Json::Value(2);
        slb_aliyun["regex"] = regs;
        slb_aliyun["keys"] = keys;
        Json::Value filterRegs;
        filterRegs.append(Json::Value(".*value1.*"));
        filterRegs.append(Json::Value(".*value2.*"));
        Json::Value filterKeys;
        filterKeys.append(Json::Value("key1"));
        filterKeys.append(Json::Value("key2"));
        slb_aliyun["filter_keys"] = filterKeys;
        slb_aliyun["filter_regs"] = filterRegs;
        rootJson["slb_aliyun"] = slb_aliyun;
        Json::Value aliyun_com;
        aliyun_com["project_name"] = Json::Value("41000000_proj");
        aliyun_com["category"] = Json::Value("41000000_category");
        aliyun_com["log_type"] = Json::Value("common_reg_log");
        aliyun_com["log_path"] = Json::Value(gRootDir + PATH_SEPARATOR + "Service");
        aliyun_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        SetConfigFilePattern(aliyun_com);
        aliyun_com["enable"] = Json::Value(true);
        aliyun_com["preserve"] = Json::Value(true);
        aliyun_com["regex"] = regs;
        aliyun_com["keys"] = keys;
        rootJson["aliyun.com"] = aliyun_com;

        // set the logtail dump data to local file
        STRING_FLAG(logtail_send_address) = "file";

        Json::Value metrics;
        metrics["metrics"] = rootJson;
        ofstream fout("user_log_config.json");
        fout << metrics << endl;
        fout.close();
        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config));
        ThreadPtr threadPtr = CreateThread(bind(&LogFilterUnittest::RunningDispatcher, this));
        Sender::Instance()->InitSender();
        sleep(3);
        string path[] = {gRootDir + PATH_SEPARATOR + "Job" + PATH_SEPARATOR,
                         gRootDir + PATH_SEPARATOR + "Service" + PATH_SEPARATOR};
        rmdir("log_file_out");
        string logs[]
            = {" xvalue1y xvalue2y\n", " xvalue1y xvalue2y\n", " xvalue3y xvalue3y\n", " xvalue4y xvalue4y\n"};
        for (int i = 0; i < 2; ++i) {
            char timeBuffer[50];
            struct tm timeInfo;
            time_t cur = time(NULL);
#if defined(__linux__)
            localtime_r(&cur, &timeInfo);
#elif defined(_MSC_VER)
            localtime_s(&timeInfo, &cur);
#endif
            strftime(timeBuffer, sizeof(timeBuffer), "%d/%b/%Y:%R:%S", &timeInfo);

            ofstream fout((path[i] + "a.log").c_str());
            for (int j = 0; j < 4; ++j) {
                string log = timeBuffer + logs[j];
                fout << log;
            }
            fout.flush();
            fout.close();
            sleep(1);
        }
        sleep(10);
        ifstream fin("log_file_out");
        char result[1025];
        vector<string> lines;
        while (fin.getline(result, 1024)) {
            string log = result;
            if (log.find("key") != string::npos)
                lines.push_back(log);
        }
        APSARA_TEST_EQUAL(lines.size(), 6);
        if (lines.size() == 6) {
            for (int i = 0; i < 2; ++i) {
                APSARA_TEST_TRUE_DESC(lines[i].find("value1") != string::npos, lines[i]);
                APSARA_TEST_TRUE_DESC(lines[i].find("value2") != string::npos, lines[i]);
            }
            for (int i = 2; i < 6; ++i) {
                APSARA_TEST_TRUE(lines[i].find("value" + ToString(i - 1)) != string::npos);
            }
        }
        threadPtr->GetValue(1000);
        sleep(2);
        //clean the config file produced by this case
        bfs::remove_all("ilogtail_config.json");
        LOG_INFO(sLogger, ("TestCategoryFilter() end", time(NULL)));
    }
    //APSARA_UNIT_TEST_CASE(TestCategoryFilter,0);

    static const char UTF8_BYTE_PREFIX = 0x80;
    static const char UTF8_BYTE_MASK = 0xc0;

    void TestFilterNoneUtf8() {
        const int caseCharacterNum = 80; //ten charactor for every situation
        string characterSet[caseCharacterNum];
        bool isBlunk[caseCharacterNum][4]; //every charactor has 4 byte atmost

        //generate one byte utf8;

        for (int i = 0; i < 10; ++i) {
            char tmp;
            do {
                tmp = rand() & 0xff;
                tmp &= 0x7f;
            } while (tmp == 32 || tmp == 9); //tmp should not be space or \t

            characterSet[i] = string(1, tmp);
            isBlunk[i][0] = false;
        }

        //generate one byte none utf8
        for (int i = 10; i < 20; ++i) {
            char tmp;
            do {
                tmp = rand() & 0xff;
                tmp |= 0x80;
                tmp &= 0xbf;
            } while (tmp == 32 || tmp == 9); // tmp shoud be 10xx xxxx;
            characterSet[i] = string(1, tmp);
            isBlunk[i][0] = true;
        }

        //generate two byte utf8

        for (int i = 20; i < 30; ++i) {
            char tmp1, tmp2;
            uint16_t unicode;
            do {
                tmp1 = rand() & 0xff;
                tmp2 = rand() & 0xff;
                tmp1 |= 0xc0;
                tmp1 &= 0xdf; //tmp1 should be 0x 110x xxxx
                tmp2 |= 0x80;
                tmp2 &= 0xbf; //tmp2 should be 0x 10xx xxxx
                unicode = (((tmp1 & 0x1f) << 6) | (tmp2 & 0x3f));
            } while (!(unicode >= 0x80 && unicode <= 0x7ff));

            characterSet[i] = string(1, tmp1) + string(1, tmp2);
            isBlunk[i][0] = false;
            isBlunk[i][1] = false;
        }

        //generate two byte noneutf8
        char randArr1[10], randArr2[10], randArr3[10], randArr4[10];
        for (int i = 0; i < 10; ++i) {
            char tmp1, tmp2;
            tmp1 = rand() & 0xff;
            tmp2 = rand() & 0xff;
            tmp1 |= 0xc0;
            tmp1 &= 0xdf; //tmp1 should be 0x110x xxxx
            tmp2 |= 0x80;
            tmp2 &= 0xbf;
            randArr1[i] = tmp1;
            randArr2[i] = tmp2;
        }
        // five case with second binary 0xxx xxxx;
        for (int i = 0; i < 5; ++i) {
            do {
                randArr2[i] = rand() & 0xff;
                randArr2[i] &= 0x7f;
            } while (randArr2[i] == 32);
        }

        for (int index = 30; index < 35; ++index) {
            characterSet[index] = string(1, randArr1[index - 30]) + string(1, randArr2[index - 30]);
            isBlunk[index][0] = true;
            isBlunk[index][1] = false;
        }
        // five case of the situation thar only the format is utf8, but not unicode;

        for (int index = 35; index < 40; ++index) {
            randArr1[index - 30]
                &= 0xe1; //unicode must in rand [0x80,0x7fff]; ant two byte has 11 bits ,so the situation can only be < 0x80
            characterSet[index] = string(1, randArr1[index - 30]) + string(1, randArr2[index - 30]);
            isBlunk[index][0] = true;
            isBlunk[index][1] = true;
        }


        //generate three bytes utf8

        for (int i = 40; i < 50; ++i) {
            char tmp1, tmp2, tmp3;
            uint16_t unicode;
            do {
                tmp1 = rand() & 0xff;
                tmp2 = rand() & 0xff;
                tmp3 = rand() & 0xff;
                tmp1 |= 0xe0;
                tmp1 &= 0xef; //tmp1 should be 0x 1110x xxxx
                tmp2 |= 0x80;
                tmp2 &= 0xbf; //tmp2 should be 10xx xxxx
                tmp3 |= 0x80;
                tmp3 &= 0xbf; //tmp3 should be 10xx xxxx
                unicode = (((tmp1 & 0x0f) << 12) | ((tmp2 & 0x3f) << 6) | (tmp3 & 0x3f));
            } while (!(unicode >= 0x800));

            characterSet[i] = string(1, tmp1) + string(1, tmp2) + string(1, tmp3);
            isBlunk[i][0] = false;
            isBlunk[i][1] = false;
            isBlunk[i][2] = false;
        }

        //generate three bytes none utf8
        for (int i = 50; i < 60; ++i) {
            char tmp1, tmp2, tmp3;
            tmp1 = rand() & 0xff;
            tmp2 = rand() & 0xff;
            tmp3 = rand() & 0xff;
            tmp1 |= 0xe0;
            tmp1 &= 0xef; //tmp1 should be 0x 1110x xxxx
            tmp2 |= 0x80;
            tmp2 &= 0xbf; //tmp2 should be 10xx xxxx
            tmp3 |= 0x80;
            tmp3 &= 0xbf; //tmp3 should be 10xx xxxx
            randArr1[i - 50] = tmp1;
            randArr2[i - 50] = tmp2;
            randArr3[i - 50] = tmp3;
        }

        //the situation of 1110xxxx 0xxxxxxx 10xxxxxxx
        for (int i = 50; i < 52; ++i) {
            do {
                randArr2[i - 50] = rand() & 0xff;
                randArr2[i - 50] &= 0x7f; //second bytes is 0xxx xxxx;
            } while (randArr2[i - 50] == 32);
            characterSet[i] = string(1, randArr1[i - 50]) + string(1, randArr2[i - 50]) + string(1, randArr3[i - 50]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = false;
            isBlunk[i][2] = true;
        }
        //the situation of 1110xxxx 10xxxxxx 0xxxxxxx
        for (int i = 52; i < 54; ++i) {
            do {
                randArr3[i - 50] = rand() & 0xff;
                randArr3[i - 50] &= 0x7f; //second bytes is 0xxx xxxx;
            } while (randArr3[i - 50] == 32);
            characterSet[i] = string(1, randArr1[i - 50]) + string(1, randArr2[i - 50]) + string(1, randArr3[i - 50]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = true;
            isBlunk[i][2] = false;
        }
        //the situation of 1110xxxx 0xxxxxxx 0xxxxxxx

        for (int i = 54; i < 56; ++i) {
            do {
                randArr2[i - 50] = rand() & 0xff;
                randArr2[i - 50] &= 0x7f;
                randArr3[i - 50] = rand() & 0xff;
                randArr3[i - 50] &= 0x7f; //second bytes is 0xxx xxxx
            } while (randArr3[i - 50] == 32 || randArr2[i - 50] == 32);
            characterSet[i] = string(1, randArr1[i - 50]) + string(1, randArr2[i - 50]) + string(1, randArr3[i - 50]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = false;
            isBlunk[i][2] = false;
        }

        //the situation of only format in utf8;
        for (int i = 56; i < 60; ++i) {
            randArr1[i - 50] &= 0xf0;
            randArr2[i - 50] &= 0xdf; //1110 0000  100xxxxx 10xxxxxx

            characterSet[i] = string(1, randArr1[i - 50]) + string(1, randArr2[i - 50]) + string(1, randArr3[i - 50]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = true;
            isBlunk[i][2] = true;
        }
        //generate four bytes utf8

        for (int i = 60; i < 70; ++i) {
            char tmp1, tmp2, tmp3, tmp4;
            uint32_t unicode;
            do {
                tmp1 = rand() & 0xff;
                tmp2 = rand() & 0xff;
                tmp3 = rand() & 0xff;
                tmp4 = rand() & 0xff;
                tmp1 |= 0xf0;
                tmp1 &= 0xf7; //tmp1 should be 0x 11110x xxxx
                tmp2 |= 0x80;
                tmp2 &= 0xbf; //tmp2 should be 10xx xxxx
                tmp3 |= 0x80;
                tmp3 &= 0xbf; //tmp3 should be 10xx xxxx
                tmp4 |= 0x80;
                tmp4 &= 0xbf; //tmp3 should be 10xx xxxx
                unicode = ((tmp1 & 0x07) << 18) | ((tmp2 & 0x3f) << 12) | ((tmp3 & 0x3f) << 6) | (tmp4 & 0x3f);
            } while (!(unicode >= 0x00010000 && unicode <= 0x0010ffff));

            characterSet[i] = string(1, tmp1) + string(1, tmp2) + string(1, tmp3) + string(1, tmp4);
            isBlunk[i][0] = false;
            isBlunk[i][1] = false;
            isBlunk[i][2] = false;
            isBlunk[i][3] = false;
        }

        //generate 4 bytes none utf8

        for (int i = 70; i < 80; ++i) {
            char tmp1, tmp2, tmp3, tmp4;
            tmp1 = rand() & 0xff;
            tmp2 = rand() & 0xff;
            tmp3 = rand() & 0xff;
            tmp4 = rand() & 0xff;
            tmp1 |= 0xf0;
            tmp1 &= 0xf7; //tmp1 should be 0x 1110x xxxx
            tmp2 |= 0x80;
            tmp2 &= 0xbf; //tmp2 should be 10xx xxxx
            tmp3 |= 0x80;
            tmp3 &= 0xbf; //tmp3 should be 10xx xxxx
            tmp4 |= 0x80;
            tmp4 &= 0xbf; //tmp3 should be 10xx xxxx

            randArr1[i - 70] = tmp1;
            randArr2[i - 70] = tmp2;
            randArr3[i - 70] = tmp3;
            randArr4[i - 70] = tmp4;
        }

        //the situation of 11110xxx 0xxxxxxx 10xxxxxxx 10xxxxxx
        for (int i = 70; i < 72; ++i) {
            do {
                randArr2[i - 70] = rand() & 0xff;
                randArr2[i - 70] &= 0x7f; //second bytes is 0xxx xxxx;
            } while (randArr2[i - 70] == 32);

            characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
                + string(1, randArr4[i - 70]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = false;
            isBlunk[i][2] = true;
            isBlunk[i][3] = true;
        }
        //the situation of 1110xxxx 10xxxxxx 0xxxxxxx 10xxxxxx
        for (int i = 72; i < 74; ++i) {
            do {
                randArr3[i - 70] = rand() & 0xff;
                randArr3[i - 70] &= 0x7f; //second bytes is 0xxx xxxx;
            } while (randArr3[i - 70] == 32);
            characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
                + string(1, randArr4[i - 70]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = true;
            isBlunk[i][2] = false;
            isBlunk[i][3] = true;
        }
        //the situation of 1110xxxx 0xxxxxxx 0xxxxxxx 0xxxxxxxx

        for (int i = 74; i < 76; ++i) {
            do {
                randArr2[i - 70] = rand() & 0xff;
                randArr2[i - 70] &= 0x7f;
                randArr3[i - 70] = rand() & 0xff;
                randArr3[i - 70] &= 0x7f; //second bytes is 0xxx xxxx
                randArr4[i - 70] = rand() & 0xff;
                randArr4[i - 70] &= 0x7f; //second bytes is 0xxx xxxx
            } while (randArr4[i - 70] == 32 || randArr2[i - 70] == 32 || randArr3[i - 70] == 32);
            characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
                + string(1, randArr4[i - 70]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = false;
            isBlunk[i][2] = false;
            isBlunk[i][3] = false;
        }

        //the situation of only format in utf8; and the real unicode is not in range

        //less than range
        for (int i = 76; i < 78; ++i) {
            randArr1[i - 70] &= 0xf0;
            randArr2[i - 70] &= 0x8f; //1110 0000  100xxxxx 10xxxxxx
            characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
                + string(1, randArr4[i - 70]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = true;
            isBlunk[i][2] = true;
            isBlunk[i][3] = true;
        }


        //greater than range
        for (int i = 78; i < 80; ++i) {
            randArr1[i - 70] |= 0x04;
            randArr2[i - 70] |= 0x10; //1110 0000  100xxxxx 10xxxxxx

            characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
                + string(1, randArr4[i - 70]);
            isBlunk[i][0] = true;
            isBlunk[i][1] = true;
            isBlunk[i][2] = true;
            isBlunk[i][3] = true;
        }

        for (int i = 0; i < 10; ++i) {
            LOG_INFO(sLogger, ("################################round", i));
            string testStr;
            const int CHARACTER_COUNT = 8192;
            bool flow[CHARACTER_COUNT * 4];
            int index = 0; //index of flow
            //generate test string with character randomly, and record whether a position should be replaced by blunck
            for (int j = 0; j < CHARACTER_COUNT; ++j) {
                int randIndex = rand() % 80;
                LOG_INFO(sLogger, ("j", j)("randIndex", randIndex)("index", index));
                testStr += characterSet[randIndex];
                if (randIndex >= 0 && randIndex < 20) {
                    flow[index] = isBlunk[randIndex][0];
                    index++;
                } else if (randIndex >= 20 && randIndex < 40) {
                    flow[index] = isBlunk[randIndex][0];
                    flow[index + 1] = isBlunk[randIndex][1];
                    index += 2;
                } else if (randIndex >= 40 && randIndex < 60) {
                    flow[index] = isBlunk[randIndex][0];
                    flow[index + 1] = isBlunk[randIndex][1];
                    flow[index + 2] = isBlunk[randIndex][2];
                    index += 3;
                } else if (randIndex >= 60 && randIndex < 80) {
                    flow[index] = isBlunk[randIndex][0];
                    flow[index + 1] = isBlunk[randIndex][1];
                    flow[index + 2] = isBlunk[randIndex][2];
                    flow[index + 3] = isBlunk[randIndex][3];
                    index += 4;
                }

                if (j == (CHARACTER_COUNT - 1) && randIndex >= 20
                    && randIndex % 20 < 10) //the last character of string ,and at least two bytes,ant is utf8
                {
                    testStr = testStr.substr(0, testStr.size() - 1);
                    if (randIndex >= 20 && randIndex < 30)
                        flow[index - 2] = true;
                    else if (randIndex >= 40 && randIndex < 50)
                        flow[index - 3] = flow[index - 2] = true;
                    else if (randIndex >= 60 && randIndex < 70)
                        flow[index - 4] = flow[index - 3] = flow[index - 2] = true;
                }
            }
            FilterNoneUtf8(testStr);
            for (uint32_t indexOfString = 0; indexOfString < testStr.size(); ++indexOfString) {
                if (flow[indexOfString] == true) {
                    if (testStr[indexOfString] != ' ')
                        LOG_ERROR(sLogger, ("#indexOfString", indexOfString)("char", testStr[indexOfString]));
                    APSARA_TEST_TRUE(testStr[indexOfString] == ' ');
                } else {
                    if (testStr[indexOfString] == ' ')
                        LOG_ERROR(sLogger, ("*indexOfString", indexOfString)("char", testStr[indexOfString]));
                    APSARA_TEST_TRUE(testStr[indexOfString] != ' ');
                }
            }
        }
    } //end of case

    void TestFilterNode() {
        LogGroupContext context;
        {
            // a: int, b: string c: ip, d: date
            const char* jsonStr
                = "{\n"
                  "  \"operator\": \"and\",\n"
                  "  \"operands\": [\n"
                  "    {\n"
                  "      \"operator\": \"and\",\n"
                  "      \"operands\": [\n"
                  "        {\n"
                  "          \"type\": \"regex\",\n"
                  "          \"key\": \"a\",\n"
                  "          \"exp\": \"\\\\d+\"\n"
                  "        },\n"
                  "      \t{\n"
                  "      \t  \"operator\": \"not\",\n"
                  "      \t  \"operands\": [\n"
                  "      \t    {\n"
                  "      \t      \"type\": \"regex\",\n"
                  "              \"key\": \"d\",\n"
                  "              \"exp\": \"20\\\\d{1,2}-\\\\d{1,2}-\\\\d{1,2}\"\n"
                  "      \t    }\n"
                  "      \t  ]\n"
                  "      \t}\n"
                  "      ]\n"
                  "    },\n"
                  "    {\n"
                  "      \"operator\": \"or\",\n"
                  "      \"operands\": [\n"
                  "        {\n"
                  "          \"type\": \"regex\",\n"
                  "          \"key\": \"b\",\n"
                  "          \"exp\": \"\\\\S+\"\n"
                  "        },\n"
                  "        {\n"
                  "          \"type\": \"regex\",\n"
                  "          \"key\": \"c\",\n"
                  "          \"exp\": "
                  "\"((2[0-4]\\\\d|25[0-5]|[01]?\\\\d\\\\d?)\\\\.){3}(2[0-4]\\\\d|25[0-5]|[01]?\\\\d\\\\d?)\"\n"
                  "        }\n"
                  "      ]\n"
                  "    }\n"
                  "  ]\n"
                  "}";

            Json::Reader reader;
            Json::Value rootNode;
            APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

            // (a and not d) and (b or c)
            BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
            APSARA_TEST_TRUE_FATAL(root.get() != NULL);

            sls_logs::LogGroup logGroup;
            {
                sls_logs::Log* logPtr = logGroup.add_logs();
                sls_logs::Log_Content* contentPtr = logPtr->add_contents();
                contentPtr->set_key("a");
                contentPtr->set_value("100");
                contentPtr = logPtr->add_contents();
                contentPtr->set_key("b");
                contentPtr->set_value("xxx");
                contentPtr = logPtr->add_contents();
                contentPtr->set_key("c");
                contentPtr->set_value("192.168.1.1");
                contentPtr = logPtr->add_contents();
                contentPtr->set_key("d");
                contentPtr->set_value(
                    "2008-08-08"); // matched 21 century date regex, but because of not operator, match result is false
            }
            {
                sls_logs::Log* logPtr = logGroup.add_logs();
                sls_logs::Log_Content* contentPtr = logPtr->add_contents();
                contentPtr->set_key("a");
                contentPtr->set_value("100");
                contentPtr = logPtr->add_contents();
                contentPtr->set_key("b");
                contentPtr->set_value("xxx");
                contentPtr = logPtr->add_contents();
                contentPtr->set_key("c");
                contentPtr->set_value("888.168.1.1"); // failed to match ip regex
                contentPtr->set_key("d");
                contentPtr->set_value(
                    "1999-1-1"); // failed to match 21 century date regex, but because of not operator, match result is true
            }
            {
                sls_logs::Log* logPtr = logGroup.add_logs();
                sls_logs::Log_Content* contentPtr = logPtr->add_contents();
                contentPtr->set_key("a");
                contentPtr->set_value("aaa"); // failed to match int
                contentPtr = logPtr->add_contents();
                contentPtr->set_key("b");
                contentPtr->set_value("xxx");
                contentPtr = logPtr->add_contents();
                contentPtr->set_key("c");
                contentPtr->set_value("8.8.8.8");
                contentPtr->set_key("d");
                contentPtr->set_value(
                    "2222-22-22"); // failed to match 21 century date regex, but because of not operator, match result is true
            }

            APSARA_TEST_EQUAL_FATAL(logGroup.logs_size(), 3);
            APSARA_TEST_TRUE(root->Match(logGroup.logs(0), context) == false);
            APSARA_TEST_TRUE(root->Match(logGroup.logs(1), context));
            APSARA_TEST_TRUE(root->Match(logGroup.logs(2), context) == false);

            static LogFilter* filter = LogFilter::Instance();
            std::vector<int32_t> index = filter->Filter(
                logGroup, root, LogGroupContext("default_region", "ant-test-project", "test-logstore"));
            APSARA_TEST_EQUAL_FATAL(index.size(), 1);
            APSARA_TEST_EQUAL(index[0], 1);
        }

        {
            const char* jsonStr = "{\n"
                                  "  \"operator\": \"and\",\n"
                                  "  \"operands\": [\n"
                                  "    {\n"
                                  "      \"type\": \"regex\",\n"
                                  "      \"key\": \"a\",\n"
                                  "      \"exp\": \"regex1\"\n"
                                  "    },\n"
                                  "    {\n"
                                  "      \"type\": \"regex\",\n"
                                  "      \"key\": \"b\",\n"
                                  "      \"exp\": \"regex2\"\n"
                                  "    }\n"
                                  "  ]\n"
                                  "}";
            Json::Reader reader;
            Json::Value rootNode;
            APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

            BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
            APSARA_TEST_TRUE_FATAL(root.get() != NULL);
        }

        {
            const char* jsonStr = "{\n"
                                  "    \"key\": \"a\",\n"
                                  "    \"exp\": \"xxx\",\n"
                                  "    \"type\": \"regex\"\n"
                                  "}";
            Json::Reader reader;
            Json::Value rootNode;
            APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

            BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
            APSARA_TEST_TRUE_FATAL(root.get() != NULL);
        }

        {
            // not operator
            const char* jsonStr = "{\n"
                                  "  \"operator\": \"not\",\n"
                                  "  \"operands\": [\n"
                                  "    {\n"
                                  "      \"type\": \"regex\",\n"
                                  "      \"key\": \"a\",\n"
                                  "      \"exp\": \"regex1\"\n"
                                  "    }\n"
                                  "  ]\n"
                                  "}";
            Json::Reader reader;
            Json::Value rootNode;
            APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

            BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
            APSARA_TEST_TRUE_FATAL(root.get() != NULL);
        }

        {
            // missing reg
            const char* jsonStr = "{\n"
                                  "  \"operator\": \"and\",\n"
                                  "  \"operands\": [\n"
                                  "    {\n"
                                  "      \"type\": \"regex\",\n"
                                  "      \"key\": \"a\",\n"
                                  "      \"exp\": \"regex1\"\n"
                                  "    },\n"
                                  "    {\n"
                                  "      \"operator\": \"or\",\n"
                                  "      \"operands\": [\n"
                                  "        {\n"
                                  "          \"type\": \"regex\",\n"
                                  "          \"key\": \"b\"\n"
                                  "        },\n"
                                  "        {\n"
                                  "          \"type\": \"regex\",\n"
                                  "          \"key\": \"c\",\n"
                                  "          \"exp\": \"regex3\"\n"
                                  "        }\n"
                                  "      ]\n"
                                  "    }\n"
                                  "  ]\n"
                                  "}";
            Json::Reader reader;
            Json::Value rootNode;
            APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

            BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
            APSARA_TEST_TRUE_FATAL(root.get() == NULL);
        }

        {
            // missing right
            const char* jsonStr = "{\n"
                                  "  \"operator\": \"and\",\n"
                                  "  \"operands\": [\n"
                                  "    {\n"
                                  "      \"type\": \"regex\",\n"
                                  "      \"key\": \"a\",\n"
                                  "      \"exp\": \"regex1\"\n"
                                  "    },\n"
                                  "    {\n"
                                  "      \"operator\": \"or\",\n"
                                  "      \"operands\": [\n"
                                  "        {\n"
                                  "          \"type\": \"regex\",\n"
                                  "          \"key\": \"b\",\n"
                                  "          \"exp\": \"regex2\"\n"
                                  "        }\n"
                                  "      ]\n"
                                  "    }\n"
                                  "  ]\n"
                                  "}";
            Json::Reader reader;
            Json::Value rootNode;
            APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

            BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
            APSARA_TEST_TRUE_FATAL(root.get() == NULL);
        }

        {
            // missing op
            const char* jsonStr = "{\n"
                                  "  \"operator\": \"and\",\n"
                                  "  \"operands\": [\n"
                                  "    {\n"
                                  "      \"type\": \"regex\",\n"
                                  "      \"key\": \"a\",\n"
                                  "      \"exp\": \"regex1\"\n"
                                  "    },\n"
                                  "    {\n"
                                  "      \"operands\": [\n"
                                  "        {\n"
                                  "          \"type\": \"regex\",\n"
                                  "          \"key\": \"b\",\n"
                                  "          \"exp\": \"regex2\"\n"
                                  "        },\n"
                                  "        {\n"
                                  "          \"type\": \"regex\",\n"
                                  "          \"key\": \"c\",\n"
                                  "          \"exp\": \"regex3\"\n"
                                  "        }\n"
                                  "      ]\n"
                                  "    }\n"
                                  "  ]\n"
                                  "}";
            Json::Reader reader;
            Json::Value rootNode;
            APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

            BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
            APSARA_TEST_TRUE_FATAL(root.get() == NULL);
        }

        {
            // redundant fields
            const char* jsonStr = "{\n"
                                  "  \"operator\": \"and\",\n"
                                  "  \"operands\": [\n"
                                  "    {\n"
                                  "      \"type\": \"regex\",\n"
                                  "      \"key\": \"b\",\n"
                                  "      \"exp\": \"regex2\"\n"
                                  "    },\n"
                                  "    {\n"
                                  "      \"type\": \"regex\",\n"
                                  "      \"key\": \"c\",\n"
                                  "      \"exp\": \"regex3\"\n"
                                  "    }\n"
                                  "  ],\n"
                                  "  \"type\": \"regex\",\n"
                                  "  \"key\": \"c\",\n"
                                  "  \"exp\": \"regex3\"\n"
                                  "}";
            Json::Reader reader;
            Json::Value rootNode;
            APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

            BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
            APSARA_TEST_TRUE_FATAL(root.get() != NULL);
            APSARA_TEST_TRUE(root->GetNodeType() == OPERATOR_NODE);
        }
    }
};

APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestFilterNoneUtf8, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestParseCastSensWordConfig, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestCastSensWordConst, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestCastSensWordMD5, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestCastSensWordFail, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestCastSensWordLoggroup, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestCastSensWordMulti, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestCastWholeKey, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestInitFilter, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestInitFilterFail, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestInitFilterMissFieldFail, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestFilter, 0);
APSARA_UNIT_TEST_CASE(LogFilterUnittest, TestFilterNode, 0);

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
