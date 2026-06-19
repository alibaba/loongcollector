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

#include "file_server/checkpoint/FileReaderCommitTracker.h"

#include "common/Flags.h"

// Maximum number of consecutive runtime rollbacks that make no commit progress before the stuck
// (poison-pill) read unit is skipped to let reading move forward. A non-positive value disables the
// skip, restoring the unbounded re-read behavior.
DEFINE_FLAG_INT32(max_file_reader_send_rollback_retries,
                  "max consecutive send-failure rollbacks without progress before dropping the stuck read unit",
                  3);

using namespace std;

namespace logtail {

void FileReaderCommitTracker::Init(int64_t committedOffset) {
    lock_guard<mutex> lock(mMutex);
    mCommittedOffset = committedOffset;
}

uint64_t FileReaderCommitTracker::RegisterInflight(int64_t endOffset) {
    lock_guard<mutex> lock(mMutex);
    uint64_t seq = mNextSeq++;
    mPending.emplace(seq, make_pair(endOffset, false));
    return seq;
}

void FileReaderCommitTracker::ConfirmSent(uint64_t seq) {
    lock_guard<mutex> lock(mMutex);
    auto it = mPending.find(seq);
    if (it == mPending.end()) {
        // already committed, dropped by a rollback, or unknown seq; ignore
        return;
    }
    it->second.second = true; // mark confirmed
    // advance the committed offset through consecutively confirmed read units, so the persisted
    // offset never jumps over data whose delivery has not been acknowledged
    while (true) {
        auto cit = mPending.find(mCommitSeq);
        if (cit == mPending.end() || !cit->second.second) {
            break;
        }
        mCommittedOffset = cit->second.first;
        mPending.erase(cit);
        ++mCommitSeq;
    }
}

void FileReaderCommitTracker::ReportSendFailure(uint64_t seq) {
    lock_guard<mutex> lock(mMutex);
    if (mPending.find(seq) == mPending.end()) {
        // the unit was already committed (or cleared by a previous rollback); stale report, ignore
        return;
    }
    mRollbackRequested = true;
}

bool FileReaderCommitTracker::ConsumeRollback(int64_t* rewindOffset, int64_t* droppedBytes) {
    lock_guard<mutex> lock(mMutex);
    if (droppedBytes != nullptr) {
        *droppedBytes = 0;
    }
    if (!mRollbackRequested) {
        return false;
    }

    // Track consecutive rollbacks that make no commit progress. If the committed offset advanced
    // since the previous rollback, real progress was made, so reset the counter.
    if (mCommittedOffset != mLastRollbackCommittedOffset) {
        mConsecutiveRollbacks = 0;
        mLastRollbackCommittedOffset = mCommittedOffset;
    }
    ++mConsecutiveRollbacks;

    int64_t target = mCommittedOffset;
    const int32_t maxRetries = INT32_FLAG(max_file_reader_send_rollback_retries);
    if (maxRetries > 0 && mConsecutiveRollbacks > maxRetries) {
        // Poison-pill guard: the same range keeps failing. Skip the first unconfirmed read unit by
        // advancing the rewind target past its end offset, dropping its data so reading can make
        // forward progress. The next read starts after the dropped unit.
        auto cit = mPending.find(mCommitSeq);
        if (cit != mPending.end() && cit->second.first > mCommittedOffset) {
            if (droppedBytes != nullptr) {
                *droppedBytes = cit->second.first - mCommittedOffset;
            }
            target = cit->second.first;
            mCommittedOffset = target;
        }
        // Reset so the next stuck unit gets a fresh retry budget at the new offset.
        mConsecutiveRollbacks = 0;
        mLastRollbackCommittedOffset = mCommittedOffset;
    }

    if (rewindOffset != nullptr) {
        *rewindOffset = target;
    }
    // Drop every in-flight unit so its offset range is re-read. We do NOT reset mNextSeq: keeping it
    // monotonic guarantees that read units registered after the rollback receive sequence numbers
    // strictly greater than any pre-rollback unit, so a late-arriving (stale) ConfirmSent for an
    // abandoned seq can never collide with a post-rollback unit. mCommitSeq jumps to mNextSeq because
    // the next unit to be committed is the first one registered after this rollback.
    mPending.clear();
    mCommitSeq = mNextSeq;
    mRollbackRequested = false;
    return true;
}

int64_t FileReaderCommitTracker::GetCommittedOffset() const {
    lock_guard<mutex> lock(mMutex);
    return mCommittedOffset;
}

} // namespace logtail
