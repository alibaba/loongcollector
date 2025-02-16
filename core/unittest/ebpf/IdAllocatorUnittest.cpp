#include <algorithm>
#include <iostream>
#include <random>

// #include "logger/Logger.h"
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

// void IdAllocatorUnittest::TestMaxId() {
//     // add until max
//     int maxId = IdAllocator::GetInstance()->GetMaxId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(maxId, nami::BPFMapTraits<nami::StringPrefixMap>::outter_max_entries);
//     for (size_t i = 0; i < maxId + 20; i ++) {
//         int nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//         if (i >= maxId) {
//             APSARA_TEST_EQUAL(nextIdx, -1);
//         } else {
//             APSARA_TEST_EQUAL(nextIdx, i);
//         }
//     }

//     IdAllocator::GetInstance()->ReleaseId<nami::StringPrefixMap>(2);
//     int nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(nextIdx, 2);
//     IdAllocator::GetInstance()->ReleaseId<nami::StringPrefixMap>(3);
//     nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(nextIdx, 3);

//     nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(nextIdx, -1);
// }

// void IdAllocatorUnittest::TestGetAndRelease() {
//     int nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(nextIdx, 0);
//     nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(nextIdx, 1);
//     nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(nextIdx, 2);

//     IdAllocator::GetInstance()->ReleaseId<nami::StringPrefixMap>(0);
//     nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(nextIdx, 0);

//     IdAllocator::GetInstance()->ReleaseId<nami::StringPrefixMap>(1);
//     nextIdx = IdAllocator::GetInstance()->GetNextId<nami::StringPrefixMap>();
//     APSARA_TEST_EQUAL(nextIdx, 1);

//     IdAllocator::GetInstance()->ReleaseId<nami::StringPrefixMap>(0);
//     IdAllocator::GetInstance()->ReleaseId<nami::StringPrefixMap>(1);
//     IdAllocator::GetInstance()->ReleaseId<nami::StringPrefixMap>(2);
// }

// UNIT_TEST_CASE(IdAllocatorUnittest, TestGetAndRelease);
// UNIT_TEST_CASE(IdAllocatorUnittest, TestMaxId);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
