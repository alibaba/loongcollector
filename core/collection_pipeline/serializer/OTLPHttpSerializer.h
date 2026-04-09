/*
 * Copyright 2025 iLogtail Authors
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

#include "collection_pipeline/serializer/Serializer.h"

namespace logtail {

class OTLPEventGroupSerializer : public Serializer<BatchedEvents> {
public:
    OTLPEventGroupSerializer(Flusher* f) : Serializer<BatchedEvents>(f) {}

    // Serialize to protobuf binary string (for http+protobuf transport).
    bool SerializeToBinaryString(BatchedEvents&& p, std::string& res, std::string& errorMsg);

private:
    bool Serialize(BatchedEvents&& p, std::string& res, std::string& errorMsg) override;
};

} // namespace logtail
