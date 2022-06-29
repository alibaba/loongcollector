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

#include "LogFileReader.h"
#include <time.h>
#include <limits>
#include <numeric>
#if defined(_MSC_VER)
#include <fcntl.h>
#include <io.h>
#endif
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <cityhash/city.h>
#include "common/util.h"
#include "common/Flags.h"
#include "common/HashUtil.h"
#include "common/ErrorUtil.h"
#include "common/TimeUtil.h"
#include "common/FileSystemUtil.h"
#include "common/RandomUtil.h"
#include "common/Constants.h"
#include "sdk/Common.h"
#include "logger/Logger.h"
#include "checkpoint/CheckPointManager.h"
#include "checkpoint/CheckpointManagerV2.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogFileProfiler.h"
#include "event_handler/LogInput.h"
#include "app_config/AppConfig.h"
#include "config_manager/ConfigManager.h"
#include "common/LogFileCollectOffsetIndicator.h"
#include "fuse/UlogfsHandler.h"
#include "sender/Sender.h"
#include "GloablFileDescriptorManager.h"

using namespace sls_logs;
using namespace std;

DEFINE_FLAG_INT32(delay_bytes_upperlimit,
                  "if (total_file_size - current_readed_size) exceed uppperlimit, send READ_LOG_DELAY_ALARM, bytes",
                  200 * 1024 * 1024);
DEFINE_FLAG_INT32(read_delay_alarm_duration,
                  "if read delay elapsed this duration, send READ_LOG_DELAY_ALARM, seconds",
                  60);
DEFINE_FLAG_INT32(reader_close_unused_file_time, "second ", 60);
DEFINE_FLAG_INT32(skip_first_modify_time, "second ", 5 * 60);
DEFINE_FLAG_INT32(max_reader_open_files, "max fd count that reader can open max", 100000);
DEFINE_FLAG_INT32(truncate_pos_skip_bytes, "skip more xx bytes when truncate", 0);
DEFINE_FLAG_INT32(max_fix_pos_bytes, "", 128 * 1024);

namespace logtail {

#define COMMON_READER_INFO \
    ("log path", mLogPath)("real path", mRealLogPath)("config", mConfigName)("inode", mDevInode.inode)

size_t LogFileReader::BUFFER_SIZE = 1024 * 512; // 512KB

void LogFileReader::DumpMetaToMem(bool checkConfigFlag) {
    if (checkConfigFlag) {
        size_t index = mLogPath.rfind(PATH_SEPARATOR);
        if (index == string::npos || index == mLogPath.size() - 1) {
            LOG_INFO(sLogger, ("skip dump reader meta", "invalid log path")COMMON_READER_INFO);
            return;
        }
        string dirPath = mLogPath.substr(0, index);
        string fileName = mLogPath.substr(index + 1, mLogPath.size() - index - 1);
        if (ConfigManager::GetInstance()->FindBestMatch(dirPath, fileName) == NULL) {
            LOG_INFO(sLogger, ("skip dump reader meta", "best match not found")COMMON_READER_INFO);
            return;
        }
    }
    CheckPoint* checkPointPtr = new CheckPoint(mLogPath,
                                               mLastFilePos,
                                               mLastFileSignatureSize,
                                               mLastFileSignatureHash,
                                               mDevInode,
                                               mConfigName,
                                               mRealLogPath,
                                               mLogFileOp.IsOpen() ? 1 : 0);
    // use last event time as checkpoint's last update time
    checkPointPtr->mLastUpdateTime = mLastEventTime;
    CheckPointManager::Instance()->AddCheckPoint(checkPointPtr);
}
void LogFileReader::InitReader(bool tailExisted, FileReadPolicy policy, uint32_t eoConcurrency) {
    string buffer = LogFileProfiler::mIpAddr + "_" + mLogPath + "_" + CalculateRandomUUID();
    uint64_t cityHash = CityHash64(buffer.c_str(), buffer.size());
    mSourceId = ToHexString(cityHash);

    if (!tailExisted) {
        static CheckPointManager* checkPointManagerPtr = CheckPointManager::Instance();
        // hold on checkPoint share ptr, so this check point will not be delete in this block
        CheckPointPtr checkPointSharePtr;
        if (checkPointManagerPtr->GetCheckPoint(mDevInode, mConfigName, checkPointSharePtr)) {
            CheckPoint* checkPointPtr = checkPointSharePtr.get();
            mLastFilePos = checkPointPtr->mOffset;
            mLastReadPos = mLastFilePos;
            mLastFileSignatureHash = checkPointPtr->mSignatureHash;
            mLastFileSignatureSize = checkPointPtr->mSignatureSize;
            mRealLogPath = checkPointPtr->mRealFileName;
            mLastEventTime = checkPointPtr->mLastUpdateTime;
            LOG_DEBUG(sLogger,
                      ("init reader by checkpoint", mLogPath)(mRealLogPath, mLastFilePos)("config", mConfigName));
            // check if we should skip first modify
            // file is open or last update time is new
            if (checkPointPtr->mFileOpenFlag != 0
                || (int32_t)time(NULL) - checkPointPtr->mLastUpdateTime < INT32_FLAG(skip_first_modify_time)) {
                mSkipFirstModify = false;
            } else {
                mSkipFirstModify = true;
            }
            // delete checkpoint at last
            checkPointManagerPtr->DeleteCheckPoint(mDevInode, mConfigName);
            // because the reader is initialized by checkpoint, so set first watch to false
            mFirstWatched = false;
        }
    }

    if (eoConcurrency > 0) {
        initExactlyOnce(eoConcurrency);
    }

    if (mFirstWatched) {
        CheckForFirstOpen(policy);
    }
}

namespace detail {

    void updatePrimaryCheckpoint(const std::string& key, PrimaryCheckpointPB& cpt, const std::string& field) {
        cpt.set_update_time(time(NULL));
        if (CheckpointManagerV2::GetInstance()->SetPB(key, cpt)) {
            LOG_INFO(sLogger, ("update primary checkpoint", key)("field", field)("checkpoint", cpt.DebugString()));
        } else {
            LOG_WARNING(sLogger,
                        ("update primary checkpoint error", key)("field", field)("checkpoint", cpt.DebugString()));
        }
    }

