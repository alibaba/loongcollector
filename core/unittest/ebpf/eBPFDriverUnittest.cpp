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
#include "ebpf/driver/BPFWrapper.h"
#include "ebpf/driver/CallName.h"
// #include "ebpf/driver/FileFilter.h"
// #include "ebpf/driver/NetworkFilter.h"
#include "ebpf/driver/eBPFDriver.h"
#include "ebpf/include/export.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class eBPFDriverUnittest : public ::testing::Test {
public:
    void TestCallNameIdx();
    void TestNetworkFilter();
    void TestFileFilter();

    void TestStartPlugin();
    void TestPerfbufferManagement();

protected:
    void SetUp() override {
        mLogPrinter = [](int16_t level, const char* format, va_list args) -> int {
            eBPFLogType printLevel = (eBPFLogType)level;
            switch (printLevel) {
                case eBPFLogType::NAMI_LOG_TYPE_WARN:
                    if (!SHOULD_LOG_WARNING(sLogger)) {
                        return 0;
                    }
                    break;
                case eBPFLogType::NAMI_LOG_TYPE_DEBUG:
                    if (!SHOULD_LOG_DEBUG(sLogger)) {
                        return 0;
                    }
                    break;
                case eBPFLogType::NAMI_LOG_TYPE_INFO:
                    [[fallthrough]];
                default:
                    if (!SHOULD_LOG_INFO(sLogger)) {
                        return 0;
                    }
                    break;
            }
            char buffer[4096] = {0};
            vsnprintf(buffer, sizeof(buffer), format, args);
            buffer[sizeof(buffer) - 1] = '\0';
            switch (printLevel) {
                case eBPFLogType::NAMI_LOG_TYPE_WARN:
                    LOG_WARNING(sLogger, ("module", "eBPFDriver")("msg", buffer));
                    break;
                case eBPFLogType::NAMI_LOG_TYPE_INFO:
                    LOG_INFO(sLogger, ("module", "eBPFDriver")("msg", buffer));
                    break;
                case eBPFLogType::NAMI_LOG_TYPE_DEBUG:
                    LOG_DEBUG(sLogger, ("module", "eBPFDriver")("msg", buffer));
                    break;
                default:
                    LOG_INFO(sLogger, ("module", "eBPFDriver")("level", int(level))("msg", buffer));
                    break;
            }
            return 0;
        };
        set_logger(mLogPrinter);
    }
    void TearDown() override {}

    eBPFLogHandler mLogPrinter;
};

