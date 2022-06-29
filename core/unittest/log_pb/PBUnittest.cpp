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

#include <logger/Logger.h>
#include "unittest/Unittest.h"
#include "log_pb/RawLogGroup.h"
#include "log_pb/sls_logs.pb.h"

using namespace std;
using namespace sls_logs;

namespace logtail {

class PBUnittest : public ::testing::Test {
public:
    string longLogValue10 = string(10 * 1024, 'c');
    string longLogValue100 = string(100 * 1024, 'd');
    string longLogValue1000 = string(1000 * 1024, 'e');
    string longLogValue10000 = string(10000 * 1024, 'f');

    static void SetUpTestCase() //void Setup();
    {}
    static void TearDownTestCase() //void CleanUp();
    {}

    void TestFullWrite() {
        boost::match_results<const char*> what;
        boost::regex reg("(\\w+) (\\w+) (\\w+) (\\w+)");
        boost::regex_match("1 234 567 890xyz", what, reg, boost::match_default);
        vector<string> keys;
        keys.push_back("key1");
        keys.push_back("key2");
        keys.push_back("key3");
        keys.push_back("");

        int32_t nowTime = time(NULL);

        RawLog* rawLog = RawLog::AddLogFull(nowTime, keys, what);


        LogGroup loggroup;
        Log* log = loggroup.add_logs();
        Log_Content* kv = log->add_contents();
        kv->set_key("key1");
        kv->set_value("1");
        kv = log->add_contents();
        kv->set_key("key2");
        kv->set_value("234");
        kv = log->add_contents();
        kv->set_key("key3");
        kv->set_value("567");
        kv = log->add_contents();
        kv->set_key("");
        kv->set_value("890xyz");
        log->set_time(nowTime);

        string rawLogStr;
        rawLog->AppendToString(&rawLogStr);
        string logStr;
        EXPECT_EQ(loggroup.AppendToString(&logStr), true);

        EXPECT_EQ(rawLogStr, logStr);

        delete rawLog;
    }

    void TestNormalWrite() {
        RawLog rawLog;
        int32_t nowTime = time(NULL);
        rawLog.AddLogStart(nowTime);
        rawLog.AddKeyValue("key1", strlen("key1"), "value", strlen("value"));
        rawLog.AddKeyValue("key2", strlen("key2"), "", 0);
        rawLog.AddKeyValue("key3", strlen("key3"), "value3", strlen("value3"));
        rawLog.AddKeyValue("key4", strlen("key4"), "value", strlen("value"));
        rawLog.AddLogDone();

        LogGroup loggroup;
        Log* log = loggroup.add_logs();
        Log_Content* kv = log->add_contents();
        kv->set_key("key1");
        kv->set_value("value");
        kv = log->add_contents();
        kv->set_key("key2");
        kv->set_value("");
        kv = log->add_contents();
        kv->set_key("key3");
        kv->set_value("value3");
        kv = log->add_contents();
        kv->set_key("key4");
        kv->set_value("value");
        log->set_time(nowTime);

        string rawLogStr;
        rawLog.AppendToString(&rawLogStr);
        string logStr;
        EXPECT_EQ(loggroup.AppendToString(&logStr), true);
        EXPECT_EQ(rawLogStr, logStr);
    }

    void TestBigLog() {
        RawLog rawLog;
        int32_t nowTime = time(NULL);
        rawLog.AddLogStart(nowTime);
        rawLog.AddKeyValue("key1", strlen("key1"), "value", strlen("value"));
        rawLog.AddKeyValue("key2", strlen("key2"), longLogValue100.c_str(), longLogValue100.size());
        rawLog.AddKeyValue("key3", strlen("key3"), longLogValue10.c_str(), longLogValue10.size());
        rawLog.AddKeyValue("key4", strlen("key4"), longLogValue1000.c_str(), longLogValue1000.size());
        rawLog.AddLogDone();

        LogGroup loggroup;
        Log* log = loggroup.add_logs();
        Log_Content* kv = log->add_contents();
        kv->set_key("key1");
        kv->set_value("value");
        kv = log->add_contents();
        kv->set_key("key2");
        kv->set_value(longLogValue100);
        kv = log->add_contents();
        kv->set_key("key3");
        kv->set_value(longLogValue10);
        kv = log->add_contents();
        kv->set_key("key4");
        kv->set_value(longLogValue1000);
        log->set_time(nowTime);

        string rawLogStr;
        rawLog.AppendToString(&rawLogStr);
        string logStr;
        EXPECT_EQ(loggroup.AppendToString(&logStr), true);
        EXPECT_EQ(rawLogStr, logStr);
    }

