/*
 * Copyright 2022 iLogtail Authors
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

#include <atomic>
#include <deque>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "checkpoint/RangeCheckpoint.h"
#include "common/DevInode.h"
#include "common/EncodingConverter.h"
#include "common/FileInfo.h"
#include "common/LogFileOperator.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "event/Event.h"
#include "file_server/FileDiscoveryOptions.h"
#include "file_server/MultilineOptions.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"
#include "reader/FileReaderOptions.h"
#include "reader/SourceBuffer.h"

namespace logtail {

struct LogBuffer;
class LogFileReader;
class DevInode;

typedef std::shared_ptr<LogFileReader> LogFileReaderPtr;
typedef std::deque<LogFileReaderPtr> LogFileReaderPtrArray;

// Only get the currently written log file, it will choose the last modified file to read. There are several condition
// to choose the lastmodify file:
// 1. if the last read file don't exist
// 2. if the file's first 100 bytes(file signature) is not same with the last read file's signature, which meaning the
// log file has be rolled
//
// when a new file is choosen, it will set the read position
// 1. if the time in the file's first line >= the last read log time , then set the file read position to 0 (which mean
// the file is new created)
// 2. other wise , set the position to the end of the file
// *bufferptr is null terminated.
/*
 * 1. for multiline log, "xxx" mean a string without '\n'
 * 1-1. bufferSize = 512KB:
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3\n" -> "MultiLineLog_1\nMultiLineLog_2\0"
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3_Line_1\n" -> "MultiLineLog_1\nMultiLineLog_2\0"
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3_Line_1\nxxx" -> "MultiLineLog_1\nMultiLineLog_2\0"
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3\nxxx" -> "MultiLineLog_1\nMultiLineLog_2\0"
 *
 * 1-2. bufferSize < 512KB:
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3\n" -> "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3\0"
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3_Line_1\n" -> "MultiLineLog_1\nMultiLineLog_2\MultiLineLog_3_Line_1\0"
 * **this is not expected !** "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3_Line_1\nxxx" ->
 * "MultiLineLog_1\nMultiLineLog_2\0" "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3\nxxx" ->
 * "MultiLineLog_1\nMultiLineLog_2\0"
 *
 * 2. for singleline log, "xxx" mean a string without '\n'
 * "SingleLineLog_1\nSingleLineLog_2\nSingleLineLog_3\n" -> "SingleLineLog_1\nSingleLineLog_2\nSingleLineLog_3\0"
 * "SingleLineLog_1\nSingleLineLog_2\nxxx" -> "SingleLineLog_1\nSingleLineLog_2\0"
 */
class LogFileReader {
public:
    enum FileCompareResult {
        FileCompareResult_DevInodeChange,
        FileCompareResult_SigChange,
        FileCompareResult_SigSameSizeChange,
        FileCompareResult_SigSameSizeSame,
        FileCompareResult_Error
    };

    enum FileReadPolicy {
        BACKWARD_TO_BEGINNING,
        BACKWARD_TO_BOOT_TIME,
        BACKWARD_TO_FIXED_POS,
    };

    static LogFileReader* CreateLogFileReader(const std::string& hostLogPathDir,
                                              const std::string& hostLogPathFile,
                                              const DevInode& devInode,
                                              const FileReaderConfig& readerConfig,
                                              const MultilineConfig& multilineConfig,
                                              const FileDiscoveryConfig& discoveryConfig,
                                              uint32_t exactlyonceConcurrency,
                                              bool forceFromBeginning);

    LogFileReader(const std::string& hostLogPathDir,
                  const std::string& hostLogPathFile,
                  const DevInode& devInode,
                  const FileReaderConfig& readerConfig,
                  const MultilineConfig& multilineConfig);

    bool ReadLog(LogBuffer& logBuffer, const Event* event);
    time_t GetLastUpdateTime() const // actually it's the time whenever ReadLogs is called
    {
        return mLastUpdateTime;
    }
    // 转移至multilineoptions
    // // this function should only be called once
    // void SetLogMultilinePolicy(const std::string& begReg, const std::string& conReg, const std::string& endReg);