    std::pair<size_t, size_t> getPartitionRange(size_t idx, size_t concurrency, size_t totalPartitionCount) {
        auto base = totalPartitionCount / concurrency;
        auto extra = totalPartitionCount % concurrency;
        if (extra == 0) {
            return std::make_pair(idx * base, (idx + 1) * base - 1);
        }
        size_t min = idx <= extra ? idx * (base + 1) : extra * (base + 1) + (idx - extra) * base;
        size_t max = idx < extra ? min + base : min + base - 1;
        return std::make_pair(min, max);
    }

} // namespace detail

void LogFileReader::initExactlyOnce(uint32_t concurrency) {
    mEOOption.reset(new ExactlyOnceOption);

    // Primary key.
    auto& primaryCptKey = mEOOption->primaryCheckpointKey;
    primaryCptKey = makePrimaryCheckpointKey();

    // Recover primary checkpoint from local if have.
    PrimaryCheckpointPB primaryCpt;
    static auto sCptM = CheckpointManagerV2::GetInstance();
    bool hasCheckpoint = sCptM->GetPB(primaryCptKey, primaryCpt);
    if (hasCheckpoint) {
        hasCheckpoint = validatePrimaryCheckpoint(primaryCpt);
        if (!hasCheckpoint) {
            LOG_WARNING(sLogger,
                        COMMON_READER_INFO("ignore primary checkpoint and delete range checkpoints", primaryCptKey));
            std::vector<std::string> rangeCptKeys;
            CheckpointManagerV2::AppendRangeKeys(primaryCptKey, concurrency, rangeCptKeys);
            sCptM->DeleteCheckpoints(rangeCptKeys);
        }
    }
    mEOOption->concurrency = hasCheckpoint ? primaryCpt.concurrency() : concurrency;
    if (!hasCheckpoint) {
        primaryCpt.set_concurrency(mEOOption->concurrency);
        primaryCpt.set_sig_hash(mLastFileSignatureHash);
        primaryCpt.set_sig_size(mLastFileSignatureSize);
        primaryCpt.set_config_name(mConfigName);
        primaryCpt.set_log_path(mLogPath);
        primaryCpt.set_real_path(mRealLogPath.empty() ? mLogPath : mRealLogPath);
        primaryCpt.set_dev(mDevInode.dev);
        primaryCpt.set_inode(mDevInode.inode);
        detail::updatePrimaryCheckpoint(mEOOption->primaryCheckpointKey, primaryCpt, "all (new)");
    }
    mEOOption->primaryCheckpoint.Swap(&primaryCpt);
    LOG_INFO(sLogger,
             ("primary checkpoint", primaryCptKey)("old", hasCheckpoint)("checkpoint",
                                                                         mEOOption->primaryCheckpoint.DebugString()));

    // Randomize range checkpoints index for load balance.
    std::vector<uint32_t> concurrencySequence(mEOOption->concurrency);
    std::iota(concurrencySequence.begin(), concurrencySequence.end(), 0);
    std::random_shuffle(concurrencySequence.begin(), concurrencySequence.end());
    // Initialize range checkpoints (recover from local if have).
    mEOOption->rangeCheckpointPtrs.resize(mEOOption->concurrency);
    std::string baseHashKey;
    for (size_t idx = 0; idx < concurrencySequence.size(); ++idx) {
        const uint32_t partIdx = concurrencySequence[idx];
        auto& rangeCpt = mEOOption->rangeCheckpointPtrs[idx];
        rangeCpt.reset(new RangeCheckpoint);
        rangeCpt->index = idx;
        rangeCpt->key = CheckpointManagerV2::MakeRangeKey(mEOOption->primaryCheckpointKey, partIdx);

        // No checkpoint, generate random hash key.
        bool newCpt = !hasCheckpoint || !sCptM->GetPB(rangeCpt->key, rangeCpt->data);
        if (newCpt) {
            if (baseHashKey.empty()) {
                baseHashKey = GenerateRandomHashKey();
            }

            // Map to partition range so that it can fits the case that the number of
            //  logstore's shards is bigger than concurreny.
            const size_t kPartitionCount = 512;
            auto partitionRange = detail::getPartitionRange(partIdx, mEOOption->concurrency, kPartitionCount);
            auto partitionID = partitionRange.first + rand() % (partitionRange.second - partitionRange.first + 1);
            rangeCpt->data.set_hash_key(GenerateHashKey(baseHashKey, partitionID, kPartitionCount));
            rangeCpt->data.set_sequence_id(sdk::kFirstHashKeySeqID);
            rangeCpt->data.set_committed(false);
        }
        LOG_DEBUG(sLogger,
                  ("range checkpoint", rangeCpt->key)("index", idx)("new", newCpt)("checkpoint",
                                                                                   rangeCpt->data.DebugString()));
    }

    // Initialize feedback queues.
    mEOOption->fbKey = QueueManager::GetInstance()->InitializeExactlyOnceQueues(
        mProjectName,
        mEOOption->primaryCheckpointKey + mEOOption->rangeCheckpointPtrs[0]->data.hash_key(),
        mEOOption->rangeCheckpointPtrs);
    for (auto& cpt : mEOOption->rangeCheckpointPtrs) {
        cpt->fbKey = mEOOption->fbKey;
    }

    adjustParametersByRangeCheckpoints();
}

bool LogFileReader::validatePrimaryCheckpoint(const PrimaryCheckpointPB& cpt) {
#define METHOD_LOG_PATTERN ("method", "validatePrimaryCheckpoint")("checkpoint", cpt.DebugString())
    auto const sigSize = cpt.sig_size();
    auto const sigHash = cpt.sig_hash();
    if (sigSize > 0) {
        auto filePath = cpt.real_path().empty() ? cpt.log_path() : cpt.real_path();
        auto hasFileBeenRotated = [&]() {
            auto devInode = GetFileDevInode(filePath);
            if (devInode == mDevInode) {
                return false;
            }

            auto dirPath = boost::filesystem::path(filePath).parent_path();
            const auto searchResult = SearchFilePathByDevInodeInDirectory(dirPath.string(), 0, mDevInode, nullptr);
            if (!searchResult) {
                LOG_WARNING(sLogger, METHOD_LOG_PATTERN("can not find file with dev inode", mDevInode.inode));
                return false;
            }
            const auto& newFilePath = searchResult.value();
            LOG_INFO(sLogger,
                     METHOD_LOG_PATTERN("file has been rotated from", "")("from", filePath)("to", newFilePath));
            filePath = newFilePath;
            return true;
        };
        if (CheckFileSignature(filePath, sigHash, sigSize, mIsFuseMode)
            || (hasFileBeenRotated() && CheckFileSignature(filePath, sigHash, sigSize, mIsFuseMode))) {
            mLastFileSignatureSize = sigSize;
            mLastFileSignatureHash = sigHash;
            mRealLogPath = filePath;
            return true;
        }
        LOG_WARNING(sLogger, METHOD_LOG_PATTERN("mismatch with local file content", filePath));
        return false;
    }
    if (mLastFileSignatureSize > 0) {
        LOG_WARNING(
            sLogger,
            METHOD_LOG_PATTERN("mismatch with checkpoint v1 signature, sig size", sigSize)("sig hash", sigHash));
        return false;
    }
    return false;
#undef METHOD_LOG_PATTERN
}

void LogFileReader::adjustParametersByRangeCheckpoints() {
    auto& uncommittedCheckpoints = mEOOption->toReplayCheckpoints;
    uint32_t maxOffsetIndex = mEOOption->concurrency;
    for (uint32_t idx = 0; idx < mEOOption->concurrency; ++idx) {
        auto& rangeCpt = mEOOption->rangeCheckpointPtrs[idx];
        if (!rangeCpt->data.has_read_offset()) {
            continue;
        } // Skip new checkpoint.

        if (!rangeCpt->data.committed()) {
            uncommittedCheckpoints.push_back(rangeCpt);
        } else {
            rangeCpt->IncreaseSequenceID();
        }

        const int64_t maxReadOffset = maxOffsetIndex != mEOOption->concurrency
            ? mEOOption->rangeCheckpointPtrs[maxOffsetIndex]->data.read_offset()
            : -1;
        if (static_cast<int64_t>(rangeCpt->data.read_offset()) > maxReadOffset) {
            maxOffsetIndex = idx;
        }
    }

    // Find uncommitted checkpoints, sort them by offset for replay.
    if (!uncommittedCheckpoints.empty()) {
        std::sort(uncommittedCheckpoints.begin(),
                  uncommittedCheckpoints.end(),
                  [](const RangeCheckpointPtr& lhs, const RangeCheckpointPtr& rhs) {
                      return lhs->data.read_offset() < rhs->data.read_offset();
                  });
        auto& firstCpt = uncommittedCheckpoints.front()->data;
        mLastReadPos = mLastFilePos = firstCpt.read_offset();
        mFirstWatched = false;

        // Set skip position if there are comitted checkpoints.
        if (maxOffsetIndex != mEOOption->concurrency) {
            auto& cpt = mEOOption->rangeCheckpointPtrs[maxOffsetIndex]->data;
            if (cpt.committed()) {
                mEOOption->lastComittedOffset = static_cast<int64_t>(cpt.read_offset() + cpt.read_length());
            }
        }

        LOG_INFO(
            sLogger,
            ("initialize reader", "uncommitted checkpoints")COMMON_READER_INFO("count", uncommittedCheckpoints.size())(
                "first checkpoint", firstCpt.DebugString())("last committed offset", mEOOption->lastComittedOffset));
    }
    // All checkpoints are committed, skip them.
    else if (maxOffsetIndex != mEOOption->concurrency) {
        auto& cpt = mEOOption->rangeCheckpointPtrs[maxOffsetIndex]->data;
        mLastReadPos = mLastFilePos = cpt.read_offset() + cpt.read_length();
        mFirstWatched = false;
        LOG_INFO(sLogger,
                 ("initialize reader", "checkpoint with max offset")COMMON_READER_INFO("max index", maxOffsetIndex)(
                     "checkpoint", cpt.DebugString()));
    } else {
        LOG_INFO(sLogger,
                 ("initialize reader", "no available checkpoint")COMMON_READER_INFO("first watch", mFirstWatched));
    }
}

void LogFileReader::updatePrimaryCheckpointSignature() {
    auto& cpt = mEOOption->primaryCheckpoint;
    cpt.set_sig_size(mLastFileSignatureSize);
    cpt.set_sig_hash(mLastFileSignatureHash);
    detail::updatePrimaryCheckpoint(mEOOption->primaryCheckpointKey, cpt, "signature");
}

void LogFileReader::updatePrimaryCheckpointRealPath() {
    auto& cpt = mEOOption->primaryCheckpoint;
    cpt.set_real_path(mRealLogPath);
    detail::updatePrimaryCheckpoint(mEOOption->primaryCheckpointKey, cpt, "real_path");
}

LogFileReader::LogFileReader(const string& projectName,
                             const string& category,
                             const string& logPathDir,
                             const std::string& logPathFile,
                             int32_t tailLimit,
                             bool discardUnmatch,
                             bool dockerFileFlag) {
    mFirstWatched = true;
    mProjectName = projectName;
    mCategory = category;
    mTopicName = "";
    mLogPathFile = logPathFile;
    mLogPath = PathJoin(logPathDir, logPathFile);
    mTailLimit = tailLimit;
    mLastFilePos = 0;
    mLastFileSize = 0;
    mLogBeginRegPtr = NULL;
    mDiscardUnmatch = discardUnmatch;
    mLastUpdateTime = time(NULL);
    mLastEventTime = mLastUpdateTime;
    mFileDeleted = false;
    mDeletedTime = (time_t)0;
    mContainerStopped = false;
    mContainerStoppedTime = (time_t)0;
    mReadStoppedContainerAlarmTime = (time_t)0;
    mLastFileSignatureHash = 0;
    mLastFileSignatureSize = 0;
    mReadDelayTime = 0;
    mReaderArray = NULL;
    mSkipFirstModify = false;
    mReadDelayAlarmBytes = INT32_FLAG(delay_bytes_upperlimit);
    mPluginFlag = false;
    mPackId = 0;
    mReadDelaySkipBytes = 0;
    mCloseUnusedInterval = INT32_FLAG(reader_close_unused_file_time);
    mPreciseTimestampConfig.enabled = false;
}

LogFileReader::LogFileReader(const std::string& projectName,
                             const std::string& category,
                             const std::string& logPathDir,
                             const std::string& logPathFile,
                             int32_t tailLimit,
                             const std::string& topicFormat,
                             const std::string& groupTopic,
                             FileEncoding fileEncoding,
                             bool discardUnmatch,
                             bool dockerFileFlag) {
    mFirstWatched = true;
    mProjectName = projectName;
    mCategory = category;
    mLogPathFile = logPathFile;
    mLogPath = PathJoin(logPathDir, logPathFile);
    mTailLimit = tailLimit;
    mLastFilePos = 0;
    mLastFileSize = 0;
    const std::string lowerConfig = ToLowerCaseString(topicFormat);
    if (lowerConfig == "none" || lowerConfig == "default" || lowerConfig == "customized") {
        // For customized, it will be set through SetTopicName.
        mTopicName = "";
    } else if (lowerConfig == "global_topic") {
        static LogtailGlobalPara* sGlobalPara = LogtailGlobalPara::Instance();
        mTopicName = sGlobalPara->GetTopic();
    } else if (lowerConfig == "group_topic")
        mTopicName = groupTopic;
    else if (!dockerFileFlag) // if docker file, wait for reset topic format
        mTopicName = GetTopicName(topicFormat, mLogPath);
    mFileEncoding = fileEncoding;
    mLogBeginRegPtr = NULL;
    mDiscardUnmatch = discardUnmatch;
    mLastUpdateTime = time(NULL);
    mLastEventTime = mLastUpdateTime;
    mFileDeleted = false;
    mDeletedTime = (time_t)0;
    mContainerStopped = false;
    mContainerStoppedTime = (time_t)0;
    mReadStoppedContainerAlarmTime = (time_t)0;
    mLastFileSignatureHash = 0;
    mLastFileSignatureSize = 0;
    mReadDelayTime = 0;
    mReaderArray = NULL;
    mSkipFirstModify = false;
    mReadDelayAlarmBytes = INT32_FLAG(delay_bytes_upperlimit);
    mPluginFlag = false;
    mPackId = 0;
    mReadDelaySkipBytes = 0;
    mCloseUnusedInterval = INT32_FLAG(reader_close_unused_file_time);
    mPreciseTimestampConfig.enabled = false;
}

void LogFileReader::SetDockerPath(const std::string& dockerBasePath, size_t dockerReplaceSize) {
    if (dockerReplaceSize > (size_t)0 && mLogPath.size() > dockerReplaceSize && !dockerBasePath.empty()) {
        if (dockerBasePath.size() == (size_t)1) {
            mDockerPath = mLogPath.substr(dockerReplaceSize);
        } else {
            mDockerPath = dockerBasePath + mLogPath.substr(dockerReplaceSize);
        }

        LOG_DEBUG(sLogger, ("convert docker file path", "")("host path", mLogPath)("final path", mDockerPath));
    }
}

void LogFileReader::SetReadFromBeginning() {
    mLastFilePos = 0;
    mLastReadPos = 0;
    LOG_DEBUG(sLogger, ("begin to read file", mLogPath)("start offset", mLastFilePos));
    mFirstWatched = false;
}

bool LogFileReader::SetReadPosForBackwardReading(LogFileOperator& op) {
    if (mTimeFormat.empty()) {
        LOG_ERROR(sLogger, ("time format is empty", "cannnot set read pos backward"));
        return false;
    }

    int32_t systemBootTime = LogFileProfiler::mSystemBootTime;
    if (systemBootTime <= 0) {
        LOG_ERROR(sLogger, ("invalid boot time", "cannnot set read pos backward"));
        return false;
    }

    int64_t fileSize = op.GetFileSize();
    int64_t begin = 0, end = fileSize - 1, nextPos = -1;

    bool found = false;
    int32_t logTime = -1;
    while (end - begin > 0) {
        logTime = ParseTimeInBuffer(op, begin, end, systemBootTime, mTimeFormat, nextPos, found);
        if (logTime == -1) {
            // if we cannot parse time in this buffer, break
            LOG_WARNING(sLogger, ("failed to parse time", "cannnot set read pos backward"));
            return false;
        }

        if (found)
            break;

        if (logTime < systemBootTime)
            begin = nextPos;
        else //if (logTime >= systemBootTime)
            end = nextPos;
    }

    if (found || nextPos != -1) {
        mLastFilePos = nextPos;
        mLastReadPos = nextPos;
    }
    return true;
}

int32_t LogFileReader::ParseTimeInBuffer(LogFileOperator& op,
                                         int64_t begin,
                                         int64_t end,
                                         int32_t bootTime,
                                         const std::string& timeFormat,
                                         int64_t& filePos,
                                         bool& found) {
    if (begin >= end)
        return -1;

    if (!op.IsOpen()) {
        return -1;
    }

    // we should fix begin and end
    int64_t mid = (begin + end) / 2;
    begin = std::max(begin, (int64_t)(mid - BUFFER_SIZE));
    end = std::min((int64_t)(mid + BUFFER_SIZE), end);
    size_t size = (size_t)(end - begin) + 1;

    char* buffer = new char[size]();
    size_t nbytes = ReadFile(op, buffer, size, begin);

    // find start pos and end pos, searching for '\n'
    int lineBegin = 0, lineEnd = (int)nbytes - 1;
    // if begin is 0, we should not find \n from begin
    while (begin != 0 && lineBegin < (int)nbytes - 1) {
        if (buffer[lineBegin++] == '\n')
            break;
    }
    while (lineEnd > 0) {
        if (buffer[lineEnd] == '\n')
            break;
        --lineEnd;
    }

    if (lineBegin > lineEnd) {
        delete[] buffer;
        return -1;
    }

    // now lineBegin is the beginning of first whole line
    // and lineEnd is the end of last whole, specially, buffer[lineEnd] is \n
    // so sz is the size of whole lines
    size_t sz = (size_t)(lineEnd - lineBegin) + 1;
    int32_t parsedTime = -1, pos = -1;
    int result = ParseAllLines(buffer + lineBegin, sz, bootTime, timeFormat, parsedTime, pos);

    if (result == 1)
        filePos = begin + lineBegin + pos;
    else if (result == 2)
        filePos = begin + lineEnd + 1;
    else {
        if (result == 0) {
            found = true;
            filePos = begin + lineBegin + pos;
        }
        // if result == -1, parsedTime is -1
    }

    delete[] buffer;
    return parsedTime;
}

int LogFileReader::ParseAllLines(
    char* buffer, size_t size, int32_t bootTime, const std::string& timeFormat, int32_t& parsedTime, int& pos) {
    std::vector<int> lineFeedPos;
    int begin = 0;
    // here we push back 0 because the pos 0 must be the beginning of the first line
    lineFeedPos.push_back(begin);

    // data in buffer is between [0, size)
    for (int i = 1; i < (int)size; ++i) {
        if (buffer[i] == '\n') {
            buffer[i] = '\0';
            begin = i + 1;
            // if begin == size, we meet the end of all lines buffer
            if (begin < (int)size)
                lineFeedPos.push_back(begin);
        }
    }

    // parse first and last line
    size_t firstLogIndex = 0, lastLogIndex = lineFeedPos.size() - 1;
    int32_t firstLogTime = -1, lastLogTime = -1;
    for (size_t i = 0; i < lineFeedPos.size(); ++i) {
        firstLogTime = ParseTime(buffer + lineFeedPos[i], timeFormat);
        if (firstLogTime != -1) {
            firstLogIndex = i;
            break;
        }
    }
    if (firstLogTime >= bootTime) {
        parsedTime = firstLogTime;
        pos = lineFeedPos[firstLogIndex];
        return 1;
    }

    for (int i = (int)lineFeedPos.size() - 1; i >= 0; --i) {
        lastLogTime = ParseTime(buffer + lineFeedPos[i], timeFormat);
        if (lastLogTime != -1) {
            lastLogIndex = (size_t)i;
            break;
        }
    }
    if (lastLogTime < bootTime) {
        parsedTime = lastLogTime;
        pos = lineFeedPos[lastLogIndex];
        return 2;
    }

    // parse all lines, now fisrtLogTime < bootTime, lastLogTime >= booTime
    for (size_t i = firstLogIndex + 1; i < lastLogIndex; ++i) {
        parsedTime = ParseTime(buffer + lineFeedPos[i], timeFormat);
        if (parsedTime >= bootTime) {
            pos = lineFeedPos[i];
            return 0;
        }
    }

    parsedTime = lastLogTime;
    pos = lineFeedPos[lastLogIndex];
    return 0;
}

int32_t LogFileReader::ParseTime(const char* buffer, const std::string& timeFormat) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    const char* result = strptime(buffer, timeFormat.c_str(), &tm);
    tm.tm_isdst = -1;
    if (result != NULL) {
        time_t logTime = mktime(&tm);
        return logTime;
    }
    return -1;
}

