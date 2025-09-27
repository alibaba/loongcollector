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

#include <string>

#include "plugin/flusher/kafka/KafkaConfig.h"
#include "plugin/flusher/kafka/KafkaProducer.h"
#include "unittest/Unittest.h"

namespace logtail {

class KafkaTlsUnittest : public ::testing::Test {
public:
    void TestInitWithTLSMinimal();
    void TestInitWithTLSFullPaths();
};

void KafkaTlsUnittest::TestInitWithTLSMinimal() {
    KafkaConfig cfg;
    cfg.Brokers = {"localhost:9092"};
    cfg.Topic = "tls-test";
    cfg.Version = "2.6.0";
    cfg.EnableTLS = true;

    KafkaProducer p;
    APSARA_TEST_TRUE(p.Init(cfg));
    p.Close();
}

void KafkaTlsUnittest::TestInitWithTLSFullPaths() {
    KafkaConfig cfg;
    cfg.Brokers = {"localhost:9092"};
    cfg.Topic = "tls-test";
    cfg.Version = "2.6.0";
    cfg.EnableTLS = true;
    cfg.TLSCaFile = "/tmp/does-not-need-to-exist.ca";
    cfg.TLSCertFile = "/tmp/does-not-need-to-exist.crt";
    cfg.TLSKeyFile = "/tmp/does-not-need-to-exist.key";
    cfg.TLSKeyPassword = "secret";

    KafkaProducer p;
    APSARA_TEST_TRUE(p.Init(cfg));
    p.Close();
}

UNIT_TEST_CASE(KafkaTlsUnittest, TestInitWithTLSMinimal)
UNIT_TEST_CASE(KafkaTlsUnittest, TestInitWithTLSFullPaths)

} // namespace logtail

UNIT_TEST_MAIN
