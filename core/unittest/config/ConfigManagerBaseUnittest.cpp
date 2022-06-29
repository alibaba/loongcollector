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
#include <assert.h>
#if defined(__linux__)
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <fnmatch.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <typeinfo>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <set>
#include <json/json.h>
#include "common/Flags.h"
#include "common/FileSystemUtil.h"
#include "common/util.h"
#include "event_handler/EventHandler.h"
#include "polling/PollingEventQueue.h"
#include "controller/EventDispatcher.h"
#include "app_config/AppConfig.h"
#include "config_manager/ConfigManagerBase.h"
#include "reader/LogFileReader.h"
#include "event_handler/LogInput.h"
#include "event/Event.h"
#include "logger/Logger.h"

namespace logtail {

class ConfigManagerBaseUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}

    void CaseSetUp() {}
    void CaseCleanUp() {}

    void TestReplaceEnvVarRefInStr() {
        LOG_INFO(sLogger, ("TestReplaceEnvVarRefInStr() begin", time(NULL)));
        // test empty string
        std::string str{""};
        APSARA_TEST_EQUAL(logtail::replaceEnvVarRefInStr(str), "");

#if defined(__linux__)
        str = "${__path4ut}/${__file4ut:*.log}";
        unsetenv("__path4ut");
        unsetenv("__file4ut");
        APSARA_TEST_EQUAL(logtail::replaceEnvVarRefInStr(str), "/*.log");
        //APSARA_TEST_TRUE_DESC(pwItr != EventDispatcher::GetInstance()->mPathWdMap.end(), dir);
        setenv("__path4ut", "/_home/$work", 1);
        setenv("__file4ut", "!transaction/~un-do.log", 1);
        APSARA_TEST_EQUAL(logtail::replaceEnvVarRefInStr(str), "/_home/$work/!transaction/~un-do.log");
#else //windows
        str = "${__path4ut}\\${__file4ut:*.log}";
        _putenv_s("__path4ut", "");
        _putenv_s("__file4ut", "");
        APSARA_TEST_EQUAL(logtail::replaceEnvVarRefInStr(str), "\\*.log");
        //APSARA_TEST_TRUE_DESC(pwItr != EventDispatcher::GetInstance()->mPathWdMap.end(), dir);
        _putenv_s("__path4ut", "C:\\Users\\$work");
        _putenv_s("__file4ut", "!transaction\\~un-do.log");
        APSARA_TEST_EQUAL(logtail::replaceEnvVarRefInStr(str), "C:\\Users\\$work\\!transaction\\~un-do.log");
#endif
        // test unescape function
        str = "$${__path4ut}/${__file4ut:*.log$}";
        APSARA_TEST_EQUAL(logtail::replaceEnvVarRefInStr(str), "${__path4ut}/${__file4ut:*.log}");
        LOG_INFO(sLogger, ("TestReplaceEnvVarRefInStr() end", time(NULL)));
    }

    void TestReplaceEnvVarRefInConf() {
        LOG_INFO(sLogger, ("TestReplaceEnvVarRefInConf() begin", time(NULL)));
        std::string configString = R""""({
	"metrics" :
	{
		"##1.0##unit-test$apache" :
		{
			"advanced" :
			{
				"blacklist" :
				{
					"dir_blacklist" :
					[
						"/${__path4ut}/tmp"
					]
				}
			},
            "file_pattern" : "${__file4ut}",
			"log_path" : "/${__path4ut}"
		}
    }
})"""";
        std::string answer = R""""({
    "metrics" : 
    {
        "##1.0##unit-test$apache" : 
        {
            "advanced" : 
            {
                "blacklist" : 
                {
                    "dir_blacklist" : 
                    [
                        "/_home/$work/tmp"
                    ]
                }
            },
            "file_pattern" : "!transaction/~un-do.log",
            "log_path" : "/_home/$work"
        }
    }
})"""";
        // TODO: Add for Windows.
        // _putenv_s
        Json::Reader reader;
        Json::Value root;
        APSARA_TEST_TRUE(reader.parse(configString, root));
#if defined(__linux__)
        setenv("__path4ut", "_home/$work", 1);
        setenv("__file4ut", "!transaction/~un-do.log", 1);
#else
        _putenv_s("__path4ut", "_home/$work");
        _putenv_s("__file4ut", "!transaction/~un-do.log");
#endif
        logtail::ReplaceEnvVarRefInConf(root);
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "    ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        std::ostringstream oss;
        writer->write(root, &oss);
        APSARA_TEST_EQUAL(oss.str(), answer);
        LOG_INFO(sLogger, ("TestReplaceEnvVarRefInConf() end", time(NULL)));
    }
};

TEST_F(ConfigManagerBaseUnittest, TestReplaceEnvVarRefInStr) {
    TestReplaceEnvVarRefInStr();
}

TEST_F(ConfigManagerBaseUnittest, TestReplaceEnvVarRefInConf) {
    TestReplaceEnvVarRefInConf();
}

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
