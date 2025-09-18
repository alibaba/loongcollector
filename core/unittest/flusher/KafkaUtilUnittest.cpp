/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef APSARA_UNIT_TEST_MAIN
#define APSARA_UNIT_TEST_MAIN
#endif

#include <cassert>

#include <string>
#include <vector>

#include "plugin/flusher/kafka/KafkaConstant.h"
#include "plugin/flusher/kafka/KafkaUtil.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class KafkaUtilUnittest : public ::testing::Test {
public:
    void TestBrokersToStringEmpty();
    void TestBrokersToStringSingle();
    void TestBrokersToStringMultiple();

    void TestParseKafkaVersionValid();
    void TestParseKafkaVersionInvalid();
    void TestDeriveConfigs_GE_010();
    void TestDeriveConfigs_090x();
    void TestDeriveConfigs_08xy();
    void TestDeriveConfigs_EmptyOrInvalid();
};

void KafkaUtilUnittest::TestBrokersToStringEmpty() {
    vector<string> emptyBrokers;
    string result = KafkaUtil::BrokersToString(emptyBrokers);
    APSARA_TEST_EQUAL("", result);
}

void KafkaUtilUnittest::TestBrokersToStringSingle() {
    vector<string> singleBroker = {"localhost:9092"};
    string result = KafkaUtil::BrokersToString(singleBroker);
    APSARA_TEST_EQUAL("localhost:9092", result);
}

void KafkaUtilUnittest::TestBrokersToStringMultiple() {
    vector<string> multipleBrokers = {"broker1:9092", "broker2:9092", "broker3:9092"};
    string result = KafkaUtil::BrokersToString(multipleBrokers);
    APSARA_TEST_EQUAL("broker1:9092,broker2:9092,broker3:9092", result);
}

void KafkaUtilUnittest::TestParseKafkaVersionValid() {
    KafkaUtil::Version v{};
    APSARA_TEST_TRUE(KafkaUtil::ParseKafkaVersion("0.8.2.2", v));
    APSARA_TEST_EQUAL(0, v.major);
    APSARA_TEST_EQUAL(8, v.minor);
    APSARA_TEST_EQUAL(2, v.patch);
    APSARA_TEST_EQUAL(2, v.build);

    APSARA_TEST_TRUE(KafkaUtil::ParseKafkaVersion("0.9.0.1", v));
    APSARA_TEST_EQUAL(0, v.major);
    APSARA_TEST_EQUAL(9, v.minor);
    APSARA_TEST_EQUAL(0, v.patch);
    APSARA_TEST_EQUAL(1, v.build);

    APSARA_TEST_TRUE(KafkaUtil::ParseKafkaVersion("0.10.2.1", v));
    APSARA_TEST_EQUAL(0, v.major);
    APSARA_TEST_EQUAL(10, v.minor);
    APSARA_TEST_EQUAL(2, v.patch);
    APSARA_TEST_EQUAL(1, v.build);

    APSARA_TEST_TRUE(KafkaUtil::ParseKafkaVersion("2.6.0", v));
    APSARA_TEST_EQUAL(2, v.major);
    APSARA_TEST_EQUAL(6, v.minor);
    APSARA_TEST_EQUAL(0, v.patch);
    APSARA_TEST_EQUAL(0, v.build);

    APSARA_TEST_TRUE(KafkaUtil::ParseKafkaVersion("3.7.0", v));
    APSARA_TEST_EQUAL(3, v.major);
    APSARA_TEST_EQUAL(7, v.minor);
    APSARA_TEST_EQUAL(0, v.patch);
    APSARA_TEST_EQUAL(0, v.build);

    APSARA_TEST_TRUE(KafkaUtil::ParseKafkaVersion("4.0.0", v));
    APSARA_TEST_EQUAL(4, v.major);
}

void KafkaUtilUnittest::TestParseKafkaVersionInvalid() {
    KafkaUtil::Version v{};
    APSARA_TEST_FALSE(KafkaUtil::ParseKafkaVersion("", v));
    APSARA_TEST_FALSE(KafkaUtil::ParseKafkaVersion("abc", v));
    APSARA_TEST_FALSE(KafkaUtil::ParseKafkaVersion("1..2", v));
    APSARA_TEST_FALSE(KafkaUtil::ParseKafkaVersion("1.2.3.4.5", v));
}

