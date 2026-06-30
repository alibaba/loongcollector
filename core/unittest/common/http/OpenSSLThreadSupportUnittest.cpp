// Copyright 2024 iLogtail Authors
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

#include <openssl/crypto.h>
#include <openssl/err.h>

#include <atomic>
#include <thread>
#include <vector>

#include "common/http/Curl.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class OpenSSLThreadSupportUnittest : public ::testing::Test {
public:
    // After setup, OpenSSL < 1.1.0 must have a locking/thread-id callback
    // installed; otherwise concurrent SSL usage corrupts the global ERR state.
    void TestSetupRegistersCallbacks();
    // Calling the setup repeatedly must be safe.
    void TestSetupIsIdempotent();
    // Regression: concurrently churning OpenSSL's per-thread error state from
    // many threads must not crash once thread support is installed. Without the
    // locking callbacks this reproduces the SIGSEGV in lh_insert / lh_delete.
    void TestConcurrentErrorStateNoCrash();
};

void OpenSSLThreadSupportUnittest::TestSetupRegistersCallbacks() {
    SetupOpenSSLThreadSupport();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    APSARA_TEST_TRUE(CRYPTO_get_locking_callback() != nullptr);
    APSARA_TEST_TRUE(CRYPTO_THREADID_get_callback() != nullptr);
#else
    // OpenSSL >= 1.1.0 manages locking internally; setup is a no-op and must
    // simply not crash.
    APSARA_TEST_TRUE(true);
#endif
}

void OpenSSLThreadSupportUnittest::TestSetupIsIdempotent() {
    SetupOpenSSLThreadSupport();
    SetupOpenSSLThreadSupport();
    SetupOpenSSLThreadSupport();
    APSARA_TEST_TRUE(true);
}

void OpenSSLThreadSupportUnittest::TestConcurrentErrorStateNoCrash() {
    SetupOpenSSLThreadSupport();

    constexpr int kThreadCount = 8;
    constexpr int kIterations = 2000;
    std::atomic<int> finished{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (int i = 0; i < kThreadCount; ++i) {
        workers.emplace_back([&finished, kIterations]() {
            for (int j = 0; j < kIterations; ++j) {
                // Touch the global per-thread error-state hash (insert path).
                ERR_put_error(ERR_LIB_SYS, 0, 1, "OpenSSLThreadSupportUnittest", 0);
                ERR_clear_error();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
                // Delete path: removes this thread's entry from the global hash.
                ERR_remove_thread_state(nullptr);
#endif
            }
            ++finished;
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    APSARA_TEST_EQUAL(kThreadCount, finished.load());
}

UNIT_TEST_CASE(OpenSSLThreadSupportUnittest, TestSetupRegistersCallbacks)
UNIT_TEST_CASE(OpenSSLThreadSupportUnittest, TestSetupIsIdempotent)
UNIT_TEST_CASE(OpenSSLThreadSupportUnittest, TestConcurrentErrorStateNoCrash)

} // namespace logtail

UNIT_TEST_MAIN
