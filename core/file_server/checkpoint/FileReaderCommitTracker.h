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

#include <cstdint>

#include <map>
#include <mutex>
#include <utility>

namespace logtail {

// FileReaderCommitTracker implements deferred (send-confirmed) offset commit for the tailing file
// input (input_file). Unlike input_static_file, which owns a single central checkpoint manager keyed
// by (config, input idx), each tailing LogFileReader owns its own tracker. This keeps the lock local
// to a single file, avoiding global lock contention on the high-throughput tailing path.
//
// The reader thread advances its read cursor (LogFileReader::mLastFilePos) at read time and registers
// each pushed read unit here via RegisterInflight, which returns a monotonically increasing sequence
// number. A flusher confirms the unit (ConfirmSent) only after the data has been durably delivered,
// at which point the committed offset is advanced strictly in read order (contiguous-ack), tolerating
// the out-of-order delivery acknowledgements of asynchronous flushers such as Kafka. The reader
// persists GetCommittedOffset() (not the read cursor) into the checkpoint, so a crash/restart only
// re-reads data whose delivery was never confirmed (at-least-once).
//
// The tracker is held by a shared_ptr owned by the reader; send tokens reference it via weak_ptr so a
// late acknowledgement arriving after the reader (and tracker) is destroyed is safely ignored.
class FileReaderCommitTracker {
public:
    FileReaderCommitTracker() = default;

    // Initialize the committed offset, e.g. from a loaded checkpoint. Called before reading starts.
    void Init(int64_t committedOffset);

    // Called by the reader thread at read time. Returns a monotonically increasing sequence number
    // for the read unit ending at endOffset.
    uint64_t RegisterInflight(int64_t endOffset);

    // Called by a flusher (any thread) on send success. Advances the committed offset through
    // consecutively confirmed read units, so the persisted offset never jumps over data whose
    // delivery has not been acknowledged.
    void ConfirmSent(uint64_t seq);

    // Called by a flusher on terminal send failure. Requests a runtime rollback so the reader
    // rewinds to the last committed offset and re-reads the read-but-unconfirmed range.
    void ReportSendFailure(uint64_t seq);

    // Called by the reader thread. If a rollback was requested, drops all in-flight units (so their
    // offset ranges are re-read), writes the rewind target to rewindOffset, and returns true.
    //
    // Normally the rewind target is the last committed offset, so the read-but-unconfirmed range is
    // re-read (at-least-once). To bound the cost of a poison-pill (data that always fails to send),
    // consecutive rollbacks that do not make commit progress are counted; once the count exceeds
    // max_file_reader_send_rollback_retries, the first unconfirmed read unit is skipped (its data is
    // dropped) and the rewind target is advanced past it so reading can make forward progress. When
    // this happens, droppedBytes (if not null) is set to the number of skipped bytes so the caller
    // can emit a warning/alarm; otherwise droppedBytes is set to 0.
    bool ConsumeRollback(int64_t* rewindOffset, int64_t* droppedBytes = nullptr);

    // Called by the reader thread when dumping checkpoint: returns the offset whose data has been
    // confirmed delivered.
    int64_t GetCommittedOffset() const;

private:
    mutable std::mutex mMutex;
    uint64_t mNextSeq = 0;
    uint64_t mCommitSeq = 0;
    int64_t mCommittedOffset = 0;
    // seq -> (endOffset, confirmed)
    std::map<uint64_t, std::pair<int64_t, bool>> mPending;
    bool mRollbackRequested = false;
    // Poison-pill guard: count consecutive rollbacks that did not advance the committed offset. When
    // the committed offset moves forward (progress was made) the counter resets.
    int32_t mConsecutiveRollbacks = 0;
    int64_t mLastRollbackCommittedOffset = -1;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FileReaderCommitTrackerUnittest;
#endif
};

} // namespace logtail