bool LogFileReader::CheckForFirstOpen(FileReadPolicy policy) {
    mFirstWatched = false;
    if (mLastFilePos != 0)
        return false;

    // here we should NOT use mLogFileOp to open file, because LogFileReader may be created from checkpoint
    // we just want to set file pos, then a TEMPORARY object for LogFileOperator is needed here, not a class member LogFileOperator
    // we should open file via UpdateFilePtr, then start reading
    LogFileOperator op;
    op.Open(mLogPath.c_str(), mIsFuseMode);
    if (op.IsOpen() == false) {
        mLastFilePos = 0;
        mLastReadPos = 0;
        LOG_DEBUG(sLogger, ("begin to read file", mLogPath)("start offset", mLastFilePos));
        auto error = GetErrno();
        if (fsutil::Dir::IsENOENT(error))
            return true;
        else {
            LOG_ERROR(sLogger, ("open log file fail", mLogPath)("errno", ErrnoToString(error)));
            LogtailAlarm::GetInstance()->SendAlarm(OPEN_LOGFILE_FAIL_ALARM,
                                                   string("Failed to open log file: ") + mLogPath
                                                       + "; errono:" + ErrnoToString(error),
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
            return false;
        }
    }

    if (policy == BACKWARD_TO_FIXED_POS) {
        SetFilePosBackwardToFixedPos(op);
    } else if (policy == BACKWARD_TO_BOOT_TIME) {
        bool succeeded = SetReadPosForBackwardReading(op);
        if (!succeeded) {
            // fallback
            SetFilePosBackwardToFixedPos(op);
        }
    } else if (policy == BACKWARD_TO_BEGINNING) {
        mLastFilePos = 0;
        mLastReadPos = 0;
    } else {
        LOG_ERROR(sLogger, ("invalid file read policy for file", mLogPath));
        return false;
    }
    LOG_DEBUG(sLogger, ("begin to read file", mLogPath)("start offset", mLastFilePos));
    return true;
}

void LogFileReader::SetFilePosBackwardToFixedPos(LogFileOperator& op) {
    int64_t endOffset = op.GetFileSize();
    mLastFilePos = endOffset <= ((int64_t)mTailLimit * 1024) ? 0 : (endOffset - ((int64_t)mTailLimit * 1024));
    mLastReadPos = mLastFilePos;
    FixLastFilePos(op, endOffset);
}

void LogFileReader::FixLastFilePos(LogFileOperator& op, int64_t endOffset) {
    if (mLastFilePos == 0 || op.IsOpen() == false) {
        return;
    }
    int32_t readSize = endOffset - mLastFilePos < INT32_FLAG(max_fix_pos_bytes) ? endOffset - mLastFilePos
                                                                                : INT32_FLAG(max_fix_pos_bytes);
    char* readBuf = (char*)malloc(readSize + 1);
    memset(readBuf, 0, readSize + 1);
    size_t readSizeReal = ReadFile(op, readBuf, readSize, mLastFilePos);
    if (readSizeReal == (size_t)0) {
        free(readBuf);
        return;
    }
    for (size_t i = 0; i < readSizeReal - 1; ++i) {
        if (readBuf[i] == '\n') {
            if (mLogBeginRegPtr == NULL) {
                mLastFilePos += i + 1;
                mLastReadPos = mLastFilePos;
                free(readBuf);
                return;
            }
            // cast '\n' to '\0'
            readBuf[i] = '\0';
        }
    }
    string exception;
    if (mLogBeginRegPtr != NULL) {
        for (size_t i = 0; i < readSizeReal - 1; ++i) {
            if (readBuf[i] == '\0' && BoostRegexMatch(readBuf + i + 1, *mLogBeginRegPtr, exception)) {
                mLastFilePos += i + 1;
                mLastReadPos = mLastFilePos;
                free(readBuf);
                return;
            }
        }
    }

    LOG_INFO(sLogger,
             ("file path", mLogPath)("can not fix last file pos, no begin line found from",
                                     mLastFilePos)("to", mLastFilePos + readSizeReal));

    free(readBuf);
    return;
}

std::string LogFileReader::GetTopicName(const std::string& topicConfig, const std::string& path) {
    std::string finalPath = mDockerPath.size() > 0 ? mDockerPath : path;
    size_t len = finalPath.size();
    // ignore the ".1" like suffix when the log file is roll back
    if (len > 2 && finalPath[len - 2] == '.' && finalPath[len - 1] > '0' && finalPath[len - 1] < '9') {
        finalPath = finalPath.substr(0, len - 2);
    }

    {
        string res;
        // use xpressive
        std::vector<string> keys;
        std::vector<string> values;
        if (ExtractTopics(finalPath, topicConfig, keys, values)) {
            size_t matchedSize = values.size();
            if (matchedSize == (size_t)1) {
                // != default topic name
                if (keys[0] != "__topic_1__") {
                    sls_logs::LogTag tag;
                    tag.set_key(keys[0]);
                    tag.set_value(values[0]);
                    mExtraTags.push_back(tag);
                }
                return values[0];
            } else {
                for (size_t i = 0; i < matchedSize; ++i) {
                    if (res.empty()) {
                        res = values[i];
                    } else {
                        res = res + "_" + values[i];
                    }
                    sls_logs::LogTag tag;
                    tag.set_key(keys[i]);
                    tag.set_value(values[i]);
                    mExtraTags.push_back(tag);
                }
            }
            return res;
        }
    }

    boost::match_results<const char*> what;
    string res, exception;
    // catch exception
    boost::regex topicRegex;
    try {
        topicRegex = boost::regex(topicConfig.c_str());
        if (BoostRegexMatch(finalPath.c_str(), topicRegex, exception, what, boost::match_default)) {
            size_t matchedSize = what.size();
            for (size_t i = 1; i < matchedSize; ++i) {
                if (res.empty()) {
                    res = what[i];
                } else {
                    res = res + "_" + what[i];
                }
                if (matchedSize > 2) {
                    sls_logs::LogTag tag;
                    tag.set_key(string("__topic_") + ToString(i) + "__");
                    tag.set_value(what[i]);
                    mExtraTags.push_back(tag);
                }
            }
        } else {
            if (!exception.empty())
                LOG_ERROR(sLogger,
                          ("extract topic by regex", "fail")("exception", exception)("project", mProjectName)(
                              "logstore", mCategory)("path", finalPath)("regx", topicConfig));
            else
                LOG_WARNING(sLogger,
                            ("extract topic by regex", "fail")("project", mProjectName)("logstore", mCategory)(
                                "path", finalPath)("regx", topicConfig));

            LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                   string("extract topic by regex fail, exception:") + exception
                                                       + ", path:" + finalPath + ", regex:" + topicConfig,
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
        }
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("extract topic by regex", "fail")("exception", exception)("project", mProjectName)(
                      "logstore", mCategory)("path", finalPath)("regx", topicConfig));
        LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                               string("extract topic by regex fail, exception:") + exception
                                                   + ", path:" + finalPath + ", regex:" + topicConfig,
                                               mProjectName,
                                               mCategory,
                                               mRegion);
    }

    return res;
}