    void TestIterator() {
        RawLog rawLog;
        int32_t nowTime = time(NULL);
        rawLog.AddLogStart(nowTime);
        rawLog.AddKeyValue("key1", strlen("key1"), "value1", strlen("value1"));
        rawLog.AddKeyValue("key2", strlen("key2"), "value2", strlen("value2"));
        rawLog.AddKeyValue("key3", strlen("key3"), "value3", strlen("value3"));
        rawLog.AddKeyValue("key4", strlen("key4"), "value4", strlen("value4"));
        rawLog.AddLogDone();

        RawLog::iterator iter = rawLog.GetIterator();
        const char* key = NULL;
        uint32_t keyLen = 0;
        const char* value = NULL;
        uint32_t valueLen = 0;
        int i = 0;
        while (rawLog.NextKeyValue(iter, key, keyLen, value, valueLen)) {
            ++i;
            string keyStr(key, keyLen);
            string valueStr(value, valueLen);
            string expectKey = "key" + std::to_string(i);
            string expectValue = "value" + std::to_string(i);
            EXPECT_EQ(keyStr, expectKey);
            EXPECT_EQ(valueStr, expectValue);
        }
    }

    void TestUpdateDeleteAppend() {
        RawLog rawLog;
        int32_t nowTime = time(NULL);
        rawLog.AddLogStart(nowTime);
        rawLog.AddKeyValue("key1", strlen("key1"), "value1", strlen("value1"));
        rawLog.AddKeyValue("k", strlen("k"), "value", strlen("value"));
        rawLog.AddKeyValue("k", strlen("k"), "value", strlen("value"));
        rawLog.AddKeyValue("k", strlen("k"), "value", strlen("value"));
        rawLog.AddLogDone();

        RawLog::iterator iter = rawLog.GetIterator();
        const char* key = NULL;
        uint32_t keyLen = 0;
        const char* value = NULL;
        uint32_t valueLen = 0;
        {
            EXPECT_TRUE(rawLog.NextKeyValue(iter, key, keyLen, value, valueLen));
            string keyStr(key, keyLen);
            string valueStr(value, valueLen);
            EXPECT_EQ(keyStr, string("key1"));
            EXPECT_EQ(valueStr, string("value1"));
        }
        {
            EXPECT_TRUE(rawLog.NextKeyValue(iter, key, keyLen, value, valueLen));
            rawLog.UpdateKeyValue(iter, "key2", strlen("key2"), "value2", strlen("value2"));
        }
        {
            EXPECT_TRUE(rawLog.NextKeyValue(iter, key, keyLen, value, valueLen));
            rawLog.DeleteKeyValue(iter);
        }
        {
            EXPECT_TRUE(rawLog.NextKeyValue(iter, key, keyLen, value, valueLen));
            rawLog.UpdateKeyValue(iter, "key3", strlen("key3"), "value3", strlen("value3"));
        }
        EXPECT_FALSE(rawLog.NextKeyValue(iter, key, keyLen, value, valueLen));
        rawLog.AppendKeyValue("key4", strlen("key4"), "value4", strlen("value4"));

        LogGroup loggroup;
        Log* log = loggroup.add_logs();
        Log_Content* kv = log->add_contents();
        kv->set_key("key1");
        kv->set_value("value1");
        kv = log->add_contents();
        kv->set_key("key2");
        kv->set_value("value2");
        kv = log->add_contents();
        kv->set_key("key3");
        kv->set_value("value3");
        kv = log->add_contents();
        kv->set_key("key4");
        kv->set_value("value4");
        log->set_time(nowTime);

        string rawLogStr;
        rawLog.AppendToString(&rawLogStr);
        string logStr;
        EXPECT_EQ(loggroup.AppendToString(&logStr), true);
        EXPECT_EQ(rawLogStr, logStr);
    }

    void TestLogGroup() {
        RawLog* pRawLog = new RawLog();
        RawLog& rawLog = *pRawLog;
        int32_t nowTime = time(NULL);
        rawLog.AddLogStart(nowTime);
        rawLog.AddKeyValue("key1", strlen("key1"), "value", strlen("value"));
        rawLog.AddKeyValue("key2", strlen("key2"), "", 0);
        rawLog.AddKeyValue("key3", strlen("key3"), "value3", strlen("value3"));
        rawLog.AddKeyValue("key4", strlen("key4"), "value", strlen("value"));
        rawLog.AddLogDone();

        LogGroup loggroup;
        Log* log = loggroup.add_logs();
        Log_Content* kv = log->add_contents();
        kv->set_key("key1");
        kv->set_value("value");
        kv = log->add_contents();
        kv->set_key("key2");
        kv->set_value("");
        kv = log->add_contents();
        kv->set_key("key3");
        kv->set_value("value3");
        kv = log->add_contents();
        kv->set_key("key4");
        kv->set_value("value");
        log->set_time(nowTime);


        RawLogGroup* pRawLogGroup = new RawLogGroup;
        pRawLogGroup->set_source("192.168.1.1");
        loggroup.set_source("192.168.1.1");

        pRawLogGroup->set_category("logstore");
        loggroup.set_category("logstore");

        pRawLogGroup->set_machineuuid("123-456");
        loggroup.set_machineuuid("123-456");

        pRawLogGroup->set_topic("topic");
        loggroup.set_topic("topic");

        pRawLogGroup->add_logtags("tagkey1", "tagvalue1");
        pRawLogGroup->add_logtags("tagkey2", "tagvalue2");

        auto tag1 = loggroup.add_logtags();
        tag1->set_key("tagkey1");
        tag1->set_value("tagvalue1");

        auto tag2 = loggroup.add_logtags();
        tag2->set_key("tagkey2");
        tag2->set_value("tagvalue2");

        string rawLogStr;
        pRawLogGroup->add_logs(pRawLog);
        pRawLogGroup->AppendToString(&rawLogStr);
        string logStr;
        EXPECT_EQ(loggroup.AppendToString(&logStr), true);
        EXPECT_EQ(rawLogStr, logStr);
        delete pRawLogGroup;
    }