    // bool IsMultiLine() { return mLogBeginRegPtr != NULL || mLogContinueRegPtr != NULL || mLogEndRegPtr != NULL; }

    // void SetReaderFlushTimeout(int timeout) { mReaderFlushTimeout = timeout; }

    std::string GetTopicName(const std::string& topicConfig, const std::string& path);

    void SetTopicName(const std::string& topic) { mTopicName = topic; }

    FileCompareResult CompareToFile(const std::string& filePath);

    virtual int32_t
    RemoveLastIncompleteLog(char* buffer, int32_t size, int32_t& rollbackLineFeedCount, bool allowRollback = true);

    size_t AlignLastCharacter(char* buffer, size_t size);

    virtual ~LogFileReader();

    // const std::string& GetRegion() const { return mRegion; }

    // void SetRegion(const std::string& region) { mRegion = region; }

    // const std::string& GetConfigName() const { return mConfigName; }

    // void SetConfigName(const std::string& configName) { mConfigName = configName; }

    // const std::string& GetProjectName() const { return mProjectName; }

    const std::string& GetTopicName() const { return mTopicName; }

    // const std::string& GetCategory() const { return mCategory; }

    /// @return e.g. `/logtail_host/var/xxx/home/admin/access.log`,
    const std::string& GetHostLogPath() const { return mHostLogPath; }

    bool GetSymbolicLinkFlag() const { return mSymbolicLinkFlag; }

    /// @return e.g. `/home/admin/access.log`
    const std::string& GetConvertedPath() const { return mDockerPath.empty() ? mHostLogPath : mDockerPath; }

    const std::string& GetHostLogPathFile() const { return mHostLogPathFile; }

    int64_t GetFileSize() const { return mLastFileSize; }

    int64_t GetLastFilePos() const { return mLastFilePos; }

    void ResetLastFilePos() { mLastFilePos = 0; }

    bool NeedSkipFirstModify() const { return mSkipFirstModify; }

    void DisableSkipFirstModify() { mSkipFirstModify = false; }

    void SetReadFromBeginning();

    // fuse, 废弃
    // bool SetReadPosForBackwardReading(LogFileOperator& op);

    void SetLastFilePos(int64_t pos) {
        if (pos > 0)
            mFirstWatched = false;
        mLastFilePos = pos;
    }
    void
    InitReader(bool tailExisted = false, FileReadPolicy policy = BACKWARD_TO_FIXED_POS, uint32_t eoConcurrency = 0);

    void DumpMetaToMem(bool checkConfigFlag = false);

    std::string GetSourceId() { return mSourceId; }

    bool IsFileDeleted() const { return mFileDeleted; }

    void SetFileDeleted(bool flag);

    time_t GetDeletedTime() const { return mDeletedTime; }

    bool IsContainerStopped() const { return mContainerStopped; }

    void SetContainerStopped();

    time_t GetContainerStoppedTime() const { return mContainerStoppedTime; }

    bool IsFileOpened() const { return mLogFileOp.IsOpen(); }

    bool ShouldForceReleaseDeletedFileFd();

    // void SetPluginFlag(bool flag) { mPluginFlag = flag; }

    // bool GetPluginFlag() const { return mPluginFlag; }

    void OnOpenFileError();

    // if update file ptr return false, then we should delete this reader
    bool UpdateFilePtr();

    bool CloseTimeoutFilePtr(int32_t curTime);

    bool CheckDevInode();

    bool CheckFileSignatureAndOffset(bool isOpenOnUpdate);

    void UpdateLogPath(const std::string& filePath) {
        if (mHostLogPath == filePath) {
            return;
        }
        mRealLogPath = filePath;
    }

    void SetSymbolicLinkFlag(bool flag) { mSymbolicLinkFlag = flag; }

    void CloseFilePtr();

    // void SetLogstoreKey(uint64_t logstoreKey) { mLogstoreKey = logstoreKey; }