RangeCheckpointPtr LogFileReader::selectCheckpointToReplay() {
    if (mEOOption->toReplayCheckpoints.empty()) {
        return nullptr;
    }

    do {
        auto& last = mEOOption->toReplayCheckpoints.back()->data;
        if (static_cast<int64_t>(last.read_offset() + last.read_length()) > mLastFileSize) {
            LOG_ERROR(sLogger,
                      ("current file size does not match last checkpoint",
                       "")COMMON_READER_INFO("file size", mLastFileSize)("checkpoint", last.DebugString()));
            break;
        }
        auto& first = mEOOption->toReplayCheckpoints.front()->data;
        if (static_cast<int64_t>(first.read_offset()) != mLastFilePos) {
            LOG_ERROR(sLogger,
                      ("current offset does not match first checkpoint",
                       "")COMMON_READER_INFO("offset", mLastFilePos)("checkpoint", first.DebugString()));
            break;
        }

        auto cpt = mEOOption->toReplayCheckpoints.front();
        mEOOption->toReplayCheckpoints.pop_front();
        return cpt;
    } while (false);

    LOG_ERROR(sLogger, COMMON_READER_INFO("delete all checkpoints to replay", mEOOption->toReplayCheckpoints.size()));
    for (auto& cpt : mEOOption->toReplayCheckpoints) {
        cpt->IncreaseSequenceID();
    }
    mEOOption->toReplayCheckpoints.clear();
    return nullptr;
}

void LogFileReader::skipCheckpointRelayHole() {
    if (mEOOption->toReplayCheckpoints.empty()) {
        if (mEOOption->lastComittedOffset != -1 && mEOOption->lastComittedOffset > mLastFilePos) {
            LOG_INFO(sLogger,
                     COMMON_READER_INFO("no more checkpoint to replay", "skip to last committed offset")(
                         "offset", mEOOption->lastComittedOffset)("current", mLastFilePos));
            mLastFilePos = mEOOption->lastComittedOffset;
            mEOOption->lastComittedOffset = -1;
        }
        return;
    }

    auto& next = mEOOption->toReplayCheckpoints.front()->data;
    auto const readOffset = static_cast<int64_t>(next.read_offset());
    if (readOffset == mLastFilePos) {
        return;
    }
    LOG_INFO(sLogger,
             ("skip replay hole for next checkpoint, size", readOffset - mLastFilePos)
                 COMMON_READER_INFO("offset", mLastFilePos)("checkpoint", next.DebugString()));
    mLastFilePos = readOffset;
}

bool LogFileReader::ReadLog(LogBuffer*& logBuffer) {
    if (mLogFileOp.IsOpen() == false) {
        LOG_ERROR(sLogger, ("unknow error, log file not open", mLogPath));
        return false;
    }
    if (AppConfig::GetInstance()->IsInputFlowControl())
        LogInput::GetInstance()->FlowControl();

    if (mFirstWatched && (mLastFilePos == 0))
        CheckForFirstOpen();

    // Init checkpoint for this read, new if no checkpoint to replay.
    if (mEOOption) {
        mEOOption->selectedCheckpoint = selectCheckpointToReplay();
        if (!mEOOption->selectedCheckpoint) {
            mEOOption->selectedCheckpoint.reset(new RangeCheckpoint);
            mEOOption->selectedCheckpoint->fbKey = mEOOption->fbKey;
        }
    }

    char* buffer = NULL;
    size_t size = 0;
    FileInfo* fileInfo = NULL;
    TruncateInfo* truncateInfo = NULL;
    auto const beginOffset = mLastFilePos;
    bool moreData = GetRawData(buffer, &size, mLastFileSize, fileInfo, truncateInfo);
    if (size > 0) {
        FileInfoPtr fileInfoPtr(fileInfo);
        TruncateInfoPtr truncateInfoPtr(truncateInfo);
        logBuffer = new LogBuffer(buffer, size, fileInfoPtr, truncateInfoPtr);
        if (mEOOption) {
            // This read was replayed by checkpoint, adjust mLastFilePos to skip hole.
            if (mEOOption->selectedCheckpoint->IsComplete()) {
                skipCheckpointRelayHole();
            }
            logBuffer->exactlyOnceCheckpoint = mEOOption->selectedCheckpoint;
            logBuffer->beginOffset = logBuffer->exactlyOnceCheckpoint->data.read_offset();
        } else {
            logBuffer->beginOffset = beginOffset;
        }
    } else {
        // if size == 0 and pointers below is not NULL(memory allocated in GetRawData),
        // then we should delete pointers in case of memory leak
        if (buffer != NULL) {
            delete[] buffer;
        }

        if (fileInfo != NULL) {
            delete fileInfo;
        }
        if (truncateInfo != NULL) {
            delete truncateInfo;
        }
    }
    LOG_DEBUG(sLogger,
              ("read log file", mRealLogPath)("last file pos", mLastFilePos)("last file size",
                                                                             mLastFileSize)("read size", size));
    return moreData;
}

