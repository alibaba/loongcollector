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

class KafkaKerberosUnittest : public ::testing::Test {
public:
    void TestInitWithKerberosMinimal();
    void TestInitWithKerberosAndTLS();
    void TestInitWithKerberosFull();
};

void KafkaKerberosUnittest::TestInitWithKerberosMinimal() {
    KafkaConfig cfg;
    cfg.Brokers = {"localhost:9092"};
    cfg.Topic = "krb-test";
    cfg.Version = "2.6.0";
    cfg.EnableKerberos = true;
    // default mechanism GSSAPI, service name kafka

    KafkaProducer p;
    APSARA_TEST_TRUE(p.Init(cfg));
    p.Close();
}

void KafkaKerberosUnittest::TestInitWithKerberosAndTLS() {
    KafkaConfig cfg;
    cfg.Brokers = {"localhost:9092"};
    cfg.Topic = "krb-test";
    cfg.Version = "2.6.0";
    cfg.EnableKerberos = true;
    cfg.EnableTLS = true;
    cfg.TLSCaFile = "/tmp/ca.pem"; // path existence is not validated at this stage

    KafkaProducer p;
    APSARA_TEST_TRUE(p.Init(cfg));
    p.Close();
}

void KafkaKerberosUnittest::TestInitWithKerberosFull() {
    KafkaConfig cfg;
    cfg.Brokers = {"localhost:9092"};
    cfg.Topic = "krb-test";
    cfg.Version = "2.6.0";
    cfg.EnableKerberos = true;
    cfg.SaslMechanisms = "GSSAPI";
    cfg.KerberosServiceName = "kafka";
    cfg.KerberosPrincipal = "kafka/test@EXAMPLE.COM";
    cfg.KerberosKeytab = "/tmp/test.keytab";
    cfg.KerberosKinitCmd = "kinit -k -t %{sasl.kerberos.keytab} %{sasl.kerberos.principal}";

    KafkaProducer p;
    APSARA_TEST_TRUE(p.Init(cfg));
    p.Close();
}

UNIT_TEST_CASE(KafkaKerberosUnittest, TestInitWithKerberosMinimal)
UNIT_TEST_CASE(KafkaKerberosUnittest, TestInitWithKerberosAndTLS)
UNIT_TEST_CASE(KafkaKerberosUnittest, TestInitWithKerberosFull)

} // namespace logtail

UNIT_TEST_MAIN