    // Return the key of queues into which next read data will push.
    //
    // For normal reader, there is only one key for all read data, just return it.
    //
    // For exactly once reader, there are N keys, N is named concurrency, each
    //  read data can only enter specified queue.
    // When one of the concurrency is blocked, concurrencies after it will also be
    //  blocked. For example, N is 8, concurrency 0-7, concurrency 1 is blocked, then
    //  nex read of concurrency 2-7 will also be blocked.
    uint64_t GetLogstoreKey() const;

    void SetDevInode(const DevInode& devInode) { mDevInode = devInode; }

    DevInode GetDevInode() const { return mDevInode; }

    const std::string& GetRealLogPath() const { return mRealLogPath; }

    // void SetTimeFormat(const std::string& timeFormat) { mTimeFormat = timeFormat; }

    // std::string GetTimeFormat() const { return mTimeFormat; }

    bool IsReadToEnd() const { return GetLastReadPos() == mLastFileSize; }

    bool HasDataInCache() const { return mCache.size(); }

    LogFileReaderPtrArray* GetReaderArray();

    void SetReaderArray(LogFileReaderPtrArray* readerArray);

    // // some Reader will overide these functions (eg. JsonLogFileReader)
    // virtual bool ParseLogLine(StringView buffer,
    //                           sls_logs::LogGroup& logGroup,
    //                           ParseLogError& error,
    //                           LogtailTime& lastLogLineTime,
    //                           std::string& lastLogTimeStr,
    //                           uint32_t& logGroupSize)
    //     = 0;
    // virtual bool LogSplit(const char* buffer,
    //                       int32_t size,
    //                       int32_t& lineFeed,
    //                       std::vector<StringView>& logIndex,
    //                       std::vector<StringView>& discardIndex);

    // added by xianzhi(bowen.gbw@antfin.com)
    static bool ParseLogTime(const char* buffer,
                             const boost::regex* reg,
                             LogtailTime& logTime,
                             const std::string& timeFormat,
                             const std::string& region = "",
                             const std::string& project = "",
                             const std::string& logStore = "",
                             const std::string& logPath = "");
    static bool GetLogTimeByOffset(const char* buffer,
                                   int32_t pos,
                                   LogtailTime& logTime,
                                   const std::string& timeFormat,
                                   const std::string& region = "",
                                   const std::string& project = "",
                                   const std::string& logStore = "",
                                   const std::string& logPath = "");

    bool IsFromCheckPoint() { return mLastFileSignatureHash != 0 && mLastFileSignatureSize > (size_t)0; }

    // void SetDelayAlarmBytes(int64_t value) { mReadDelayAlarmBytes = value; }

    // int64_t GetPackId() { return ++mPackId; }

    void SetDockerPath(const std::string& dockerBasePath, size_t dockerReplaceSize);

    const std::vector<sls_logs::LogTag>& GetExtraTags() { return mExtraTags; }

    void AddExtraTags(const std::vector<sls_logs::LogTag>& tags) {
        mExtraTags.insert(mExtraTags.end(), tags.begin(), tags.end());
    }

    // void SetDelaySkipBytes(int64_t value) { mReadDelaySkipBytes = value; }

    // void SetFuseMode(bool fusemode) { mIsFuseMode = fusemode; }

    // bool GetFuseMode() const { return mIsFuseMode; }

    // void SetMarkOffsetFlag(bool markOffsetFlag) { mMarkOffsetFlag = markOffsetFlag; }

    // bool GetMarkOffsetFlag() const { return mMarkOffsetFlag; }

    // void SetFuseTrimedFilename(const std::string& filename) { mFuseTrimedFilename = filename; }

    // std::string GetFuseTrimedFilename() const { return mFuseTrimedFilename; }

    void ResetTopic(const std::string& topicFormat);

    // SetReadBufferSize set reader buffer size, which controls the max size of single log.
    static void SetReadBufferSize(int32_t bufSize);

    // void SetSpecifiedYear(int32_t y) { mSpecifiedYear = y; }

    // void SetCloseUnusedInterval(int32_t interval) { mCloseUnusedInterval = interval; }