void LogFileReader::OnOpenFileError() {
    switch (errno) {
        case ENOENT:
            LOG_DEBUG(sLogger, ("log file not exist, probably caused by rollback", mLogPath));
            break;
        case EACCES:
            LOG_ERROR(sLogger, ("open log file fail because of permission", mLogPath));
            LogtailAlarm::GetInstance()->SendAlarm(LOGFILE_PERMINSSION_ALARM,
                                                   string("Failed to open log file because of permission: ") + mLogPath,
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
            break;
        case EMFILE:
            LOG_ERROR(sLogger, ("too many open file", mLogPath));
            LogtailAlarm::GetInstance()->SendAlarm(OPEN_LOGFILE_FAIL_ALARM,
                                                   string("Failed to open log file because of : Too many open files")
                                                       + mLogPath,
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
            break;
        default:
            LOG_ERROR(sLogger, ("open log file fail", mLogPath)("errno", ErrnoToString(GetErrno())));
            LogtailAlarm::GetInstance()->SendAlarm(OPEN_LOGFILE_FAIL_ALARM,
                                                   string("Failed to open log file: ") + mLogPath
                                                       + "; errono:" + ErrnoToString(GetErrno()),
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
    }
}

bool LogFileReader::UpdateFilePtr() {
    // move last update time before check IsValidToPush
    mLastUpdateTime = time(NULL);
    if (mLogFileOp.IsOpen() == false) {
        // we may have mislabeled the deleted flag and then closed fd. Correct it here.
        SetFileDeleted(false);
        if (GloablFileDescriptorManager::GetInstance()->GetOpenedFilePtrSize() > INT32_FLAG(max_reader_open_files)) {
            LOG_ERROR(sLogger,
                      ("log file reader fd limit, too many open files",
                       mLogPath)(mProjectName, mCategory)("limit", INT32_FLAG(max_reader_open_files)));
            LogtailAlarm::GetInstance()->SendAlarm(OPEN_FILE_LIMIT_ALARM,
                                                   string("Failed to open log file: ") + mLogPath
                                                       + " limit:" + ToString(INT32_FLAG(max_reader_open_files)),
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
            //set errno to "too many open file"
            errno = EMFILE;
            return false;
        }
        int32_t tryTime = 0;
        LOG_DEBUG(sLogger, ("UpdateFilePtr open log file ", mLogPath));
        if (mRealLogPath.size() > 0) {
            while (tryTime++ < 5) {
                mLogFileOp.Open(mRealLogPath.c_str(), mIsFuseMode);
                if (mLogFileOp.IsOpen() == false) {
                    usleep(100);
                } else {
                    break;
                }
            }
            if (mLogFileOp.IsOpen() == false) {
                OnOpenFileError();
                LOG_WARNING(sLogger, ("LogFileReader open real log file failed", mRealLogPath));
            } else if (CheckDevInode()) {
                GloablFileDescriptorManager::GetInstance()->OnFileOpen(this);
                return true;
            } else {
                mLogFileOp.Close();
            }
        }
        if (mRealLogPath == mLogPath) {
            LOG_INFO(sLogger,
                     ("log file dev inode changed or file deleted ",
                      "prepare to delete reader or put reader into rotated map")("log path", mRealLogPath));
            return false;
        }
        tryTime = 0;
        while (tryTime++ < 5) {
            mLogFileOp.Open(mLogPath.c_str(), mIsFuseMode);
            if (mLogFileOp.IsOpen() == false) {
                usleep(100);
            } else {
                break;
            }
        }
        if (mLogFileOp.IsOpen() == false) {
            OnOpenFileError();
            LOG_WARNING(sLogger, ("LogFileReader open log file failed", mLogPath));
            return false;
        } else if (CheckDevInode()) {
            // the mLogPath's dev inode equal to mDevInode, so real log path is mLogPath
            mRealLogPath = mLogPath;
            GloablFileDescriptorManager::GetInstance()->OnFileOpen(this);
            return true;
        } else {
            mLogFileOp.Close();
        }
        LOG_INFO(sLogger,
                 ("log file dev inode changed or file deleted ", "prepare to delete reader")(mLogPath, mRealLogPath));
        return false;
    }
    return true;
}

bool LogFileReader::CloseTimeoutFilePtr(int32_t curTime) {
    int32_t timeOut = (int32_t)(mCloseUnusedInterval / 100.f * (100 + rand() % 50));
    if (mLogFileOp.IsOpen() && curTime - mLastUpdateTime > timeOut) {
        fsutil::PathStat buf;
        if (mLogFileOp.Stat(buf) != 0) {
            return false;
        }
        if ((int64_t)buf.GetFileSize() == mLastFilePos) {
            LOG_DEBUG(sLogger,
                      ("clost timeout fileptr", mLogPath)(mRealLogPath, mLastFilePos)("inode", mDevInode.inode));
            CloseFilePtr();
            // delete item in LogFileCollectOffsetIndicator map
            LogFileCollectOffsetIndicator::GetInstance()->DeleteItem(mLogPath, mDevInode);
            return true;
        }
    }
    return false;
}

void LogFileReader::CloseFilePtr() {
    if (mLogFileOp.IsOpen()) {
        LOG_DEBUG(sLogger, ("start close LogFileReader", mLogPath));

        // if mLogPath is symbolic link, then we should not update it accrding to /dev/fd/xx
        if (!mSymbolicLinkFlag) {
            // retrieve file path from file descriptor in order to open it later
            string curRealLogPath = mLogFileOp.GetFilePath();
            if (!curRealLogPath.empty()) {
                LOG_DEBUG(sLogger,
                          ("reader update real log path", mLogPath)("from", mRealLogPath)("to", curRealLogPath));
                mRealLogPath = curRealLogPath;
                if (mEOOption && mRealLogPath != mEOOption->primaryCheckpoint.real_path()) {
                    updatePrimaryCheckpointRealPath();
                }
            } else {
                LOG_WARNING(sLogger, ("failed to get real log path", mLogPath));
            }
        }

        if (mLogFileOp.Close() != 0) {
            int fd = mLogFileOp.GetFd();
            LOG_WARNING(
                sLogger,
                ("close file ptr error:", mLogPath)("error", strerror(errno))("fd", fd)("inode", mDevInode.inode));
            LogtailAlarm::GetInstance()->SendAlarm(OPEN_LOGFILE_FAIL_ALARM,
                                                   string("close file ptr error because of ") + strerror(errno)
                                                       + ", file path: " + mLogPath + ", inode: "
                                                       + ToString(mDevInode.inode) + ", inode: " + ToString(fd),
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
        }
        // always call OnFileClose
        GloablFileDescriptorManager::GetInstance()->OnFileClose(this);
    }
}

uint64_t LogFileReader::GetLogstoreKey() const {
    return mEOOption ? mEOOption->fbKey : mLogstoreKey;
}

bool LogFileReader::CheckDevInode() {
    fsutil::PathStat statBuf;
    if (mLogFileOp.Stat(statBuf) != 0) {
        if (errno == ENOENT) {
            LOG_WARNING(sLogger, ("file deleted ", "unknow error")("path", mLogPath)("fd", mLogFileOp.GetFd()));
        } else {
            LOG_WARNING(sLogger,
                        ("get file info error, ", strerror(errno))("path", mLogPath)("fd", mLogFileOp.GetFd()));
        }
        return false;
    } else {
        DevInode devInode = statBuf.GetDevInode();
        return devInode == mDevInode;
    }
}

bool LogFileReader::CheckFileSignatureAndOffset(int64_t& fileSize) {
    mLastEventTime = time(NULL);
    char firstLine[1025];
    int nbytes = mLogFileOp.Pread(firstLine, 1, 1024, 0);
    if (nbytes < 0) {
        LOG_ERROR(sLogger, ("fail to read file", mLogPath)("nbytes", nbytes));
        return false;
    }
    firstLine[nbytes] = '\0';
    int64_t endSize = mLogFileOp.GetFileSize();
    if (endSize < 0) {
        int lastErrNo = errno;
        mLogFileOp.Close();
        GloablFileDescriptorManager::GetInstance()->OnFileClose(this);
        bool reopenFlag = UpdateFilePtr();
        endSize = mLogFileOp.GetFileSize();
        LOG_WARNING(
            sLogger,
            ("tell error", mLogPath)("inode", mDevInode.inode)("error", strerror(lastErrNo))("reopen", reopenFlag));
        LogtailAlarm::GetInstance()->SendAlarm(OPEN_LOGFILE_FAIL_ALARM,
                                               string("tell error because of ") + strerror(lastErrNo) + " file path: "
                                                   + mLogPath + ", inode : " + ToString(mDevInode.inode),
                                               mProjectName,
                                               mCategory,
                                               mRegion);
        if (endSize < 0) {
            return false;
        }
    }

    fileSize = endSize;
    mLastFileSize = endSize;
    bool sigCheckRst = CheckAndUpdateSignature(string(firstLine), mLastFileSignatureHash, mLastFileSignatureSize);
    if (!sigCheckRst) {
        LOG_INFO(sLogger, ("Check file truncate by signature, read from begin", mLogPath));
        mLastFilePos = 0;
        if (mEOOption) {
            updatePrimaryCheckpointSignature();
        }
        return false;
    } else if (mEOOption && mEOOption->primaryCheckpoint.sig_size() != mLastFileSignatureSize) {
        updatePrimaryCheckpointSignature();
    }

    if (endSize < mLastFilePos) {
        LOG_INFO(sLogger,
                 ("File signature is same but size decrease, read from now fileSize",
                  mLogPath)(ToString(endSize), ToString(mLastFilePos))(GetProjectName(), GetCategory()));

        LogtailAlarm::GetInstance()->SendAlarm(LOG_TRUNCATE_ALARM,
                                               mLogPath
                                                   + " signature is same but size decrease, read from now fileSize "
                                                   + ToString(endSize) + " last read pos " + ToString(mLastFilePos),
                                               mProjectName,
                                               mCategory,
                                               mRegion);

        mLastFilePos = endSize;
        // when we use truncate_pos_skip_bytes, if truncate stop and log start to append, logtail will drop less data or collect more data
        // this just work around for ant's demand
        if (INT32_FLAG(truncate_pos_skip_bytes) > 0 && mLastFilePos > (INT32_FLAG(truncate_pos_skip_bytes) + 1024)) {
            mLastFilePos -= INT32_FLAG(truncate_pos_skip_bytes);
            // after adjust mLastFilePos, we should fix last pos to assure that each log is complete
            FixLastFilePos(mLogFileOp, endSize);
        }
        return true;
    }
    return true;
}

LogFileReaderPtrArray* LogFileReader::GetReaderArray() {
    return mReaderArray;
}

void LogFileReader::SetReaderArray(LogFileReaderPtrArray* readerArray) {
    mReaderArray = readerArray;
}

void LogFileReader::ResetTopic(const std::string& topicFormat) {
    const std::string lowerConfig = ToLowerCaseString(topicFormat);
    if (lowerConfig == "none" || lowerConfig == "default" || lowerConfig == "global_topic"
        || lowerConfig == "group_topic" || lowerConfig == "customized") {
        return;
    } else {
        // only reset file's topic
        mTopicName = GetTopicName(topicFormat, mLogPath);
    }
}

void LogFileReader::SetReadBufferSize(int32_t bufSize) {
    if (bufSize < 1024 * 10 || bufSize > 1024 * 1024 * 1024) {
        LOG_ERROR(sLogger, ("invalid read buffer size", bufSize));
        return;
    }
    LOG_INFO(sLogger, ("set max read buffer size", bufSize));
    BUFFER_SIZE = bufSize;
}

vector<int32_t> LogFileReader::LogSplit(char* buffer, int32_t size, int32_t& lineFeed) {
    vector<int32_t> index;
    int begIndex = 0;
    int endIndex = 0;
    lineFeed = 0;
    string exception;
    while (endIndex < size) {
        if (buffer[endIndex] == '\n') {
            lineFeed++;
            buffer[endIndex] = '\0';
            exception.clear();
            if (mLogBeginRegPtr == NULL || BoostRegexMatch(buffer + begIndex, *mLogBeginRegPtr, exception)) {
                index.push_back(begIndex);
                if (begIndex > 0) {
                    buffer[begIndex - 1] = '\0';
                }
            } else if (!exception.empty()) {
                if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                    if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                        LOG_ERROR(sLogger,
                                  ("regex_match in LogSplit fail, exception",
                                   exception)("project", mProjectName)("logstore", mCategory)("file", mLogPath));
                    }
                    LogtailAlarm::GetInstance()->SendAlarm(REGEX_MATCH_ALARM,
                                                           "regex_match in LogSplit fail:" + exception,
                                                           mProjectName,
                                                           mCategory,
                                                           mRegion);
                }
            }
            buffer[endIndex] = '\n';
            begIndex = endIndex + 1;
        }
        endIndex++;
    }
    lineFeed++;
    exception.clear();
    if (mLogBeginRegPtr == NULL || BoostRegexMatch(buffer + begIndex, *mLogBeginRegPtr, exception)) {
        // the last second log should be terminated
        if (begIndex > 0) {
            buffer[begIndex - 1] = '\0';
        }
        // save the last log
        index.push_back(begIndex);
    } else if (!exception.empty()) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_ERROR(sLogger,
                          ("regex_match in LogSplit fail, exception",
                           exception)("project", mProjectName)("logstore", mCategory)("file", mLogPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(
                REGEX_MATCH_ALARM, "regex_match in LogSplit fail:" + exception, mProjectName, mCategory, mRegion);
        }
    }
    return index;
}

bool LogFileReader::ParseLogTime(const char* buffer,
                                 const boost::regex* reg,
                                 time_t& logTime,
                                 const std::string& timeFormat,
                                 const std::string& region,
                                 const std::string& project,
                                 const std::string& logStore,
                                 const std::string& logPath) {
    std::string exception;
    boost::match_results<const char*> what;
    if (reg != NULL && BoostRegexSearch(buffer, *reg, exception, what, boost::match_default)) {
        if (!what.empty()) {
            std::string timeStr(what[0].str());
            // convert log time
            struct tm t;
            memset(&t, 0, sizeof(t));
            if (strptime(timeStr.c_str(), timeFormat.c_str(), &t) == NULL) {
                LOG_ERROR(sLogger,
                          ("convert time failed, time str", timeStr)("time format", timeFormat)("project", project)(
                              "logstore", logStore)("file", logPath));
                return false;
            }

            t.tm_isdst = -1;
            logTime = mktime(&t);
            return true;
        }
    }
    if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
        if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_ERROR(sLogger,
                      ("parse regex log fail, exception",
                       exception)("buffer", buffer)("project", project)("logstore", logStore)("file", logPath));
        }
        LogtailAlarm::GetInstance()->SendAlarm(
            REGEX_MATCH_ALARM, "parse regex log fail:" + exception, project, logStore, region);
    }
    return false;
}

bool LogFileReader::GetLogTimeByOffset(const char* buffer,
                                       int32_t pos,
                                       time_t& logTime,
                                       const std::string& timeFormat,
                                       const std::string& region,
                                       const std::string& project,
                                       const std::string& logStore,
                                       const std::string& logPath) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    if (strptime(buffer + pos, timeFormat.c_str(), &t) == NULL) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("get time by offset fail, region", region)("project", project)("logstore",
                                                                                            logStore)("file", logPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(
                PARSE_TIME_FAIL_ALARM, "errorlog:" + string(buffer), project, logStore, region);
        }
        return false;
    }
    t.tm_isdst = -1;
    logTime = mktime(&t);
    return true;
}

