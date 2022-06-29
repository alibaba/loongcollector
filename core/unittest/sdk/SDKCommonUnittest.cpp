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
#include "sdk/Common.h"
#include "sdk/Client.h"
#include "sdk/Exception.h"
#include "sender/Sender.h"
#include "common/CompressTools.h"
#include "sls_control/SLSControl.h"

namespace logtail {

class HttpMessageUnittest : public ::testing::Test {
public:
    void TestGetServerTimeFromHeader();
};

UNIT_TEST_CASE(HttpMessageUnittest, TestGetServerTimeFromHeader);

void HttpMessageUnittest::TestGetServerTimeFromHeader() {
    sdk::HttpMessage httpMsg;
    EXPECT_EQ(0, httpMsg.GetServerTimeFromHeader());

    auto& header = httpMsg.header;
    header["Date"] = "Thu, 18 Feb 2021 10:11:10 GMT";
    EXPECT_EQ(1613643070, httpMsg.GetServerTimeFromHeader());

    const time_t kTimestamp = 1613588970;
    header["x-log-time"] = std::to_string(kTimestamp);
    EXPECT_EQ(kTimestamp, httpMsg.GetServerTimeFromHeader());
}

class SDKClientUnittest : public ::testing::Test {};

TEST_F(SDKClientUnittest, TestNetwork) {
    sdk::Client client("log-global.aliyuncs.com",
                       STRING_FLAG(default_access_key_id),
                       STRING_FLAG(default_access_key),
                       INT32_FLAG(sls_client_send_timeout),
                       "192.168.1.1",
                       BOOL_FLAG(sls_client_send_compress),
                       "");
    try {
        client.TestNetwork();
        ASSERT_TRUE(false);
    } catch (const sdk::LOGException& e) {
        const std::string& errorCode = e.GetErrorCode();
        ASSERT_EQ(errorCode, sdk::LOGE_REQUEST_ERROR);
        std::cout << "ErrorMessage: " << e.GetMessage_() << std::endl;
    }

    // Machine to run the test might have accesibility to Internet.
    client.SetSlsHost("cn-hangzhou.log.aliyuncs.com");
    try {
        client.TestNetwork();
        ASSERT_TRUE(false);
    } catch (const sdk::LOGException& e) {
        const std::string errorCode = e.GetErrorCode();
        std::cout << errorCode << std::endl;
        std::cout << e.GetMessage_() << std::endl;
        if (e.GetHttpCode() == 404) {
            EXPECT_EQ(errorCode, sdk::LOGE_PROJECT_NOT_EXIST);
        } else if (e.GetHttpCode() == 401) {
            EXPECT_EQ(ConvertErrorCode(errorCode), SEND_UNAUTHORIZED);
        } else if (e.GetHttpCode() == 400) {
            EXPECT_EQ(ConvertErrorCode(errorCode), SEND_DISCARD_ERROR);
        } else {
            std::cout << "HttpCode: " << e.GetHttpCode() << std::endl;
            EXPECT_EQ(ConvertErrorCode(errorCode), SEND_NETWORK_ERROR);
        }
    }
}

TEST_F(SDKClientUnittest, TestGetRealIp) {
    sdk::Client client("cn-shanghai-corp.sls.aliyuncs.com",
                       STRING_FLAG(default_access_key_id),
                       STRING_FLAG(default_access_key),
                       INT32_FLAG(sls_client_send_timeout),
                       "192.168.1.1",
                       BOOL_FLAG(sls_client_send_compress),
                       "");
    logtail::sdk::GetRealIpResponse resp = client.GetRealIp();
    std::cout << "realIp: " << resp.realIp << std::endl;
    EXPECT_GT(resp.realIp.size(), 0L);

    client.SetSlsHost("cn-shanghai.sls.aliyuncs.com");
    resp = client.GetRealIp();
    std::cout << "realIp: " << resp.realIp << std::endl;
    EXPECT_EQ(resp.realIp.size(), 0L);
}

/*
TEST_F(SDKClientUnittest, PostLogstoreLogsSuccessOpenSource)
{
    std::string uid = "";
    std::string accessKeyId = "";
    std::string accessKey = "";
    std::string project = "";
    std::string logstore = "";
    sdk::Client client("cn-huhehaote.log.aliyuncs.com",
                       accessKeyId,
                       accessKey,
                       INT32_FLAG(sls_client_send_timeout),
                       "192.168.1.1",
                       BOOL_FLAG(sls_client_send_compress),
                       "");
    try
    {
        sls_logs::LogGroup logGroup;
        
        logGroup.set_source("192.168.1.1");
        logGroup.set_category(logstore);
        logGroup.set_topic("unittest");
        
        sls_logs::Log* log = logGroup.add_logs();
        log->set_time(time(NULL));
        sls_logs::Log_Content* content = nullptr;
        content = log->add_contents();
        content->set_key("kk1");
        content->set_value("vv1");
        content = log->add_contents();
        content->set_key("kk2");
        content->set_value("vv2");
        
        std::string oriData;
        logGroup.SerializeToString(&oriData);
        int32_t logSize = (int32_t) logGroup.logs_size();
        time_t curTime = time(NULL);
        
        LoggroupTimeValue* data = new LoggroupTimeValue(
            project, logstore, "ut-config", "ut.log", false, 
            uid, "cn-huhehaote", LOGGROUP_LZ4_COMPRESSED,
            logSize, oriData.size(), curTime, "", 0);
        
        ASSERT_TRUE(CompressLz4(oriData, data->mLogData));
        
        sdk::PostLogStoreLogsResponse resp = client.PostLogStoreLogs(
            data->mProjectName,
            data->mLogstore,
            data->mLogData,
            data->mRawSize
        );
        std::cout << resp.requestId << "," << resp.statusCode << "," << resp.bodyBytes;
    }
    catch (const sdk::LOGException &e)
    {
        const std::string &errorCode = e.GetErrorCode();
        std::cout << errorCode << "===" << e.GetMessage_() << std::endl;
        ASSERT_TRUE(false);
    }
}

TEST_F(SDKClientUnittest, PostLogstoreLogsSuccessClosedSource)
{
    std::string uid = "";
    std::string accessKeyId = ""; // starting with #
    std::string accessKey = "";
    std::string project = "";
    std::string logstore = "";
    sdk::Client client("cn-huhehaote.log.aliyuncs.com",
                       accessKeyId,
                       accessKey,
                       INT32_FLAG(sls_client_send_timeout),
                       "192.168.1.1",
                       BOOL_FLAG(sls_client_send_compress),
                       "");
    SLSControl::Instance()->SetSlsSendClientCommonParam(&client);
    try
    {
        sls_logs::LogGroup logGroup;
        
        logGroup.set_source("192.168.1.1");
        logGroup.set_category(logstore);
        logGroup.set_topic("unittest");
        
        sls_logs::Log* log = logGroup.add_logs();
        log->set_time(time(NULL));
        sls_logs::Log_Content* content = nullptr;
        content = log->add_contents();
        content->set_key("kk1");
        content->set_value("vv1");
        content = log->add_contents();
        content->set_key("kk2");
        content->set_value("vv2");
        
        std::string oriData;
        logGroup.SerializeToString(&oriData);
        int32_t logSize = (int32_t) logGroup.logs_size();
        time_t curTime = time(NULL);
        
        LoggroupTimeValue* data = new LoggroupTimeValue(
            project, logstore, "ut-config", "ut.log", false, 
            uid, "cn-huhehaote", LOGGROUP_LZ4_COMPRESSED,
            logSize, oriData.size(), curTime, "", 0);
        
        ASSERT_TRUE(CompressLz4(oriData, data->mLogData));
        
        sdk::PostLogStoreLogsResponse resp = client.PostLogStoreLogs(
            data->mProjectName,
            data->mLogstore,
            data->mLogData,
            data->mRawSize
        );
        std::cout << resp.requestId << "," << resp.statusCode << "," << resp.bodyBytes;
    }
    catch (const sdk::LOGException &e)
    {
        const std::string &errorCode = e.GetErrorCode();
        std::cout << errorCode << "===" << e.GetMessage_() << std::endl;
        ASSERT_TRUE(false);
    }
}
*/
} // namespace logtail

UNIT_TEST_MAIN
