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

#include <cstddef>
#include <cstdint>

#include <memory>
#include <string>

namespace logtail {

class FileReaderCommitTracker;

// FileSendCheckpoint threads the identity and end offset of a read unit from the
// file input through the pipeline to the flusher. The flusher confirms it back to
// the checkpoint committer only after the data has been successfully sent, so the
// persisted file offset is advanced only for data that has been delivered.
//
// It is produced by two file inputs that route to different committers:
//   - input_static_file: keyed by (mConfigName, mInputIdx), committed by the central
//     InputStaticFileCheckpointManager.
//   - input_file (tailing): committed by the per-reader FileReaderCommitTracker referenced
//     by mTracker (a weak_ptr so a late ack arriving after the reader is destroyed is ignored).
// mSeq is a monotonically increasing sequence number assigned at read time, used by the
// committer to advance the committed offset strictly in read order (contiguous-ack), which
// tolerates the out-of-order delivery acknowledgements of asynchronous flushers such as Kafka.
struct FileSendCheckpoint {
    std::string mConfigName;
    size_t mInputIdx = 0;
    uint64_t mSeq = 0;
    uint64_t mEndOffset = 0;
    // Set only for the tailing file input; when present, confirmations route here instead of the
    // static-file checkpoint manager.
    std::weak_ptr<FileReaderCommitTracker> mTracker;

    FileSendCheckpoint() = default;
    FileSendCheckpoint(const std::string& configName, size_t inputIdx, uint64_t seq, uint64_t endOffset)
        : mConfigName(configName), mInputIdx(inputIdx), mSeq(seq), mEndOffset(endOffset) {}
};

using FileSendCheckpointPtr = std::shared_ptr<FileSendCheckpoint>;

// Dispatch a successful-send confirmation to the right committer: the per-reader commit tracker
// (tailing input_file) if mTracker is alive, otherwise the static-file checkpoint manager.
void ConfirmFileSendCheckpoint(const FileSendCheckpointPtr& cpt);
// Dispatch a terminal-failure report (requests a runtime rewind) to the right committer.
void ReportFileSendCheckpointFailure(const FileSendCheckpointPtr& cpt);

} // namespace logtail
