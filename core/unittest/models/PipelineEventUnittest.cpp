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

#include <cstdlib>
#include "unittest/Unittest.h"
#include "models/PipelineEvent.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/SpanEvent.h"

namespace logtail {

class PipelineEventUnittest : public ::testing::Test {
public:
    void SetUp() override { mSourceBuffer.reset(new SourceBuffer); }

    void TestGetType();

protected:
    std::shared_ptr<SourceBuffer> mSourceBuffer;
};

APSARA_UNIT_TEST_CASE(PipelineEventUnittest, TestGetType, 0);


void PipelineEventUnittest::TestGetType() {
    std::unique_ptr<LogEvent> logEvent = LogEvent::CreateEvent(mSourceBuffer);
    std::unique_ptr<MetricEvent> metricEvent = MetricEvent::CreateEvent(mSourceBuffer);
    std::unique_ptr<SpanEvent> spanEvent = SpanEvent::CreateEvent(mSourceBuffer);
    APSARA_TEST_STREQ_FATAL("Log", PipelineEventTypeToString(logEvent->GetType()).c_str());
    APSARA_TEST_STREQ_FATAL("Metric", PipelineEventTypeToString(metricEvent->GetType()).c_str());
    APSARA_TEST_STREQ_FATAL("Span", PipelineEventTypeToString(spanEvent->GetType()).c_str());
}

} // namespace logtail

UNIT_TEST_MAIN