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

#include "common/Flags.h"
#include "file_server/checkpoint/FileReaderCommitTracker.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(max_file_reader_send_rollback_retries);

using namespace std;

namespace logtail {

class FileReaderCommitTrackerUnittest : public testing::Test {
public:
    void TestInitialCommittedOffset() const;
    void TestContiguousAckInOrder() const;
    void TestOutOfOrderAck() const;
    void TestStaleAckIgnored() const;
    void TestRollbackOnSendFailure() const;
    void TestRollbackKeepsSeqMonotonic() const;
    void TestPoisonPillSkipAfterRetries() const;
    void TestProgressResetsRetryCounter() const;
};

void FileReaderCommitTrackerUnittest::TestInitialCommittedOffset() const {
    FileReaderCommitTracker tracker;
    tracker.Init(100);
    // before any confirmation the committed offset stays at the recovered position
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
    tracker.RegisterInflight(200);
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
}

void FileReaderCommitTrackerUnittest::TestContiguousAckInOrder() const {
    FileReaderCommitTracker tracker;
    tracker.Init(0);
    uint64_t s0 = tracker.RegisterInflight(100);
    uint64_t s1 = tracker.RegisterInflight(250);
    APSARA_TEST_EQUAL(0, tracker.GetCommittedOffset());
    tracker.ConfirmSent(s0);
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
    tracker.ConfirmSent(s1);
    APSARA_TEST_EQUAL(250, tracker.GetCommittedOffset());
}

void FileReaderCommitTrackerUnittest::TestOutOfOrderAck() const {
    FileReaderCommitTracker tracker;
    tracker.Init(0);
    uint64_t s0 = tracker.RegisterInflight(100);
    uint64_t s1 = tracker.RegisterInflight(250);
    uint64_t s2 = tracker.RegisterInflight(400);
    // s1 confirmed first must NOT advance past the unconfirmed s0
    tracker.ConfirmSent(s1);
    APSARA_TEST_EQUAL(0, tracker.GetCommittedOffset());
    tracker.ConfirmSent(s2);
    APSARA_TEST_EQUAL(0, tracker.GetCommittedOffset());
    // confirming s0 unblocks the whole contiguous run
    tracker.ConfirmSent(s0);
    APSARA_TEST_EQUAL(400, tracker.GetCommittedOffset());
}

void FileReaderCommitTrackerUnittest::TestStaleAckIgnored() const {
    FileReaderCommitTracker tracker;
    tracker.Init(0);
    uint64_t s0 = tracker.RegisterInflight(100);
    tracker.ConfirmSent(s0);
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
    // a duplicated (already-committed) confirmation is a no-op
    tracker.ConfirmSent(s0);
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
}

void FileReaderCommitTrackerUnittest::TestRollbackOnSendFailure() const {
    FileReaderCommitTracker tracker;
    tracker.Init(0);
    uint64_t s0 = tracker.RegisterInflight(100);
    uint64_t s1 = tracker.RegisterInflight(250);
    tracker.ConfirmSent(s0);
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
    // s1 finally failed: a rollback is requested
    tracker.ReportSendFailure(s1);
    int64_t rewindOffset = -1;
    APSARA_TEST_TRUE(tracker.ConsumeRollback(&rewindOffset));
    // rewinds to the last committed offset, not the read cursor
    APSARA_TEST_EQUAL(100, rewindOffset);
    // committed offset is unchanged by the rollback
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
    // a second consume without a new failure returns false
    APSARA_TEST_FALSE(tracker.ConsumeRollback(&rewindOffset));
}

void FileReaderCommitTrackerUnittest::TestRollbackKeepsSeqMonotonic() const {
    FileReaderCommitTracker tracker;
    tracker.Init(0);
    uint64_t s0 = tracker.RegisterInflight(100);
    uint64_t s1 = tracker.RegisterInflight(250);
    tracker.ConfirmSent(s0);
    tracker.ReportSendFailure(s1);
    int64_t rewindOffset = -1;
    APSARA_TEST_TRUE(tracker.ConsumeRollback(&rewindOffset));
    APSARA_TEST_EQUAL(100, rewindOffset);
    // a stale confirmation of the abandoned s1 must not advance the committed offset
    tracker.ConfirmSent(s1);
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
    // re-read after rollback gets a strictly greater seq and commits normally
    uint64_t s2 = tracker.RegisterInflight(250);
    APSARA_TEST_TRUE(s2 > s1);
    tracker.ConfirmSent(s2);
    APSARA_TEST_EQUAL(250, tracker.GetCommittedOffset());
    // a final failure on an already-committed unit is a stale no-op
    tracker.ReportSendFailure(s2);
    APSARA_TEST_FALSE(tracker.ConsumeRollback(&rewindOffset));
}

void FileReaderCommitTrackerUnittest::TestPoisonPillSkipAfterRetries() const {
    INT32_FLAG(max_file_reader_send_rollback_retries) = 3;
    FileReaderCommitTracker tracker;
    tracker.Init(0);
    // simulate a poison-pill: the same read unit [0,100) always fails to send.
    int64_t rewindOffset = -1;
    int64_t droppedBytes = -1;
    for (int i = 1; i <= 3; ++i) {
        uint64_t seq = tracker.RegisterInflight(100);
        tracker.ReportSendFailure(seq);
        APSARA_TEST_TRUE(tracker.ConsumeRollback(&rewindOffset, &droppedBytes));
        // within the retry budget: rewind to committed offset, no data dropped
        APSARA_TEST_EQUAL(0, rewindOffset);
        APSARA_TEST_EQUAL(0, droppedBytes);
        APSARA_TEST_EQUAL(0, tracker.GetCommittedOffset());
    }
    // the 4th consecutive no-progress rollback exceeds the budget: skip the stuck unit, dropping it
    uint64_t seq = tracker.RegisterInflight(100);
    tracker.ReportSendFailure(seq);
    APSARA_TEST_TRUE(tracker.ConsumeRollback(&rewindOffset, &droppedBytes));
    APSARA_TEST_EQUAL(100, rewindOffset);
    APSARA_TEST_EQUAL(100, droppedBytes);
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
}

void FileReaderCommitTrackerUnittest::TestProgressResetsRetryCounter() const {
    INT32_FLAG(max_file_reader_send_rollback_retries) = 2;
    FileReaderCommitTracker tracker;
    tracker.Init(0);
    int64_t rewindOffset = -1;
    int64_t droppedBytes = -1;
    // two no-progress rollbacks at offset 0 (within budget)
    for (int i = 0; i < 2; ++i) {
        uint64_t seq = tracker.RegisterInflight(100);
        tracker.ReportSendFailure(seq);
        APSARA_TEST_TRUE(tracker.ConsumeRollback(&rewindOffset, &droppedBytes));
        APSARA_TEST_EQUAL(0, droppedBytes);
    }
    // now a unit commits successfully, advancing the committed offset (real progress)
    uint64_t ok = tracker.RegisterInflight(100);
    tracker.ConfirmSent(ok);
    APSARA_TEST_EQUAL(100, tracker.GetCommittedOffset());
    // the next stuck unit should get a fresh budget (counter was reset by progress), so the first
    // rollback after progress does NOT drop data even though total rollbacks now exceed the cap
    uint64_t seq = tracker.RegisterInflight(200);
    tracker.ReportSendFailure(seq);
    APSARA_TEST_TRUE(tracker.ConsumeRollback(&rewindOffset, &droppedBytes));
    APSARA_TEST_EQUAL(100, rewindOffset);
    APSARA_TEST_EQUAL(0, droppedBytes);
}

UNIT_TEST_CASE(FileReaderCommitTrackerUnittest, TestInitialCommittedOffset);
UNIT_TEST_CASE(FileReaderCommitTrackerUnittest, TestContiguousAckInOrder);
UNIT_TEST_CASE(FileReaderCommitTrackerUnittest, TestOutOfOrderAck);
UNIT_TEST_CASE(FileReaderCommitTrackerUnittest, TestStaleAckIgnored);
UNIT_TEST_CASE(FileReaderCommitTrackerUnittest, TestRollbackOnSendFailure);
UNIT_TEST_CASE(FileReaderCommitTrackerUnittest, TestRollbackKeepsSeqMonotonic);
UNIT_TEST_CASE(FileReaderCommitTrackerUnittest, TestPoisonPillSkipAfterRetries);
UNIT_TEST_CASE(FileReaderCommitTrackerUnittest, TestProgressResetsRetryCounter);

} // namespace logtail

UNIT_TEST_MAIN
