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

#include <string>
#include <vector>

#include "collection_pipeline/serializer/Serializer.h"

namespace logtail {

class JsonEventGroupSerializer : public Serializer<BatchedEvents> {
public:
    JsonEventGroupSerializer(Flusher* f) : Serializer<BatchedEvents>(f) {}

private:
    bool Serialize(BatchedEvents&& p, std::string& res, std::string& errorMsg) override;
};

} // namespace logtail