    // void SetPreciseTimestampConfig(bool enabled, const std::string& key, TimeStampUnit unit) {
    //     mPreciseTimestampConfig.enabled = enabled;
    //     mPreciseTimestampConfig.key = key;
    //     mPreciseTimestampConfig.unit = unit;
    // }

    // void SetTzOffsetSecond(bool tzAdjust = false, int logTzOffsetSecond = 0) {
    //     if (tzAdjust) {
    //         mTzOffsetSecond = logTzOffsetSecond - GetLocalTimeZoneOffsetSecond();
    //     } else {
    //         mTzOffsetSecond = 0;
    //     }
    // }

    // void SetAdjustApsaraMicroTimezone(bool adjustApsaraMicroTimezone = false) {
    //     mAdjustApsaraMicroTimezone = adjustApsaraMicroTimezone;
    // }

    std::unique_ptr<Event> CreateFlushTimeoutEvent();

    const std::string& GetProject() const { return mProject; }
    const std::string& GetLogstore() const { return mLogstore; }
    const std::string& GetRegion() const { return mRegion; }
    const std::string& GetConfigName() const { return mConfigName; }

    int64_t GetLogGroupKey() const { return mLogGroupKey; }

protected:
    bool GetRawData(LogBuffer& logBuffer, int64_t fileSize, bool allowRollback = true);
    void ReadUTF8(LogBuffer& logBuffer, int64_t end, bool& moreData, bool allowRollback = true);
    void ReadGBK(LogBuffer& logBuffer, int64_t end, bool& moreData, bool allowRollback = true);

    size_t
    ReadFile(LogFileOperator& logFileOp, void* buf, size_t size, int64_t& offset, TruncateInfo** truncateInfo = NULL);
    int32_t ParseTimeInBuffer(LogFileOperator& logFileOp,
                              int64_t begin,
                              int64_t end,
                              int32_t bootTime,
                              const std::string& timeFormat,
                              int64_t& filePos,
                              bool& found);
    static int ParseAllLines(
        char* buffer, size_t size, int32_t bootTime, const std::string& timeFormat, int32_t& parsedTime, int& pos);
    static int32_t ParseTime(const char* buffer, const std::string& timeFormat);
    void SetFilePosBackwardToFixedPos(LogFileOperator& logFileOp);

    bool CheckForFirstOpen(FileReadPolicy policy = BACKWARD_TO_FIXED_POS);
    void FixLastFilePos(LogFileOperator& logFileOp, int64_t endOffset);
    inline int64_t GetLastReadPos() const { // pos read but may not consumed, used for read needed
        return mLastFilePos + mCache.size();
    }

    static size_t BUFFER_SIZE;
    // std::string mRegion;
    // std::string mCategory;
    // std::string mConfigName;
    std::string mHostLogPath;
    std::string mHostLogPathDir;
    std::string mHostLogPathFile;
    std::string mRealLogPath; // real log path
    bool mSymbolicLinkFlag = false;
    std::string mSourceId;
    // int32_t mTailLimit; // KB
    uint64_t mLastFileSignatureHash = 0;
    uint32_t mLastFileSignatureSize = 0;
    int64_t mLastFilePos = 0; // pos read and consumed, used for next read begin
    int64_t mLastFileSize = 0;
    time_t mLastMTime = 0;
    std::string mCache;
    // std::string mProjectName;
    std::string mTopicName;
    time_t mLastUpdateTime;
    // boost::regex* mLogBeginRegPtr;
    // boost::regex* mLogContinueRegPtr;
    // boost::regex* mLogEndRegPtr;
    // int mReaderFlushTimeout;
    bool mLastForceRead = false;
    // FileEncoding mFileEncoding;
    // bool mDiscardUnmatch;
    // LogType mLogType;
    DevInode mDevInode;
    bool mFirstWatched = true;
    bool mFileDeleted = false;
    time_t mDeletedTime = 0;
    bool mContainerStopped = false;
    time_t mContainerStoppedTime = 0;
    time_t mReadStoppedContainerAlarmTime = 0;
    int32_t mReadDelayTime = 0;
    bool mSkipFirstModify = false;
    // int64_t mReadDelayAlarmBytes;
    // bool mPluginFlag;
    // int64_t mPackId;
    // int64_t mReadDelaySkipBytes; // if <=0, discard it, default 0.
    int32_t mLastEventTime; // last time when process modify event, updated in check file sig
    // int32_t mSpecifiedYear;
    // bool mIsFuseMode = false;
    // bool mMarkOffsetFlag = false;
    // std::string mTimeFormat; // for backward reading
    LogFileOperator mLogFileOp; // encapsulate fuse & non-fuse mode
    // std::string mFuseTrimedFilename;
    LogFileReaderPtrArray* mReaderArray = nullptr;
    // uint64_t mLogstoreKey;
    // mHostLogPath is `/logtail_host/var/xxx/home/admin/access.log`,
    // mDockerPath is `/home/admin/access.log`
    // we should use mDockerPath to extract topic and set it to __tag__:__path__
    std::string mDockerPath;
    std::vector<sls_logs::LogTag> mExtraTags;
    // int32_t mCloseUnusedInterval;