    void TestNoOptionLogGroup() {
        RawLog* pRawLog = new RawLog();
        RawLog& rawLog = *pRawLog;
        int32_t nowTime = time(NULL);
        rawLog.AddLogStart(nowTime);
        rawLog.AddKeyValue("key1", strlen("key1"), "value", strlen("value"));
        rawLog.AddKeyValue("key2", strlen("key2"), "", 0);
        rawLog.AddKeyValue("key3", strlen("key3"), "value3", strlen("value3"));
        rawLog.AddKeyValue("key4", strlen("key4"), "value", strlen("value"));
        rawLog.AddLogDone();

        LogGroup loggroup;
        Log* log = loggroup.add_logs();
        Log_Content* kv = log->add_contents();
        kv->set_key("key1");
        kv->set_value("value");
        kv = log->add_contents();
        kv->set_key("key2");
        kv->set_value("");
        kv = log->add_contents();
        kv->set_key("key3");
        kv->set_value("value3");
        kv = log->add_contents();
        kv->set_key("key4");
        kv->set_value("value");
        log->set_time(nowTime);


        RawLogGroup* pRawLogGroup = new RawLogGroup;
        pRawLogGroup->set_source("192.168.1.1");
        loggroup.set_source("192.168.1.1");

        pRawLogGroup->set_topic("topic");
        loggroup.set_topic("topic");

        pRawLogGroup->add_logtags("tagkey1", "tagvalue1");
        pRawLogGroup->add_logtags("tagkey2", "tagvalue2");

        auto tag1 = loggroup.add_logtags();
        tag1->set_key("tagkey1");
        tag1->set_value("tagvalue1");

        auto tag2 = loggroup.add_logtags();
        tag2->set_key("tagkey2");
        tag2->set_value("tagvalue2");

        string rawLogStr;
        pRawLogGroup->add_logs(pRawLog);
        pRawLogGroup->AppendToString(&rawLogStr);
        string logStr;
        EXPECT_EQ(loggroup.AppendToString(&logStr), true);
        EXPECT_EQ(rawLogStr, logStr);
        delete pRawLogGroup;
    }

    void TestMultiLog() {
        int32_t nowTime = time(NULL);
        string rawLogStr;
        auto c1 = clock();
        for (int i = 0; i < 1000000; ++i) {
            RawLog rawLog;

            rawLog.AddLogStart(nowTime);
            rawLog.AddKeyValue("key1", strlen("key1"), "value", strlen("value"));
            rawLog.AddKeyValue("key2", strlen("key2"), "", 0);
            rawLog.AddKeyValue("key3", strlen("key3"), "value3", strlen("value3"));
            rawLog.AddKeyValue("key4", strlen("key4"), "value", strlen("value"));
            rawLog.AddLogDone();
            rawLogStr.clear();
            rawLog.AppendToString(&rawLogStr);
        }
        auto c2 = clock();
        string logStr;
        for (int i = 0; i < 1000000; ++i) {
            LogGroup loggroup;
            Log* log = loggroup.add_logs();
            Log_Content* kv = log->add_contents();
            kv->set_key("key1");
            kv->set_value("value");
            kv = log->add_contents();
            kv->set_key("key2");
            kv->set_value("");
            kv = log->add_contents();
            kv->set_key("key3");
            kv->set_value("value3");
            kv = log->add_contents();
            kv->set_key("key4");
            kv->set_value("value");
            log->set_time(nowTime);
            logStr.clear();
            loggroup.AppendToString(&logStr);
        }
        auto c3 = clock();
        EXPECT_GT((c3 - c2) / 5, c2 - c1);
        printf("%d %d \n", (int)(c3 - c2), (int)(c2 - c1));
        EXPECT_EQ(rawLogStr, logStr);
    }
};

APSARA_UNIT_TEST_CASE(PBUnittest, TestFullWrite, 0);
APSARA_UNIT_TEST_CASE(PBUnittest, TestNormalWrite, 0);
APSARA_UNIT_TEST_CASE(PBUnittest, TestIterator, 0);
APSARA_UNIT_TEST_CASE(PBUnittest, TestUpdateDeleteAppend, 0);
APSARA_UNIT_TEST_CASE(PBUnittest, TestBigLog, 0);
APSARA_UNIT_TEST_CASE(PBUnittest, TestLogGroup, 0);
APSARA_UNIT_TEST_CASE(PBUnittest, TestNoOptionLogGroup, 0);
APSARA_UNIT_TEST_CASE(PBUnittest, TestMultiLog, 0);

} // namespace logtail


int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
