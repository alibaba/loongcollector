/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <string>

#include "common/http/HttpResponse.h"
#include "models/PipelineEventGroup.h"
#include "prometheus/AsyncEvent.h"
#include "prometheus/ScrapeConfig.h"
#include "prometheus/ScrapeTarget.h"
#include "queue/FeedbackQueueKey.h"

#ifdef APSARA_UNIT_TEST_MAIN
#include "queue/ProcessQueueItem.h"
#endif

namespace logtail {

class ScrapeWorkEvent : public PromEvent {
    friend class ScrapeJobEvent;

public:
    ScrapeWorkEvent(std::shared_ptr<ScrapeConfig> scrapeConfigPtr,
                    const ScrapeTarget& scrapeTarget,
                    QueueKey queueKey,
                    size_t inputIndex);
    ScrapeWorkEvent(const ScrapeWorkEvent&) = default;
    ~ScrapeWorkEvent() override = default;

    bool operator<(const ScrapeWorkEvent& other) const;

    void Process(const HttpResponse&) override;

    std::string GetId() const override;
    bool ReciveMessage() override;

private:
    void PushEventGroup(PipelineEventGroup&&);

    PipelineEventGroup SplitByLines(const std::string& content, time_t timestampNs);

    std::shared_ptr<ScrapeConfig> mScrapeConfigPtr;

    ScrapeTarget mScrapeTarget;
    std::string mHash;

    QueueKey mQueueKey;
    size_t mInputIndex;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorLogToMetricNativeUnittest;
    friend class ScrapeWorkEventUnittest;
    std::vector<std::shared_ptr<ProcessQueueItem>> mItem;
#endif
};

} // namespace logtail