    // PreciseTimestampConfig mPreciseTimestampConfig;

    // int32_t mTzOffsetSecond;
    // bool mAdjustApsaraMicroTimezone;

    FileReaderConfig mReaderConfig;
    MultilineConfig mMultilineConfig;
    int64_t mLogGroupKey = 0;

    // since reader is destructed after the corresponding pipeline is removed, pipeline context used in destructor
    // should be copied explicitly from context.
    std::string mProject;
    std::string mLogstore;
    std::string mConfigName;
    std::string mRegion;

private:
    // Initialized when the exactly once feature is enabled.
    struct ExactlyOnceOption {
        std::string primaryCheckpointKey;
        LogstoreFeedBackKey fbKey;
        PrimaryCheckpointPB primaryCheckpoint;
        std::vector<RangeCheckpointPtr> rangeCheckpointPtrs;

        // Checkpoint for current read, maybe an existing checkpoint for replay or
        //  new checkpoint.
        RangeCheckpointPtr selectedCheckpoint;
        // Uncommitted checkpoints to replay.
        // Initialized by adjustParametersByRangeCheckpoints,
        // Used by selectCheckpointToReplay.
        std::deque<RangeCheckpointPtr> toReplayCheckpoints;
        // Recovered from checkpoints.
        int64_t lastComittedOffset = -1;

        uint32_t concurrency = 8;
    };
    std::shared_ptr<ExactlyOnceOption> mEOOption;

    // Select next checkpoint to recover from toReplayCheckpoints.
    // Called before read.
    //
    // It will check if the checkpoint to replay matches current file, mismatch cases:
    // - Mismatch between read offset and mLastFilePos.
    // - Mismatch between read length and the length of available file content.
    // If mismatch happened, all replay checkpoints will be dropped.
    //
    // @return nullptr if no more checkpoint need to be replayed.
    RangeCheckpointPtr selectCheckpointToReplay();

    // Adjust offset to skip hole for next checkpoint or last committed offset.
    // Called after read if current checkpoint is from replay checkpoints.
    //
    // Skip hole cases (compare to mLastFilePos):
    // 1. Next checkpoint's offset is bigger.
    // 2. No more checkpoint, but the last committed offset is bigger.
    void skipCheckpointRelayHole();

    // Return the size to read for following read.
    //
    // Complete selected checkpoint means that it is a replay checkpoint, so
    //  next read size should be specified.
    //
    // @param fileEnd: file size, ie. tell(seek(end)).
    // @param fromCpt: if the read size is recoveried from checkpoint, set it to true.
    size_t getNextReadSize(int64_t fileEnd, bool& fromCpt);

    StringView GetLastLine(StringView buffer, size_t end);

    // Update current checkpoint's read offset and length after success read.
    void setExactlyOnceCheckpointAfterRead(size_t readSize);

