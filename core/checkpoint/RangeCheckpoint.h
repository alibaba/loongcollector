/*
 * Copyright 2022 iLogtail Authors
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
#include <vector>
#include "log_pb/checkpoint.pb.h"
#include "common/LogstoreFeedbackKey.h"


namespace logtail {

class RangeCheckpoint {
public:
    size_t index;
    std::string key;
    LogstoreFeedBackKey fbKey;
    RangeCheckpointPB data;
    std::vector<std::pair<uint64_t, size_t>> positions;

    inline void Prepare() {
        positions.clear();
        data.set_committed(false);
        save();
    }

    inline void Commit() {
        data.set_committed(true);
        save();
    }

    inline void IncreaseSequenceID() { data.set_sequence_id(data.sequence_id() + 1); }

    inline bool IsComplete() const { return data.has_hash_key(); }

private:
    void save();
};

typedef std::shared_ptr<RangeCheckpoint> RangeCheckpointPtr;

} // namespace logtail
