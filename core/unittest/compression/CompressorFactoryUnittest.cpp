// Copyright 2024 iLogtail Authors
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

#include "compression/CompressorFactory.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class CompressorFactoryUnittest : public ::testing::Test {
public:
    void TestCreate();
    void TestCompressTypeToString();

protected:
    void SetUp() { mCtx.SetConfigName("test_config"); }

private:
    PipelineContext mCtx;
};

void CompressorFactoryUnittest::TestCreate() {
    {
        // use default
        auto compressor
            = CompressorFactory::GetInstance()->Create(Json::Value(), mCtx, "test_plugin", CompressType::LZ4);
        APSARA_TEST_EQUAL(CompressType::LZ4, compressor->GetCompressType());
    }
    {
        // lz4
        Json::Value config;
        config["CompressType"] = "lz4";
        auto compressor = CompressorFactory::GetInstance()->Create(config, mCtx, "test_plugin", CompressType::ZSTD);
        APSARA_TEST_EQUAL(CompressType::LZ4, compressor->GetCompressType());
    }
    {
        // zstd
        Json::Value config;
        config["CompressType"] = "zstd";
        auto compressor = CompressorFactory::GetInstance()->Create(config, mCtx, "test_plugin", CompressType::LZ4);
        APSARA_TEST_EQUAL(CompressType::ZSTD, compressor->GetCompressType());
    }
    {
        // none
        Json::Value config;
        config["CompressType"] = "none";
        auto compressor = CompressorFactory::GetInstance()->Create(config, mCtx, "test_plugin", CompressType::LZ4);
        APSARA_TEST_EQUAL(nullptr, compressor);
    }
    {
        // unknown
        Json::Value config;
        config["CompressType"] = "unknown";
        auto compressor
            = CompressorFactory::GetInstance()->Create(Json::Value(), mCtx, "test_plugin", CompressType::LZ4);
        APSARA_TEST_EQUAL(CompressType::LZ4, compressor->GetCompressType());
    }
    {
        // invalid
        Json::Value config;
        config["CompressType"] = 123;
        auto compressor
            = CompressorFactory::GetInstance()->Create(Json::Value(), mCtx, "test_plugin", CompressType::LZ4);
        APSARA_TEST_EQUAL(CompressType::LZ4, compressor->GetCompressType());
    }
}

void CompressorFactoryUnittest::TestCompressTypeToString() {
    APSARA_TEST_STREQ("lz4", CompressTypeToString(CompressType::LZ4).data());
    APSARA_TEST_STREQ("zstd", CompressTypeToString(CompressType::ZSTD).data());
    APSARA_TEST_STREQ("none", CompressTypeToString(CompressType::NONE).data());
}

UNIT_TEST_CASE(CompressorFactoryUnittest, TestCreate)
UNIT_TEST_CASE(CompressorFactoryUnittest, TestCompressTypeToString)

} // namespace logtail

UNIT_TEST_MAIN