    // Return primary key of current reader by combining meta.
    //
    // Conflict resolve: file signature will be stored in primary checkpoint.
    std::string makePrimaryCheckpointKey() {
        std::string key;
        key.append(mReaderConfig.second->GetConfigName())
            .append("-")
            .append(mHostLogPath)
            .append("-")
            .append(std::to_string(mDevInode.dev))
            .append("-")
            .append(std::to_string(mDevInode.inode));
        return key;
    }

    // Initialize exactly once related member variables.
    void initExactlyOnce(uint32_t concurrency);

    // Validate if the primary checkpoint is ok to use by signature.
    //
    // Signature can be find in three place:
    // 1. Checkpoint v1: /tmp/logtail_check_point.
    // 2. Checkpoint v2: checkpoint database.
    // 3. Read file data and calculate.
    //
    // Validation procedure:
    // 1. Sig is found in checkpoint v2, read file data to validate.
    // 2. Sig is found in checkpoint v1, ignore the v2 checkpoint.
    // 3. Sig not found: ignore the checkpoint v2. It is safe because sig
    //  will be written before first read.
    //
    // By the way, if the v2 checkpoint is valid, set mLastFileSignatureSize/Hash.
    bool validatePrimaryCheckpoint(const PrimaryCheckpointPB& cpt);

    // Adjust parameters for first read by range checkpoints.
    //
    // Includes:
    // - mLastFilePos, GetLastReadPos
    // - mFirstWatched
    // - mEOOption->toReplayCheckpoints
    // - mEOOption->lastComittedOffset
    // These parameters will control the ReadLog.
    //
    // Replay Checkpoints Selection Algorithm
    // 1. Iterate all range checkpoints to find all uncommitted checkpoints.
    // 2. Iterate all range checkpoints to find the one with maximum read offset.
    // 3. Update member variables:
    //  3.1. nothing is found, use default value.
    //  3.2. uncommitted checkpoints are found:
    //   - Sort them by read offset
    //   - Set them as toReplayCheckpoints
    //   - Set GetLastReadPos = mLastFilePos = ReadOffsetOfFirstCheckpoint
    //   - Set mFirstWatched = false
    //   - If maximum is found and the checkpoint is committed, use it to update
    //    the last committed offset
    //  3.3. no minimum but maximum is found, it means all data in checkpoints have
    //   already been committed, so read should start from the position after it.
    //   - Set GetLastReadPos = mLastFilePos = ReadOffset(maxOffset) + ReadLength(maxOffset)
    //   - Set mFirstWatched = false
    void adjustParametersByRangeCheckpoints();

    // Update primary checkpoint when meta updated.
    void updatePrimaryCheckpointSignature();
    void updatePrimaryCheckpointRealPath();

    void handleUnmatchLogs(const char* buffer,
                           int& multiBeginIndex,
                           int endIndex,
                           std::vector<StringView>& logIndex,
                           std::vector<StringView>& discardIndex);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventDispatcherTest;
    friend class LogFileReaderUnittest;
    friend class LogMultiBytesUnittest;
    friend class ExactlyOnceReaderUnittest;
    friend class SenderUnittest;
    friend class AppConfigUnittest;
    friend class ModifyHandlerUnittest;
    friend class LogSplitUnittest;
    friend class LogSplitDiscardUnmatchUnittest;
    friend class LogSplitNoDiscardUnmatchUnittest;
    friend class RemoveLastIncompleteLogMultilineUnittest;
    friend class LogFileReaderCheckpointUnittest;

protected:
    void UpdateReaderManual();
#endif
};

struct LogBuffer : public SourceBuffer {
    StringView rawBuffer;
    LogFileReaderPtr logFileReader;
    FileInfoPtr fileInfo;
    TruncateInfoPtr truncateInfo;
    // Current buffer's checkpoint, for exactly once feature.
    RangeCheckpointPtr exactlyOnceCheckpoint;
    // Current buffer's offset in file, for log position meta feature.
    uint64_t readOffset = 0;
    uint64_t readLength = 0;

    LogBuffer() {}
    void SetDependecy(const LogFileReaderPtr& reader) { logFileReader = reader; }
};

} // namespace logtail