void eBPFDriverUnittest::TestNetworkFilter() {
    // selector_filters kernelFilters{};
    // SecurityNetworkFilter filter;
    // std::shared_ptr<BPFWrapper<security_bpf>> bw(nullptr);
    // // 16820416 --- 192.168.0.1
    // // 33597632 --- 192.168.0.2
    // filter.mSourceAddrList = {"192.168.0.1", "192.168.0.2/14"};
    // filter.mSourceAddrBlackList = {"192.168.0.1", "192.168.0.2/14"};
    // filter.mDestAddrList = {"192.168.0.1", "192.168.0.2/14"};
    // filter.mDestAddrBlackList = {"192.168.0.1", "192.168.0.2/14"};
    // filter.mSourcePortList = {1, 2, 3};
    // filter.mSourcePortBlackList = {2, 3, 4};
    // filter.mDestPortList = {1, 2, 3};
    // filter.mDestPortBlackList = {2, 3, 4};

    // SetSaddrFilter(bw, -1, kernelFilters, &filter);
    // auto gAddr4Filters = GetAddr4Filters();
    // APSARA_TEST_EQUAL(gAddr4Filters.size(), 2UL);
    // APSARA_TEST_EQUAL(gAddr4Filters[0].mOpType, OP_TYPE_IN);
    // APSARA_TEST_EQUAL(gAddr4Filters[0].mIdx, 0);
    // APSARA_TEST_EQUAL(gAddr4Filters[0].mArg4.addr, 16820416U);
    // APSARA_TEST_EQUAL(gAddr4Filters[0].mArg4.prefix, 32U);

    // APSARA_TEST_EQUAL(gAddr4Filters[1].mOpType, OP_TYPE_IN);
    // APSARA_TEST_EQUAL(gAddr4Filters[1].mIdx, 0);
    // APSARA_TEST_EQUAL(gAddr4Filters[1].mArg4.addr, 33597632U);
    // APSARA_TEST_EQUAL(gAddr4Filters[1].mArg4.prefix, 14U);

    // SetDaddrFilter(bw, -1, kernelFilters, &filter);
    // gAddr4Filters = GetAddr4Filters();
    // APSARA_TEST_EQUAL(gAddr4Filters.size(), 4UL);
    // APSARA_TEST_EQUAL(gAddr4Filters[2].mOpType, OP_TYPE_IN);
    // APSARA_TEST_EQUAL(gAddr4Filters[2].mIdx, 1);
    // APSARA_TEST_EQUAL(gAddr4Filters[2].mArg4.addr, 16820416U);
    // APSARA_TEST_EQUAL(gAddr4Filters[2].mArg4.prefix, 32U);

    // APSARA_TEST_EQUAL(gAddr4Filters[3].mOpType, OP_TYPE_IN);
    // APSARA_TEST_EQUAL(gAddr4Filters[3].mIdx, 1);
    // APSARA_TEST_EQUAL(gAddr4Filters[3].mArg4.addr, 33597632U);
    // APSARA_TEST_EQUAL(gAddr4Filters[3].mArg4.prefix, 14U);

    // SetSaddrBlackFilter(bw, -1, kernelFilters, &filter);
    // gAddr4Filters = GetAddr4Filters();
    // APSARA_TEST_EQUAL(gAddr4Filters.size(), 6UL);
    // APSARA_TEST_EQUAL(gAddr4Filters[4].mOpType, OP_TYPE_NOT_IN);
    // APSARA_TEST_EQUAL(gAddr4Filters[4].mIdx, 2);
    // APSARA_TEST_EQUAL(gAddr4Filters[4].mArg4.addr, 16820416U);
    // APSARA_TEST_EQUAL(gAddr4Filters[4].mArg4.prefix, 32U);

    // APSARA_TEST_EQUAL(gAddr4Filters[5].mOpType, OP_TYPE_NOT_IN);
    // APSARA_TEST_EQUAL(gAddr4Filters[5].mIdx, 2);
    // APSARA_TEST_EQUAL(gAddr4Filters[5].mArg4.addr, 33597632U);
    // APSARA_TEST_EQUAL(gAddr4Filters[5].mArg4.prefix, 14U);

    // SetDaddrBlackFilter(bw, -1, kernelFilters, &filter);
    // gAddr4Filters = GetAddr4Filters();
    // APSARA_TEST_EQUAL(gAddr4Filters.size(), 8UL);
    // APSARA_TEST_EQUAL(gAddr4Filters[6].mOpType, OP_TYPE_NOT_IN);
    // APSARA_TEST_EQUAL(gAddr4Filters[6].mIdx, 3);
    // APSARA_TEST_EQUAL(gAddr4Filters[6].mArg4.addr, 16820416U);
    // APSARA_TEST_EQUAL(gAddr4Filters[6].mArg4.prefix, 32U);

    // APSARA_TEST_EQUAL(gAddr4Filters[7].mOpType, OP_TYPE_NOT_IN);
    // APSARA_TEST_EQUAL(gAddr4Filters[7].mIdx, 3);
    // APSARA_TEST_EQUAL(gAddr4Filters[7].mArg4.addr, 33597632U);
    // APSARA_TEST_EQUAL(gAddr4Filters[7].mArg4.prefix, 14U);

    // SetSportFilter(bw, -1, kernelFilters, &filter);
    // auto gPortFilters = GetPortFilters();
    // APSARA_TEST_EQUAL(gPortFilters.size(), 3UL);

    // SetDportFilter(bw, -1, kernelFilters, &filter);
    // gPortFilters = GetPortFilters();
    // APSARA_TEST_EQUAL(gPortFilters.size(), 6UL);

    // SetSportBlackFilter(bw, -1, kernelFilters, &filter);
    // gPortFilters = GetPortFilters();
    // APSARA_TEST_EQUAL(gPortFilters.size(), 9UL);

    // SetDportBlackFilter(bw, -1, kernelFilters, &filter);
    // gPortFilters = GetPortFilters();
    // APSARA_TEST_EQUAL(gPortFilters.size(), 12UL);
}

void eBPFDriverUnittest::TestFileFilter() {
}

void eBPFDriverUnittest::TestPerfbufferManagement() {
}

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
UNIT_TEST_CASE(eBPFDriverUnittest, TestNetworkFilter);
UNIT_TEST_CASE(eBPFDriverUnittest, TestFileFilter);
UNIT_TEST_CASE(eBPFDriverUnittest, TestPerfbufferManagement);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
