// Copyright 2025 iLogtail Authors
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

#include "common/magic_enum.hpp"
#include "ebpf/driver/BPFMapTraits.h"
#include "ebpf/driver/CallName.h"
#include "ebpf/driver/EbpfDriver.h"
#include "ebpf/driver/FileFilter.h"
#include "ebpf/driver/NetworkFilter.h"
#include "ebpf/driver/eBPFWrapper.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class eBPFDriverUnittest : public ::testing::Test {
public:
    void TestCallNameIdx();
    void TestNetworkFilter();
    void TestFileFilter();

    void TestStartPlugin();

protected:
    void SetUp() override {}
    void TearDown() override {}
};

void eBPFDriverUnittest::TestCallNameIdx() {
    APSARA_TEST_EQUAL(GetCallNameIdx("security_file_permission"),
                      secure_funcs::SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION);
    APSARA_TEST_EQUAL(GetCallNameIdx("security_mmap_file"),
                      secure_funcs::SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_MMAP_FILE);
    APSARA_TEST_EQUAL(GetCallNameIdx("security_path_truncate"),
                      secure_funcs::SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE);
    APSARA_TEST_EQUAL(GetCallNameIdx("sys_write"), secure_funcs::SECURE_FUNC_TRACEPOINT_FUNC_SYS_WRITE);
    APSARA_TEST_EQUAL(GetCallNameIdx("sys_read"), secure_funcs::SECURE_FUNC_TRACEPOINT_FUNC_SYS_READ);
    APSARA_TEST_EQUAL(GetCallNameIdx("tcp_close"), secure_funcs::SECURE_FUNC_TRACEPOINT_FUNC_TCP_CLOSE);
    APSARA_TEST_EQUAL(GetCallNameIdx("tcp_connect"), secure_funcs::SECURE_FUNC_TRACEPOINT_FUNC_TCP_CONNECT);
    APSARA_TEST_EQUAL(GetCallNameIdx("tcp_sendmsg"), secure_funcs::SECURE_FUNC_TRACEPOINT_FUNC_TCP_SENDMSG);
}

UNIT_TEST_CASE(eBPFDriverUnittest, TestCallNameIdx);
// UNIT_TEST_CASE(eBPFDriverUnittest, TestNetDataHandler);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