// Only get the currently written log file, it will choose the last modified file to read. There are several condition to choose the lastmodify file:
// 1. if the last read file don't exist
// 2. if the file's first 100 bytes(file signature) is not same with the last read file's signature, which meaning the log file has be rolled
//
// when a new file is choosen, it will set the read position
// 1. if the time in the file's first line >= the last read log time , then set the file read position to 0 (which mean the file is new created)
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
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3_Line_1\n" -> "MultiLineLog_1\nMultiLineLog_2\MultiLineLog_3_Line_1\0"  **this is not expected !**
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3_Line_1\nxxx" -> "MultiLineLog_1\nMultiLineLog_2\0"
 * "MultiLineLog_1\nMultiLineLog_2\nMultiLineLog_3\nxxx" -> "MultiLineLog_1\nMultiLineLog_2\0"
 *
 * 2. for singleline log, "xxx" mean a string without '\n'
 * "SingleLineLog_1\nSingleLineLog_2\nSingleLineLog_3\n" -> "SingleLineLog_1\nSingleLineLog_2\nSingleLineLog_3\0"
 * "SingleLineLog_1\nSingleLineLog_2\nxxx" -> "SingleLineLog_1\nSingleLineLog_2\0"
 */
bool LogFileReader::GetRawData(
    char*& bufferptr, size_t* size, int64_t fileSize, FileInfo*& fileInfo, TruncateInfo*& truncateInfo) {
    *size = 0;

    // Truncate, return false to indicate no more data.
    if (fileSize == mLastFilePos) {
        return false;
    }

    bool moreData = false;
    if (mFileEncoding == ENCODING_GBK)
        ReadGBK(bufferptr, size, fileSize, moreData, truncateInfo);
    else
        ReadUTF8(bufferptr, size, fileSize, moreData, truncateInfo);

    int64_t delta = fileSize - mLastFilePos;
    if (delta > mReadDelayAlarmBytes && bufferptr != NULL) {
        int32_t curTime = time(NULL);
        if (mReadDelayTime == 0)
            mReadDelayTime = curTime;
        else if (curTime - mReadDelayTime >= INT32_FLAG(read_delay_alarm_duration)) {
            mReadDelayTime = curTime;
            LOG_WARNING(sLogger,
                        ("read log delay", mLogPath)("fall behind bytes", delta)("file size", fileSize)("read pos",
                                                                                                        mLastFilePos));
            LogtailAlarm::GetInstance()->SendAlarm(
                READ_LOG_DELAY_ALARM,
                string("fall behind ") + ToString(delta) + " bytes, file size:" + ToString(fileSize)
                    + ", now position:" + ToString(mLastFilePos) + ", path:" + mLogPath
                    + ", now read log content:" + std::string(bufferptr, *size < 256 ? *size : 256),
                mProjectName,
                mCategory,
                mRegion);
        }
    } else
        mReadDelayTime = 0;

    // if delta size > mReadDelaySkipBytes, force set file pos and send alarm
    if (mReadDelaySkipBytes > 0 && delta > mReadDelaySkipBytes) {
        LOG_WARNING(sLogger,
                    ("read log delay and force set file pos to file size", mLogPath)("fall behind bytes", delta)(
                        "skip bytes config", mReadDelaySkipBytes)("file size", fileSize)("read pos", mLastFilePos));
        LogtailAlarm::GetInstance()->SendAlarm(
            READ_LOG_DELAY_ALARM,
            string("force set file pos to file size, fall behind ") + ToString(delta)
                + " bytes, file size:" + ToString(fileSize) + ", now position:" + ToString(mLastFilePos)
                + ", path:" + mLogPath + ", now read log content:" + std::string(bufferptr, *size < 256 ? *size : 256),
            mProjectName,
            mCategory,
            mRegion);
        mLastFilePos = fileSize;
        mLastReadPos = mLastFilePos;
    }

    if (mMarkOffsetFlag && *size > 0) {
        fileInfo = new FileInfo(mLogFileOp.GetFd(), mDevInode);
        fileInfo->filename = mIsFuseMode ? mFuseTrimedFilename : mLogPath;
        fileInfo->offset = mLastFilePos - (int64_t)(*size);
        fileInfo->len = (int64_t)(*size);
        fileInfo->filePos = mLastFilePos;
        fileInfo->readPos = mLastReadPos;
        fileInfo->fileSize = fileSize;
    }

    if (mIsFuseMode && *size > 0)
        UlogfsHandler::GetInstance()->Sparse(fileInfo);

    if (mContainerStopped) {
        int32_t curTime = time(NULL);
        if (curTime - mContainerStoppedTime >= INT32_FLAG(logtail_alarm_interval)
            && curTime - mReadStoppedContainerAlarmTime >= INT32_FLAG(logtail_alarm_interval)) {
            mReadStoppedContainerAlarmTime = curTime;
            LOG_WARNING(sLogger,
                        ("read stopped container file", mLogPath)("stopped time", mContainerStoppedTime)(
                            "file size", fileSize)("read pos", mLastFilePos));
            LogtailAlarm::GetInstance()->SendAlarm(
                READ_STOPPED_CONTAINER_ALARM,
                string("path: ") + mLogPath + ", stopped time:" + ToString(mContainerStoppedTime)
                    + ", file size:" + ToString(fileSize) + ", now position:" + ToString(mLastFilePos),
                mProjectName,
                mCategory,
                mRegion);
        }
    }

    return moreData;
}

