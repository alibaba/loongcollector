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

#include <algorithm>
#include <iostream>
#include <random>

#include "ebpf/driver/BPFMapTraits.h"
#include "ebpf/driver/IdAllocator.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {
namespace ebpf {
class IdAllocatorUnittest : public testing::Test {
public:
    IdAllocatorUnittest() {}

    void TestGetAndRelease();

    void TestMaxId();

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
};

void IdAllocatorUnittest::TestMaxId() {
    // add until max
    int maxId = IdAllocator::GetInstance()->GetMaxId<StringPrefixMap>();
    APSARA_TEST_EQUAL(maxId, BPFMapTraits<StringPrefixMap>::outter_max_entries);
    for (int i = 0; i < maxId + 20; i++) {
        int nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
        if (i >= maxId) {
            APSARA_TEST_EQUAL(nextIdx, -1);
        } else {
            APSARA_TEST_EQUAL(nextIdx, i);
        }
    }

    IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(2);
    int nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
    APSARA_TEST_EQUAL(nextIdx, 2);
    IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(3);
    nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
    APSARA_TEST_EQUAL(nextIdx, 3);

    nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
    APSARA_TEST_EQUAL(nextIdx, -1);
}

void IdAllocatorUnittest::TestGetAndRelease() {
    int nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
    APSARA_TEST_EQUAL(nextIdx, 0);
    nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
    APSARA_TEST_EQUAL(nextIdx, 1);
    nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
    APSARA_TEST_EQUAL(nextIdx, 2);

    IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(0);
    nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
    APSARA_TEST_EQUAL(nextIdx, 0);

    IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(1);
    nextIdx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
    APSARA_TEST_EQUAL(nextIdx, 1);

    IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(0);
    IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(1);
    IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(2);
}

UNIT_TEST_CASE(IdAllocatorUnittest, TestGetAndRelease);
UNIT_TEST_CASE(IdAllocatorUnittest, TestMaxId);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
