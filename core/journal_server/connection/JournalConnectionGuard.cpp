/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "JournalConnectionGuard.h"
#include "JournalConnectionManager.h"
#include "../reader/JournalReader.h"

namespace logtail {

JournalConnectionGuard::JournalConnectionGuard(std::shared_ptr<JournalConnectionInfo> connection)
    : mConnection(std::move(connection)) {
    if (mConnection) {
        mConnection->IncrementUsageCount();
    }
}

JournalConnectionGuard::~JournalConnectionGuard() {
    if (mConnection) {
        mConnection->DecrementUsageCount();
    }
}

std::shared_ptr<SystemdJournalReader> JournalConnectionGuard::GetReader() {
    return mConnection ? mConnection->GetReader() : nullptr;
}

bool JournalConnectionGuard::IsValid() const {
    return mConnection && mConnection->IsValid();
}

} // namespace logtail 