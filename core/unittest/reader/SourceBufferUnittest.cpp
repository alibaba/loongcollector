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
#include "util.h"

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
    APSARA_TEST_FALSE(allocator.IsInited());
    const size_t alloc_size1 = 1000;
    const size_t alloc_size2 = 20;
    // should not allocate anything before init
    APSARA_TEST_EQUAL(nullptr, allocator.Allocate(alloc_size1));
    const size_t size = 1024;
    APSARA_TEST_TRUE(allocator.Init(size));
    // should allocate successfully after init
    char* alloc1 = static_cast<char*>(allocator.Allocate(alloc_size1));
    APSARA_TEST_NOT_EQUAL(nullptr, alloc1);
    alloc1[0] = 'a';
    // should use inited buffer is capacity is enough
    char* alloc2 = static_cast<char*>(allocator.Allocate(alloc_size2));
    APSARA_TEST_EQUAL(alloc1 + alloc_size1, alloc2);
    // should new block is capacity is not enough
    char* alloc3 = static_cast<char*>(allocator.Allocate(alloc_size1));
    alloc3[0] = 'b';
    APSARA_TEST_NOT_EQUAL(nullptr, alloc3);
    // ensure ptr is still valid
    APSARA_TEST_EQUAL('a', static_cast<char*>(alloc1)[0]);
}

UNIT_TEST_CASE(SourceBufferUnittest, TestBufferAllocatorAllocate);

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}