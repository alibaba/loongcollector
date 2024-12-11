/*
 * Copyright 2024 iLogtail Authors
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

#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <vector>

#include "common/SafeQueue.h"
#include "pipeline/queue/SenderQueueItem.h"
#include "plugin/flusher/sls/SLSClientManager.h"
#include "plugin/flusher/sls/SendResult.h"
#include "protobuf/sls/logtail_buffer_meta.pb.h"

namespace logtail {

class DiskBufferWriter {
public:
    DiskBufferWriter(const DiskBufferWriter&) = delete;
    DiskBufferWriter& operator=(const DiskBufferWriter&) = delete;

    static DiskBufferWriter* GetInstance() {
        static DiskBufferWriter instance;
        return &instance;
    }

    void Init();
    void Stop();

    bool PushToDiskBuffer(SenderQueueItem* item, uint32_t retryTimes);

private:
    static const int32_t BUFFER_META_BASE_SIZE;

    struct EncryptionStateMeta {
        int32_t mLogDataSize;
        int32_t mEncryptionSize;
        int32_t mEncodedInfoSize;
        int32_t mTimeStamp;
        int32_t mHandled;
        int32_t mRetryTime;
    };

    DiskBufferWriter() = default;
    ~DiskBufferWriter() = default;

    void BufferWriterThread();
    void BufferSenderThread();

    SLSResponse
    SendBufferFileData(const sls_logs::LogtailBufferMeta& bufferMeta, const std::string& logData, std::string& host);
    bool SendToBufferFile(SenderQueueItem* dataPtr);
    bool LoadFileToSend(time_t timeLine, std::vector<std::string>& filesToSend);
    bool CreateNewFile();
    bool WriteBackMeta(const int32_t pos, const void* buf, int32_t length, const std::string& filename);
    bool ReadNextEncryption(int32_t& pos,
                            const std::string& filename,
                            std::string& encryption,
                            EncryptionStateMeta& meta,
                            bool& readResult,
                            sls_logs::LogtailBufferMeta& bufferMeta);
    void SendEncryptionBuffer(const std::string& filename, int32_t keyVersion);
    void SetBufferFilePath(const std::string& bufferfilepath);
    std::string GetBufferFilePath();
    std::string GetBufferFileName();
    void SetBufferFileName(const std::string& filename);
    std::string GetBufferFileHeader();

    SafeQueue<SenderQueueItem*> mQueue;

    std::future<void> mBufferWriterThreadRes;
    std::atomic_bool mIsFlush = false;

    std::future<void> mBufferSenderThreadRes;
    mutable std::mutex mBufferSenderThreadRunningMux;
    bool mIsSendBufferThreadRunning = true;
    mutable std::condition_variable mStopCV;

    struct PointerHash {
        std::size_t operator()(const std::shared_ptr<CandidateHostsInfo>& ptr) const {
            return std::hash<CandidateHostsInfo*>()(ptr.get());
        }
    };

    struct PointerEqual {
        bool operator()(const std::shared_ptr<CandidateHostsInfo>& lhs,
                        const std::shared_ptr<CandidateHostsInfo>& rhs) const {
            return lhs.get() == rhs.get();
        }
    };

    std::unordered_set<std::shared_ptr<CandidateHostsInfo>, PointerHash, PointerEqual> mCandidateHostsInfos;

    mutable std::mutex mBufferFileLock;
    std::string mBufferFilePath;
    std::string mBufferFileName;

    volatile time_t mBufferDivideTime = 0;
    int64_t mCheckPeriod = 0;

    int64_t mSendLastTime = 0;
    int32_t mSendLastByte = 0;
};

} // namespace logtail
