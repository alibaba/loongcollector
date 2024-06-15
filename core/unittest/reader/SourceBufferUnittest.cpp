// Copyright 2022 iLogtail Authors
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

#include "unittest/Unittest.h"
#include <fstream>
#include <json/json.h>
#include "LogFileReader.h"

DECLARE_FLAG_INT32(force_release_deleted_file_fd_timeout);

namespace logtail {

class SourceBufferUnittest : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
    void TestBufferAllocatorAllocate();
};

void SourceBufferUnittest::TestBufferAllocatorAllocate() {
    BufferAllocator allocator;
    const size_t alloc_size1 = 1000;

    // mChunkSize default is 4096
    // should use inited buffer when alloc_size <= mFreeBytesInChunk
    char* alloc1 = static_cast<char*>(allocator.Allocate(alloc_size1));
    APSARA_TEST_NOT_EQUAL(nullptr, alloc1);
    alloc1[0] = 'a';

    // mFreeBytesInChunk = 4096 - 1000 = 3096
    // mFreeBytesInChunk < alloc_size < 2*mChunkSize
    const size_t alloc_size2 = 4000;
    char* alloc2 = static_cast<char*>(allocator.Allocate(alloc_size2));
    APSARA_TEST_NOT_EQUAL(nullptr, alloc2);
    APSARA_TEST_EQUAL(allocator.mAllocatedChunks.size(), 2U);
    alloc2[0] = 'b';

    // alloc_size >= 2*mChunkSize
    const size_t alloc_size3 = 10000;
    char* alloc3 = static_cast<char*>(allocator.Allocate(alloc_size3));
    APSARA_TEST_NOT_EQUAL(nullptr, alloc3);
    APSARA_TEST_EQUAL(allocator.mAllocatedChunks.size(), 3U);
    alloc3[0] = 'c';

    // ensure ptr is still valid
    APSARA_TEST_EQUAL('a', static_cast<char*>(alloc1)[0]);
    APSARA_TEST_EQUAL('b', static_cast<char*>(alloc2)[0]);
    APSARA_TEST_EQUAL('c', static_cast<char*>(alloc3)[0]);
}

UNIT_TEST_CASE(SourceBufferUnittest, TestBufferAllocatorAllocate);

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}