size_t LogFileReader::getNextReadSize(int64_t fileEnd, bool& fromCpt) {
    size_t readSize = static_cast<size_t>(fileEnd - mLastFilePos);
    bool allowMoreBufferSize = false;
    fromCpt = false;
    if (mEOOption && mEOOption->selectedCheckpoint->IsComplete()) {
        fromCpt = true;
        allowMoreBufferSize = true;
        auto& checkpoint = mEOOption->selectedCheckpoint->data;
        readSize = checkpoint.read_length();
        LOG_INFO(sLogger, ("read specified length", readSize)("offset", mLastFilePos));
    }
    if (readSize > BUFFER_SIZE && !allowMoreBufferSize) {
        readSize = BUFFER_SIZE;
    }
    return readSize;
}

void LogFileReader::setExactlyOnceCheckpointAfterRead(size_t readSize) {
    if (!mEOOption || readSize == 0) {
        return;
    }

    auto& cpt = mEOOption->selectedCheckpoint->data;
    cpt.set_read_offset(mLastFilePos);
    cpt.set_read_length(readSize);
}

void LogFileReader::ReadUTF8(char*& bufferptr, size_t* size, int64_t end, bool& moreData, TruncateInfo*& truncateInfo) {
    bool fromCpt = false;
    size_t READ_BYTE = getNextReadSize(end, fromCpt);
    bufferptr = new char[READ_BYTE + 1];
    bufferptr[READ_BYTE] = '\0';
    size_t nbytes = ReadFile(mLogFileOp, bufferptr, READ_BYTE, mLastFilePos, &truncateInfo);
    mLastReadPos = mLastFilePos + nbytes;
    LOG_DEBUG(sLogger, ("read bytes", nbytes)("last read pos", mLastReadPos));
    moreData = (nbytes == BUFFER_SIZE);
    bool adjustFlag = false;
    while (nbytes > 0 && bufferptr[nbytes - 1] != '\n') {
        nbytes--;
        adjustFlag = true;
    }
    if ((nbytes > 0 && (adjustFlag || moreData) && mLogBeginRegPtr) || mLogType == JSON_LOG) {
        int32_t rollbackLineFeedCount;
        nbytes = LastMatchedLine(bufferptr, nbytes, rollbackLineFeedCount);
    }

    if (moreData && nbytes == 0) {
        nbytes = READ_BYTE;
    }
    if (nbytes == 0)
        bufferptr[0] = '\0';
    else
        bufferptr[nbytes - 1] = '\0';

    if (!moreData && fromCpt && mLastReadPos < end) {
        moreData = true;
    }
    *size = nbytes;
    setExactlyOnceCheckpointAfterRead(*size);
    mLastFilePos += nbytes;

    LOG_DEBUG(sLogger, ("read size", *size)("last file pos", mLastFilePos));
}

void LogFileReader::ReadGBK(char*& bufferptr, size_t* size, int64_t end, bool& moreData, TruncateInfo*& truncateInfo) {
    bool fromCpt = false;
    size_t READ_BYTE = getNextReadSize(end, fromCpt);
    char* gbkBuffer = new char[READ_BYTE + 1];
    size_t readCharCount = ReadFile(mLogFileOp, gbkBuffer, READ_BYTE, mLastFilePos, &truncateInfo);
    mLastReadPos = mLastFilePos + readCharCount;
    size_t originReadCount = readCharCount;
    moreData = (readCharCount == BUFFER_SIZE);
    bool adjustFlag = false;
    while (readCharCount > 0 && gbkBuffer[readCharCount - 1] != '\n') {
        readCharCount--;
        adjustFlag = true;
    }

    if (readCharCount == 0) {
        if (moreData)
            readCharCount = READ_BYTE;
        else {
            delete[] gbkBuffer;
            *size = 0;
            return;
        }
    }
    gbkBuffer[readCharCount] = '\0';

    vector<size_t> lineFeedPos;
    for (size_t idx = 0; idx < readCharCount - 1; ++idx) {
        if (gbkBuffer[idx] == '\n')
            lineFeedPos.push_back(idx);
    }
    lineFeedPos.push_back(readCharCount - 1);

    size_t srcLength = readCharCount;
    size_t desLength = 0;
    bufferptr = NULL;
    EncodingConverter::GetInstance()->ConvertGbk2Utf8(gbkBuffer, &srcLength, bufferptr, &desLength, lineFeedPos);
    size_t resultCharCount = desLength;

    delete[] gbkBuffer;
    if (resultCharCount == 0) {
        *size = 0;
        mLastFilePos += readCharCount;
        return;
    }
    int32_t rollbackLineFeedCount = 0;
    if (((adjustFlag || moreData) && mLogBeginRegPtr) || mLogType == JSON_LOG) {
        int32_t bakResultCharCount = resultCharCount;
        resultCharCount = LastMatchedLine(bufferptr, resultCharCount, rollbackLineFeedCount);
        if (resultCharCount == 0) {
            resultCharCount = bakResultCharCount;
            rollbackLineFeedCount = 0;
        }
    }

    int32_t lineFeedCount = lineFeedPos.size();
    if (rollbackLineFeedCount > 0 && lineFeedCount >= (1 + rollbackLineFeedCount))
        readCharCount -= lineFeedPos[lineFeedCount - 1] - lineFeedPos[lineFeedCount - 1 - rollbackLineFeedCount];

    bufferptr[resultCharCount - 1] = '\0';
    if (!moreData && fromCpt && mLastReadPos < end) {
        moreData = true;
    }
    *size = resultCharCount;
    setExactlyOnceCheckpointAfterRead(*size);
    mLastFilePos += readCharCount;
    LOG_DEBUG(sLogger,
              ("read gbk buffer, offset", mLastFilePos)("origin read", originReadCount)("at last read", readCharCount));
}

size_t
LogFileReader::ReadFile(LogFileOperator& op, void* buf, size_t size, int64_t& offset, TruncateInfo** truncateInfo) {
    if (buf == NULL || size == 0 || op.IsOpen() == false) {
        LOG_WARNING(sLogger, ("invalid param", ""));
        return 0;
    }

    int nbytes = 0;
    if (mIsFuseMode) {
        int64_t oriOffset = offset;
        nbytes = op.SkipHoleRead(buf, 1, size, &offset);
        if (nbytes < 0) {
            LOG_ERROR(sLogger,
                      ("SkipHoleRead fail to read log file", mLogPath)("mLastFilePos",
                                                                       mLastFilePos)("size", size)("offset", offset));
            return 0;
        }
        if (oriOffset != offset && truncateInfo != NULL) {
            *truncateInfo = new TruncateInfo(oriOffset, offset);
            LOG_INFO(sLogger,
                     ("read fuse file with a hole, size",
                      offset - oriOffset)("filename", mLogPath)("dev", mDevInode.dev)("inode", mDevInode.inode));
            LogtailAlarm::GetInstance()->SendAlarm(
                FUSE_FILE_TRUNCATE_ALARM,
                string("read fuse file with a hole, size: ") + ToString(offset - oriOffset) + " filename: " + mLogPath
                    + " dev: " + ToString(mDevInode.dev) + " inode: " + ToString(mDevInode.inode),
                mProjectName,
                mCategory,
                mRegion);
        }
    } else {
        nbytes = op.Pread(buf, 1, size, offset);
        if (nbytes < 0) {
            LOG_ERROR(sLogger,
                      ("Pread fail to read log file", mLogPath)("mLastFilePos", mLastFilePos)("size", size)("offset",
                                                                                                            offset));
            return 0;
        }
    }

    *((char*)buf + nbytes) = '\0';
    return nbytes;
}

