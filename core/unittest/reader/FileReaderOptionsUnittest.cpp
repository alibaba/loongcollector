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

#include <json/json.h>

#include "unittest/Unittest.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "pipeline/PipelineContext.h"
#include "reader/FileReaderOptions.h"

DECLARE_FLAG_INT32(default_tail_limit_kb);
DECLARE_FLAG_INT32(default_reader_flush_timeout);
DECLARE_FLAG_INT32(delay_bytes_upperlimit);
DECLARE_FLAG_INT32(reader_close_unused_file_time);
DECLARE_FLAG_INT32(logreader_max_rotate_queue_size);

using namespace std;

namespace logtail {

class FileReaderOptionsUnittest : public testing::Test {
public:
    void OnSuccessfulInit() const;
    void OnFailedInit() const;

private:
    const string pluginName = "test";
    PipelineContext ctx;
};

void FileReaderOptionsUnittest::OnSuccessfulInit() const {
    unique_ptr<FileReaderOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // only mandatory param
    config.reset(new FileReaderOptions());
    APSARA_TEST_EQUAL(FileReaderOptions::Encoding::UTF8, config->mFileEncoding);
    APSARA_TEST_FALSE(config->mTailingAllMatchedFiles);
    APSARA_TEST_EQUAL(INT32_FLAG(default_tail_limit_kb), config->mTailSizeKB);
    APSARA_TEST_EQUAL(INT32_FLAG(default_reader_flush_timeout), config->mFlushTimeoutSecs);
    APSARA_TEST_EQUAL(0, config->mReadDelaySkipThresholdBytes);
    APSARA_TEST_EQUAL(INT32_FLAG(delay_bytes_upperlimit), config->mReadDelayAlertThresholdBytes);
    APSARA_TEST_EQUAL(INT32_FLAG(reader_close_unused_file_time), config->mCloseUnusedReaderIntervalSec);
    APSARA_TEST_EQUAL(INT32_FLAG(logreader_max_rotate_queue_size), config->mRotatorQueueSize);
    APSARA_TEST_FALSE(config->mAppendingLogPositionMeta);

    // valid optional param
    configStr = R"(
        {
            "FileEncoding": "utf8",
            "TailingAllMatchedFiles": true,
            "TailSizeKB": 2048,
            "FlushTimeoutSecs": 2,
            "ReadDelaySkipThresholdBytes": 1000,
            "ReadDelayAlertThresholdBytes": 100,
            "CloseUnusedReaderIntervalSec": 10,
            "RotatorQueueSize": 15,
            "AppendingLogPositionMeta": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileReaderOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginName));
    APSARA_TEST_EQUAL(FileReaderOptions::Encoding::UTF8, config->mFileEncoding);
    APSARA_TEST_TRUE(config->mTailingAllMatchedFiles);
    APSARA_TEST_EQUAL(2048, config->mTailSizeKB);
    APSARA_TEST_EQUAL(2, config->mFlushTimeoutSecs);
    APSARA_TEST_EQUAL(1000, config->mReadDelaySkipThresholdBytes);
    APSARA_TEST_EQUAL(100, config->mReadDelayAlertThresholdBytes);
    APSARA_TEST_EQUAL(10, config->mCloseUnusedReaderIntervalSec);
    APSARA_TEST_EQUAL(15, config->mRotatorQueueSize);
    APSARA_TEST_TRUE(config->mAppendingLogPositionMeta);

    // invalid optional param (except for FileEcoding)
    configStr = R"(
        {
            "FileEncoding": "gbk",
            "TailingAllMatchedFiles": "true",
            "TailSizeKB": "2048",
            "FlushTimeoutSecs": "2",
            "ReadDelaySkipThresholdBytes": "1000",
            "ReadDelayAlertThresholdBytes": "100",
            "CloseUnusedReaderIntervalSec": "10",
            "RotatorQueueSize": "15",
            "AppendingLogPositionMeta": "true"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileReaderOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginName));
    APSARA_TEST_EQUAL(FileReaderOptions::Encoding::GBK, config->mFileEncoding);
    APSARA_TEST_FALSE(config->mTailingAllMatchedFiles);
    APSARA_TEST_EQUAL(INT32_FLAG(default_tail_limit_kb), config->mTailSizeKB);
    APSARA_TEST_EQUAL(INT32_FLAG(default_reader_flush_timeout), config->mFlushTimeoutSecs);
    APSARA_TEST_EQUAL(0, config->mReadDelaySkipThresholdBytes);
    APSARA_TEST_EQUAL(INT32_FLAG(delay_bytes_upperlimit), config->mReadDelayAlertThresholdBytes);
    APSARA_TEST_EQUAL(INT32_FLAG(reader_close_unused_file_time), config->mCloseUnusedReaderIntervalSec);
    APSARA_TEST_EQUAL(INT32_FLAG(logreader_max_rotate_queue_size), config->mRotatorQueueSize);
    APSARA_TEST_FALSE(config->mAppendingLogPositionMeta);

    // FileEncoding
    configStr = R"(
        {
            "FileEncoding": "utf16",
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileReaderOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginName));
    APSARA_TEST_EQUAL(FileReaderOptions::Encoding::UTF16, config->mFileEncoding);

    // TailSizeKB
    configStr = R"(
        {
            "FileEncoding": "gbk",
            "TailSizeKB": "200000000"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileReaderOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginName));
    APSARA_TEST_EQUAL(FileReaderOptions::Encoding::GBK, config->mFileEncoding);
    APSARA_TEST_EQUAL(INT32_FLAG(default_tail_limit_kb), config->mTailSizeKB);
}

void FileReaderOptionsUnittest::OnFailedInit() const {
    unique_ptr<FileReaderOptions> config;
    Json::Value configJson, extendedParamJson, extendedParams;
    string configStr, extendedParamStr, errorMsg;

    // FileEncoding
    configStr = R"(
        {
            "FileEncoding": "unknown"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileReaderOptions());
    APSARA_TEST_FALSE(config->Init(configJson, ctx, pluginName));

    configStr = R"(
        {
            "FileEncoding": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new FileReaderOptions());
    APSARA_TEST_FALSE(config->Init(configJson, ctx, pluginName));
}

UNIT_TEST_CASE(FileReaderOptionsUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FileReaderOptionsUnittest, OnFailedInit)

} // namespace logtail

UNIT_TEST_MAIN
