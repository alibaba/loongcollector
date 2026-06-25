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

#include <set>
#include <string>

#include "models/PipelineEvent.h"
#include "plugin/input/InputEventTypeRegistry.h"
#include "unittest/Unittest.h"

namespace logtail {

class InputEventTypeRegistryUnittest : public testing::Test {
public:
    void TestAllNativeInputsCovered();
    void TestInputEventSemantics();
};

void InputEventTypeRegistryUnittest::TestAllNativeInputsCovered() {
    static const std::set<std::string> sExpectedInputs = {
        "input_file",
        "input_static_file_onetime",
        "input_container_stdio",
        "input_prometheus",
        "input_host_monitor",
        "input_host_meta",
        "input_internal_metrics",
        "input_internal_alarms",
        "input_internal_config_container_info",
        "input_file_security",
        "input_network_security",
        "input_process_security",
        "input_agentsight",
        "input_cpu_profiling",
        "input_network_observer",
        "input_forward",
    };

    std::set<std::string> actualInputs;
    for (const auto& entry : GetNativeInputEventTypeEntries()) {
        APSARA_TEST_FALSE(entry.mInputType.empty());
        APSARA_TEST_TRUE(actualInputs.insert(std::string(entry.mInputType)).second);
        APSARA_TEST_NOT_EQUAL(InputEventMask::NONE, entry.mMask);
    }

    APSARA_TEST_EQUAL(sExpectedInputs.size(), actualInputs.size());
    for (const auto& inputType : sExpectedInputs) {
        APSARA_TEST_TRUE(actualInputs.find(inputType) != actualInputs.end());
    }
}

void InputEventTypeRegistryUnittest::TestInputEventSemantics() {
    APSARA_TEST_TRUE(InputProducesEventType("input_prometheus", PipelineEvent::Type::METRIC));
    APSARA_TEST_FALSE(InputProducesEventType("input_prometheus", PipelineEvent::Type::LOG));
    APSARA_TEST_FALSE(InputProducesEventType("input_prometheus", PipelineEvent::Type::SPAN));

    APSARA_TEST_TRUE(InputProducesEventType("input_network_observer", PipelineEvent::Type::LOG));
    APSARA_TEST_TRUE(InputProducesEventType("input_network_observer", PipelineEvent::Type::METRIC));
    APSARA_TEST_TRUE(InputProducesEventType("input_network_observer", PipelineEvent::Type::SPAN));

    APSARA_TEST_TRUE(InputProducesEventType("input_forward", PipelineEvent::Type::LOG));
    APSARA_TEST_TRUE(InputProducesEventType("input_forward", PipelineEvent::Type::METRIC));
    APSARA_TEST_TRUE(InputProducesEventType("input_forward", PipelineEvent::Type::SPAN));

    APSARA_TEST_FALSE(InputProducesEventType("input_host_monitor", PipelineEvent::Type::SPAN));
    APSARA_TEST_FALSE(InputProducesEventType("not_exists_input", PipelineEvent::Type::LOG));
}

UNIT_TEST_CASE(InputEventTypeRegistryUnittest, TestAllNativeInputsCovered)
UNIT_TEST_CASE(InputEventTypeRegistryUnittest, TestInputEventSemantics)

} // namespace logtail

UNIT_TEST_MAIN