LogFileReader::FileCompareResult LogFileReader::CompareToFile(const string& filePath) {
    LogFileOperator logFileOp;
    logFileOp.Open(filePath.c_str(), mIsFuseMode);
    if (logFileOp.IsOpen() == false) {
        return FileCompareResult_Error;
    }

    fsutil::PathStat buf;
    if (0 == logFileOp.Stat(buf)) {
        auto devInode = buf.GetDevInode();
        if (devInode != mDevInode) {
            logFileOp.Close();
            return FileCompareResult_DevInodeChange;
        }

        char sigStr[1025];
        int readSize = logFileOp.Pread(sigStr, 1, 1024, 0);
        logFileOp.Close();
        if (readSize < 0)
            return FileCompareResult_Error;
        sigStr[readSize] = '\0';
        uint64_t sigHash = mLastFileSignatureHash;
        uint32_t sigSize = mLastFileSignatureSize;
        bool sigSameRst = CheckAndUpdateSignature(string(sigStr), sigHash, sigSize);
        if (!sigSameRst) {
            return FileCompareResult_SigChange;
        }
        if ((int64_t)buf.GetFileSize() == mLastFilePos) {
            return FileCompareResult_SigSameSizeSame;
        } else {
            return FileCompareResult_SigSameSizeChange;
        }
    }
    logFileOp.Close();
    return FileCompareResult_Error;
}

int32_t LogFileReader::LastMatchedLine(char* buffer, int32_t size, int32_t& rollbackLineFeedCount) {
    int endPs = size - 1; // buffer[size] = 0 , buffer[size-1] = '\n'
    int begPs = size - 2;
    string exception;
    rollbackLineFeedCount = 0;
    while (begPs >= 0) {
        if (buffer[begPs] == '\n') {
            rollbackLineFeedCount++;
            char temp = buffer[endPs];
            buffer[endPs] = '\0';
            // ignore regex match fail, no need log here
            if (BoostRegexMatch(buffer + begPs + 1, *mLogBeginRegPtr, exception)) {
                buffer[begPs + 1] = '\0';
                return begPs + 1;
            }
            buffer[endPs] = temp;
            endPs = begPs;
        }
        begPs--;
    }
    return 0;
}

LogFileReader::~LogFileReader() {
    if (mLogBeginRegPtr != NULL) {
        delete mLogBeginRegPtr;
        mLogBeginRegPtr = NULL;
    }
    LOG_DEBUG(sLogger, ("Delete LogFileReader ", mLogPath));
    CloseFilePtr();

    // Mark GC so that corresponding resources can be released.
    // For config update, reader will be recreated, which will retrieve these
    //  resources back, then their GC flag will be removed.
    if (mEOOption) {
        static auto sQueueM = QueueManager::GetInstance();
        sQueueM->MarkGC(mProjectName,
                        mEOOption->primaryCheckpointKey + mEOOption->rangeCheckpointPtrs[0]->data.hash_key());

        static auto sCptM = CheckpointManagerV2::GetInstance();
        sCptM->MarkGC(mEOOption->primaryCheckpointKey);
    }
}

#ifdef APSARA_UNIT_TEST_MAIN
void LogFileReader::UpdateReaderManual() {
    if (mLogFileOp.IsOpen()) {
        mLogFileOp.Close();
    }
    mLogFileOp.Open(mLogPath.c_str(), mIsFuseMode);
    mDevInode = GetFileDevInode(mLogPath);
}
#endif

CommonRegLogFileReader::CommonRegLogFileReader(const std::string& projectName,
                                               const std::string& category,
                                               const std::string& logPathDir,
                                               const std::string& logPathFile,
                                               int32_t tailLimit,
                                               const std::string& timeFormat,
                                               const std::string& topicFormat,
                                               const std::string& groupTopic /* = "" */,
                                               FileEncoding fileEncoding /* = ENCODING_UTF8 */,
                                               bool discardUnmatch /* = true */,
                                               bool dockerFileFlag /* = true */)
    : LogFileReader(projectName,
                    category,
                    logPathDir,
                    logPathFile,
                    tailLimit,
                    topicFormat,
                    groupTopic,
                    fileEncoding,
                    discardUnmatch,
                    dockerFileFlag) {
    mLogType = REGEX_LOG;
    mTimeFormat = timeFormat;
    mTimeKey = "time";
}

void CommonRegLogFileReader::SetTimeKey(const std::string& timeKey) {
    if (!timeKey.empty()) {
        mTimeKey = timeKey;
    }
}

bool CommonRegLogFileReader::AddUserDefinedFormat(const string& regStr, const string& keys) {
    vector<string> keyParts = StringSpliter(keys, ",");
    boost::regex reg(regStr);
    bool isWholeLineMode = regStr == "(.*)";
    mUserDefinedFormat.push_back(UserDefinedFormat(reg, keyParts, isWholeLineMode));
    int32_t index = -1;
    for (size_t i = 0; i < keyParts.size(); i++) {
        if (ToLowerCaseString(keyParts[i]) == mTimeKey) {
            index = i;
            break;
        }
    }
    mTimeIndex.push_back(index);
    return true;
}

bool CommonRegLogFileReader::ParseLogLine(const char* buffer,
                                          LogGroup& logGroup,
                                          ParseLogError& error,
                                          time_t& lastLogLineTime,
                                          std::string& lastLogTimeStr,
                                          uint32_t& logGroupSize) {
    if (logGroup.logs_size() == 0) {
        logGroup.set_category(mCategory);
        logGroup.set_topic(mTopicName);
    }

    for (uint32_t i = 0; i < mUserDefinedFormat.size(); ++i) {
        const UserDefinedFormat& format = mUserDefinedFormat[i];
        bool res = true;
        if (mTimeIndex[i] >= 0 && !mTimeFormat.empty()) {
            res = LogParser::RegexLogLineParser(buffer,
                                                format.mReg,
                                                logGroup,
                                                mDiscardUnmatch,
                                                format.mKeys,
                                                mCategory,
                                                mTimeFormat.c_str(),
                                                mPreciseTimestampConfig,
                                                mTimeIndex[i],
                                                lastLogTimeStr,
                                                lastLogLineTime,
                                                mSpecifiedYear,
                                                mProjectName,
                                                mRegion,
                                                mLogPath,
                                                error,
                                                logGroupSize);
        } else {
            // if "time" field not exist in user config or timeformat empty, set current system time for logs
            if (format.mIsWholeLineMode) {
                res = LogParser::WholeLineModeParser(buffer,
                                                     logGroup,
                                                     format.mKeys.empty() ? DEFAULT_CONTENT_KEY : format.mKeys[0],
                                                     time(NULL),
                                                     logGroupSize);
            } else {
                res = LogParser::RegexLogLineParser(buffer,
                                                    format.mReg,
                                                    logGroup,
                                                    mDiscardUnmatch,
                                                    format.mKeys,
                                                    mCategory,
                                                    time(NULL),
                                                    mProjectName,
                                                    mRegion,
                                                    mLogPath,
                                                    error,
                                                    logGroupSize);
            }
        }
        if (res) {
            return true;
        }
    }

    return false;
}

ApsaraLogFileReader::ApsaraLogFileReader(const std::string& projectName,
                                         const std::string& category,
                                         const std::string& logPathDir,
                                         const std::string& logPathFile,
                                         int32_t tailLimit,
                                         const std::string topicFormat,
                                         const std::string& groupTopic,
                                         FileEncoding fileEncoding,
                                         bool discardUnmatch,
                                         bool dockerFileFlag)
    : LogFileReader(projectName, category, logPathDir, logPathFile, tailLimit, discardUnmatch, dockerFileFlag) {
    mLogType = APSARA_LOG;
    const std::string lowerConfig = ToLowerCaseString(topicFormat);
    if (lowerConfig == "none" || lowerConfig == "customized") {
        mTopicName = "";
    } else if (lowerConfig == "default") {
        size_t pos_dot = mLogPath.rfind("."); // the "." must be founded
        size_t pos = mLogPath.find("@");
        if (pos != std::string::npos) {
            size_t pos_slash = mLogPath.find(PATH_SEPARATOR, pos);
            if (pos_slash != std::string::npos) {
                mTopicName = mLogPath.substr(0, pos) + mLogPath.substr(pos_slash, pos_dot - pos_slash);
            }
        }
        if (mTopicName.empty()) {
            mTopicName = mLogPath.substr(0, pos_dot);
        }
        std::string lowTopic = ToLowerCaseString(mTopicName);
        std::string logSuffix = ".log";

        size_t suffixPos = lowTopic.rfind(logSuffix);
        if (suffixPos == lowTopic.size() - logSuffix.size()) {
            mTopicName = mTopicName.substr(0, suffixPos);
        }
    } else if (lowerConfig == "global_topic") {
        static LogtailGlobalPara* sGlobalPara = LogtailGlobalPara::Instance();
        mTopicName = sGlobalPara->GetTopic();
    } else if (lowerConfig == "group_topic")
        mTopicName = groupTopic;
    else
        mTopicName = GetTopicName(topicFormat, mLogPath);
    mFileEncoding = fileEncoding;
}

bool ApsaraLogFileReader::ParseLogLine(const char* buffer,
                                       sls_logs::LogGroup& logGroup,
                                       ParseLogError& error,
                                       time_t& lastLogLineTime,
                                       std::string& lastLogTimeStr,
                                       uint32_t& logGroupSize) {
    if (logGroup.logs_size() == 0) {
        logGroup.set_category(mCategory);
        logGroup.set_topic(mTopicName);
    }

    return LogParser::ApsaraEasyReadLogLineParser(buffer,
                                                  logGroup,
                                                  mDiscardUnmatch,
                                                  lastLogTimeStr,
                                                  lastLogLineTime,
                                                  mProjectName,
                                                  mCategory,
                                                  mRegion,
                                                  mLogPath,
                                                  error,
                                                  logGroupSize);
}

} // namespace logtail