void KafkaUtilUnittest::TestDeriveConfigs_GE_010() {
    std::map<std::string, std::string> out;
    KafkaUtil::DeriveApiVersionConfigs("0.10.0.0", out);
    APSARA_TEST_TRUE(out.count(KAFKA_CONFIG_API_VERSION_REQUEST) == 1);
    APSARA_TEST_EQUAL(std::string("true"), out[KAFKA_CONFIG_API_VERSION_REQUEST]);
    APSARA_TEST_TRUE(out.count(KAFKA_CONFIG_API_VERSION_FALLBACK_MS) == 1);
    APSARA_TEST_EQUAL(std::string("0"), out[KAFKA_CONFIG_API_VERSION_FALLBACK_MS]);
    APSARA_TEST_TRUE(out.count(KAFKA_CONFIG_BROKER_VERSION_FALLBACK) == 0);

    out.clear();
    KafkaUtil::DeriveApiVersionConfigs("2.6.0", out);
    APSARA_TEST_TRUE(out.count(KAFKA_CONFIG_API_VERSION_REQUEST) == 1);
}

void KafkaUtilUnittest::TestDeriveConfigs_090x() {
    std::map<std::string, std::string> out;
    KafkaUtil::DeriveApiVersionConfigs("0.9.0.1", out);
    APSARA_TEST_TRUE(out.count(KAFKA_CONFIG_API_VERSION_REQUEST) == 1);
    APSARA_TEST_EQUAL(std::string("false"), out[KAFKA_CONFIG_API_VERSION_REQUEST]);
    APSARA_TEST_TRUE(out.count(KAFKA_CONFIG_BROKER_VERSION_FALLBACK) == 1);
    APSARA_TEST_EQUAL(std::string("0.9.0.1"), out[KAFKA_CONFIG_BROKER_VERSION_FALLBACK]);
}

void KafkaUtilUnittest::TestDeriveConfigs_08xy() {
    std::map<std::string, std::string> out;
    KafkaUtil::DeriveApiVersionConfigs("0.8.2.2", out);
    APSARA_TEST_TRUE(out.count(KAFKA_CONFIG_API_VERSION_REQUEST) == 1);
    APSARA_TEST_EQUAL(std::string("false"), out[KAFKA_CONFIG_API_VERSION_REQUEST]);
    APSARA_TEST_TRUE(out.count(KAFKA_CONFIG_BROKER_VERSION_FALLBACK) == 1);
    APSARA_TEST_EQUAL(std::string("0.8.2.2"), out[KAFKA_CONFIG_BROKER_VERSION_FALLBACK]);
}

void KafkaUtilUnittest::TestDeriveConfigs_EmptyOrInvalid() {
    std::map<std::string, std::string> out;
    KafkaUtil::DeriveApiVersionConfigs("", out);
    APSARA_TEST_TRUE(out.empty());

    out.clear();
    KafkaUtil::DeriveApiVersionConfigs("abc", out);
    APSARA_TEST_TRUE(out.empty());
}

UNIT_TEST_CASE(KafkaUtilUnittest, TestBrokersToStringEmpty)
UNIT_TEST_CASE(KafkaUtilUnittest, TestBrokersToStringSingle)
UNIT_TEST_CASE(KafkaUtilUnittest, TestBrokersToStringMultiple)
UNIT_TEST_CASE(KafkaUtilUnittest, TestParseKafkaVersionValid)
UNIT_TEST_CASE(KafkaUtilUnittest, TestParseKafkaVersionInvalid)
UNIT_TEST_CASE(KafkaUtilUnittest, TestDeriveConfigs_GE_010)
UNIT_TEST_CASE(KafkaUtilUnittest, TestDeriveConfigs_090x)
UNIT_TEST_CASE(KafkaUtilUnittest, TestDeriveConfigs_08xy)
UNIT_TEST_CASE(KafkaUtilUnittest, TestDeriveConfigs_EmptyOrInvalid)

} // namespace logtail

UNIT_TEST_MAIN
