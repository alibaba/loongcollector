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
// Authors: Wardenjohn <zhangwarden@gmail.com>

#include <boost/process/search_path.hpp>
#include <boost/process/system.hpp>
#include <boost/process.hpp>

#include "MetricEvent.h"
#include "common/FileSystemUtil.h"
#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/GPUCollector.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class GPUCollectorUnittest : public testing::Test {
public:
    void TestCollect() const;
    bool CheckGPUExist() const;

protected:
    void SetUp() override {
        
    }
};

bool GPUCollectorUnittest::CheckGPUExist() const {
    if(!std::filesystem::exists(NVIDIACTL)){
        return false;
    }

    auto binary_path = boost::process::search_path(NVSMI);
    if(binary_path.empty()){
        return false;
    }

    boost::process::ipstream pipe_stream;
    std::string cmd = "nvidia-smi -L";
    std::error_code ec;

    int exit_code = boost::process::system(cmd, boost::process::std_out > pipe_stream, ec);
    if (ec || exit_code != 0){
        return false;
    }

    std::string line;
    bool has_output = false;
    while (std::getline(pipe_stream, line)) {
        if (!line.empty()) {
            has_output = true;
        }
    }

    if (!has_output) {
        return false;
    }

    return true;
}

void GPUCollectorUnittest::TestCollect() const {
    auto collector = GPUCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    HostMonitorTimerEvent::CollectConfig collectConfig(GPUCollector::sName, 0, 0, std::chrono::seconds(1));

    APSARA_TEST_EQUAL_FATAL(CheckGPUExist(), collector.Collect(collectConfig, &group));
    APSARA_TEST_EQUAL_FATAL(CheckGPUExist(), collector.Collect(collectConfig, &group));
    APSARA_TEST_EQUAL_FATAL(CheckGPUExist(), collector.Collect(collectConfig, &group));
}


UNIT_TEST_CASE(GPUCollectorUnittest, TestCollect);

} // namespace logtail

UNIT_TEST_MAIN