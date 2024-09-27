// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/flusher/sls/DiskBufferWriter.h"

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/CompressTools.h"
#include "common/ErrorUtil.h"
#include "common/FileEncryption.h"
#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "common/StringTools.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "plugin/flusher/sls/SLSClientManager.h"
#include "protobuf/sls/sls_logs.pb.h"
#include "logger/Logger.h"
#include "monitor/LogtailAlarm.h"
#include "provider/Provider.h"
#include "pipeline/queue/QueueKeyManager.h"
#include "pipeline/queue/SLSSenderQueueItem.h"
#include "sdk/Exception.h"
#include "pipeline/limiter/RateLimiter.h"
#include "sls_control/SLSControl.h"

DEFINE_FLAG_INT32(write_secondary_wait_timeout, "interval of dump seconary buffer from memory to file, seconds", 2);
DEFINE_FLAG_INT32(buffer_file_alive_interval, "the max alive time of a bufferfile, 5 minutes", 300);
DEFINE_FLAG_INT32(log_expire_time, "log expire time", 24 * 3600);
DEFINE_FLAG_INT32(quota_exceed_wait_interval, "when daemon buffer thread get quotaExceed error, sleep 5 seconds", 5);
DEFINE_FLAG_INT32(secondary_buffer_count_limit, "data ready for write buffer file", 20);
DEFINE_FLAG_INT32(send_retry_sleep_interval, "sleep microseconds when sync send fail, 50ms", 50000);
DEFINE_FLAG_INT32(buffer_check_period, "check logtail local storage buffer period", 60);

using namespace std;

namespace logtail {

const string DiskBufferWriter::BUFFER_FILE_NAME_PREFIX = "logtail_buffer_file_";
const int32_t DiskBufferWriter::BUFFER_META_BASE_SIZE = 65536;

void DiskBufferWriter::Init() {
    mBufferDivideTime = time(NULL);
    mCheckPeriod = INT32_FLAG(buffer_check_period);
    SetBufferFilePath(AppConfig::GetInstance()->GetBufferFilePath());

    mBufferSenderThreadRes = async(launch::async, &DiskBufferWriter::BufferSenderThread, this);
    mBufferWriterThreadRes = async(launch::async, &DiskBufferWriter::BufferWriterThread, this);
}

void DiskBufferWriter::Stop() {
    // stop buffer writer
    mIsFlush = true;

    // stop buffer sender
    {
        lock_guard<mutex> lock(mBufferSenderThreadRunningMux);
        mIsSendBufferThreadRunning = false;
    }
    mStopCV.notify_one();
    {
        future_status s = mBufferWriterThreadRes.wait_for(chrono::seconds(3));
        if (s == future_status::ready) {
            LOG_INFO(sLogger, ("disk buffer writer", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("disk buffer writer", "forced to stopped"));
        }
    }
    {
        future_status s = mBufferSenderThreadRes.wait_for(chrono::seconds(1));
        if (s == future_status::ready) {
            LOG_INFO(sLogger, ("disk buffer sender", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("disk buffer sender", "forced to stopped"));
        }
    }
}

bool DiskBufferWriter::PushToDiskBuffer(SenderQueueItem* item, uint32_t retryTimes) {
    auto slsItem = static_cast<SLSSenderQueueItem*>(item);

    uint32_t retry = 0;
    while (++retry < retryTimes) {
        if (Application::GetInstance()->IsExiting()
            || mQueue.Size() < static_cast<size_t>(INT32_FLAG(secondary_buffer_count_limit))) {
            if (slsItem->mExactlyOnceCheckpoint == nullptr) {
                // explicitly clone the data to avoid dataPtr be destructed by queue
                mQueue.Push(item->Clone());
            }
            return true;
        }
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    auto flusher = static_cast<const FlusherSLS*>(slsItem->mFlusher);
    LOG_WARNING(sLogger,
                ("failed to add sender queue item to disk buffer writer", "queue is full")("action", "discard data")(
                    "config-flusher-dst", QueueKeyManager::GetInstance()->GetName(item->mFlusher->GetQueueKey())));
    LogtailAlarm::GetInstance()->SendAlarm(
        DISCARD_DATA_ALARM,
        "failed to add sender queue item to disk buffer writer: queue is full\taction: discard data",
        flusher->mProject,
        slsItem->mLogstore,
        flusher->mRegion);
    return false;
}

void DiskBufferWriter::BufferWriterThread() {
    LOG_INFO(sLogger, ("disk buffer writer", "started"));
    vector<SenderQueueItem*> res;
    while (true) {
        if (!mQueue.WaitAndPopAll(res, INT32_FLAG(write_secondary_wait_timeout) * 1000)) {
            if (mIsFlush && mQueue.Empty()) {
                break;
            }
        }

        // update bufferDiveideTime to flush data; buffer file before bufferDiveideTime will be ready for read
        if (time(NULL) - mBufferDivideTime > INT32_FLAG(buffer_file_alive_interval)) {
            CreateNewFile();
        }

        if (!res.empty()) {
            for (auto itr = res.begin(); itr != res.end(); ++itr) {
                SendToBufferFile(*itr);
                delete *itr;
            }
            res.clear();
        }
    }
}

void DiskBufferWriter::BufferSenderThread() {
    LOG_INFO(sLogger, ("disk buffer sender", "started"));
    unique_lock<mutex> lock(mBufferSenderThreadRunningMux);
    while (mIsSendBufferThreadRunning) {
        if (!SLSClientManager::GetInstance()->HasNetworkAvailable()) {
            if (mStopCV.wait_for(
                    lock, chrono::seconds(mCheckPeriod), [this]() { return !mIsSendBufferThreadRunning; })) {
                break;
            }
            continue;
        }
        vector<string> filesToSend;
        if (!LoadFileToSend(mBufferDivideTime, filesToSend)) {
            if (mStopCV.wait_for(
                    lock, chrono::seconds(mCheckPeriod), [this]() { return !mIsSendBufferThreadRunning; })) {
                break;
            }
            continue;
        }
        // mIsSendingBuffer = true;
        int32_t fileToSendCount = int32_t(filesToSend.size());
        int32_t bufferFileNumValue = AppConfig::GetInstance()->GetNumOfBufferFile();
        for (int32_t i = (fileToSendCount > bufferFileNumValue ? fileToSendCount - bufferFileNumValue : 0);
             i < fileToSendCount && mIsSendBufferThreadRunning;
             ++i) {
            string fileName = GetBufferFilePath() + filesToSend[i];
            unordered_map<string, string> kvMap;
            if (FileEncryption::CheckHeader(fileName, kvMap)) {
                int32_t keyVersion = -1;
                if (kvMap.find(STRING_FLAG(file_encryption_field_key_version)) != kvMap.end()) {
                    try {
                        keyVersion = StringTo<int32_t>(kvMap[STRING_FLAG(file_encryption_field_key_version)]);
                    } catch (...) {
                        LOG_ERROR(sLogger,
                                  ("convert key_version to int32_t fail",
                                   kvMap[STRING_FLAG(file_encryption_field_key_version)]));
                        keyVersion = -1;
                    }
                }
                if (keyVersion >= 1 && keyVersion <= FileEncryption::GetInstance()->GetDefaultKeyVersion()) {
                    LOG_INFO(sLogger, ("check local encryption file", fileName)("key_version", keyVersion));
                    SendEncryptionBuffer(fileName, keyVersion);
                } else {
                    remove(fileName.c_str());
                    LOG_ERROR(sLogger,
                              ("invalid key_version in header",
                               kvMap[STRING_FLAG(file_encryption_field_key_version)])("delete bufffer file", fileName));
                    LogtailAlarm::GetInstance()->SendAlarm(
                        DISCARD_SECONDARY_ALARM, "key version in buffer file invalid, delete file: " + fileName);
                }
            } else {
                remove(fileName.c_str());
                LOG_WARNING(sLogger, ("check header of buffer file failed, delete file", fileName));
                LogtailAlarm::GetInstance()->SendAlarm(DISCARD_SECONDARY_ALARM,
                                                       "check header of buffer file failed, delete file: " + fileName);
            }
        }
        // mIsSendingBuffer = false;
        if (mStopCV.wait_for(lock, chrono::seconds(mCheckPeriod), [this]() { return !mIsSendBufferThreadRunning; })) {
            break;
        }
    }
}

void DiskBufferWriter::SetBufferFilePath(const std::string& bufferfilepath) {
    lock_guard<mutex> lock(mBufferFileLock);
    if (bufferfilepath == "") {
        mBufferFilePath = GetProcessExecutionDir();
    } else
        mBufferFilePath = bufferfilepath;

    if (mBufferFilePath[mBufferFilePath.size() - 1] != PATH_SEPARATOR[0])
        mBufferFilePath += PATH_SEPARATOR;
    mBufferFileName = "";
}

std::string DiskBufferWriter::GetBufferFilePath() {
    lock_guard<mutex> lock(mBufferFileLock);
    return mBufferFilePath;
}

void DiskBufferWriter::SetBufferFileName(const std::string& filename) {
    lock_guard<mutex> lock(mBufferFileLock);
    mBufferFileName = filename;
}

std::string DiskBufferWriter::GetBufferFileName() {
    lock_guard<mutex> lock(mBufferFileLock);
    return mBufferFileName;
}

bool DiskBufferWriter::LoadFileToSend(time_t timeLine, std::vector<std::string>& filesToSend) {
    string bufferFilePath = GetBufferFilePath();
    if (!CheckExistance(bufferFilePath)) {
        if (GetProcessExecutionDir().find(bufferFilePath) != 0) {
            LOG_WARNING(sLogger,
                        ("buffer file path not exist", bufferFilePath)("logtail will not recreate external path",
                                                                       "local secondary does not work"));
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                                   string("buffer file directory:") + bufferFilePath + " not exist");
            return false;
        }
        string errorMessage;
        if (!RebuildExecutionDir(AppConfig::GetInstance()->GetIlogtailConfigJson(), errorMessage)) {
            LOG_ERROR(sLogger, ("failed to rebuild buffer file path", bufferFilePath)("errorMessage", errorMessage));
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM, errorMessage);
            return false;
        } else
            LOG_INFO(sLogger, ("rebuild buffer file path success", bufferFilePath));
    }

    fsutil::Dir dir(bufferFilePath);
    if (!dir.Open()) {
        string errorStr = ErrnoToString(GetErrno());
        LOG_ERROR(sLogger, ("open dir error", bufferFilePath)("reason", errorStr));
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("open dir error,dir:") + bufferFilePath + ",error:" + errorStr);
        return false;
    }
    fsutil::Entry ent;
    while ((ent = dir.ReadNext())) {
        string filename = ent.Name();
        if (filename.find(BUFFER_FILE_NAME_PREFIX) == 0) {
            try {
                int32_t filetime = StringTo<int32_t>(filename.substr(BUFFER_FILE_NAME_PREFIX.size()));
                if (filetime < timeLine)
                    filesToSend.push_back(filename);
            } catch (...) {
                LOG_INFO(sLogger, ("can not get file time from file name", filename));
            }
        }
    }
    sort(filesToSend.begin(), filesToSend.end());
    return true;
}

bool DiskBufferWriter::ReadNextEncryption(int32_t& pos,
                                          const std::string& filename,
                                          std::string& encryption,
                                          EncryptionStateMeta& meta,
                                          bool& readResult,
                                          sls_logs::LogtailBufferMeta& bufferMeta) {
    bufferMeta.Clear();
    readResult = false;
    encryption.clear();
    int retryTimes = 0;

    FILE* fin = NULL;
    while (true) {
        retryTimes++;
        fin = FileReadOnlyOpen(filename.c_str(), "rb");
        if (fin)
            break;
        if (retryTimes >= 3) {
            string errorStr = ErrnoToString(GetErrno());
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                                   string("open file error:") + filename + ",error:" + errorStr);
            LOG_ERROR(sLogger, ("open file error", filename)("error", errorStr));
            return false;
        }
        usleep(5000);
    }
    fseek(fin, 0, SEEK_END);
    auto const currentSize = ftell(fin);
    if (currentSize == pos) {
        fclose(fin);
        return false;
    }
    fseek(fin, pos, SEEK_SET);
    auto nbytes = fread(static_cast<void*>(&meta), sizeof(char), sizeof(meta), fin);
    if (nbytes != sizeof(meta)) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("read encryption file meta error:") + filename
                                                   + ", error:" + errorStr + ", meta.mEncryptionSize:"
                                                   + ToString(meta.mEncryptionSize) + ", nbytes: " + ToString(nbytes)
                                                   + ", pos: " + ToString(pos) + ", ftell: " + ToString(currentSize));
        LOG_ERROR(sLogger,
                  ("read encryption file meta error",
                   filename)("error", errorStr)("nbytes", nbytes)("pos", pos)("ftell", currentSize));
        fclose(fin);
        return false;
    }

    bool pbMeta = false;
    int32_t encodedInfoSize = meta.mEncodedInfoSize;
    if (encodedInfoSize > BUFFER_META_BASE_SIZE) {
        encodedInfoSize -= BUFFER_META_BASE_SIZE;
        pbMeta = true;
    }

    if (meta.mEncryptionSize < 0 || encodedInfoSize < 0) {
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("meta of encryption file invalid:" + filename
                                                      + ", meta.mEncryptionSize:" + ToString(meta.mEncryptionSize)
                                                      + ", meta.mEncodedInfoSize:" + ToString(meta.mEncodedInfoSize)));
        LOG_ERROR(sLogger,
                  ("meta of encryption file invalid", filename)("meta.mEncryptionSize", meta.mEncryptionSize)(
                      "meta.mEncodedInfoSize", meta.mEncodedInfoSize));
        fclose(fin);
        return false;
    }

    pos += sizeof(meta) + encodedInfoSize + meta.mEncryptionSize;
    if ((time(NULL) - meta.mTimeStamp) > INT32_FLAG(log_expire_time) || meta.mHandled == 1) {
        fclose(fin);
        if (meta.mHandled != 1) {
            LOG_WARNING(sLogger, ("timeout buffer file, meta.mTimeStamp", meta.mTimeStamp));
            LogtailAlarm::GetInstance()->SendAlarm(DISCARD_SECONDARY_ALARM,
                                                   "buffer file timeout (1day), delete file: " + filename);
        }
        return true;
    }

    char* buffer = new char[encodedInfoSize + 1];
    nbytes = fread(buffer, sizeof(char), encodedInfoSize, fin);
    if (nbytes != static_cast<size_t>(encodedInfoSize)) {
        fclose(fin);
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("read projectname from file error:") + filename
                                                   + ", error:" + errorStr + ", meta.mEncodedInfoSize:"
                                                   + ToString(meta.mEncodedInfoSize) + ", nbytes:" + ToString(nbytes));
        LOG_ERROR(sLogger,
                  ("read encodedInfo from file error",
                   filename)("error", errorStr)("meta.mEncodedInfoSize", meta.mEncodedInfoSize)("nbytes", nbytes));
        delete[] buffer;
        return true;
    }
    string encodedInfo = string(buffer, encodedInfoSize);
    delete[] buffer;
    if (pbMeta) {
        if (!bufferMeta.ParseFromString(encodedInfo)) {
            fclose(fin);
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                                   string("parse buffer meta from file error:") + filename);
            LOG_ERROR(sLogger, ("parse buffer meta from file error", filename)("buffer meta", encodedInfo));
            bufferMeta.Clear();
            return true;
        }
    } else {
        bufferMeta.set_project(encodedInfo);
        bufferMeta.set_endpoint(FlusherSLS::GetDefaultRegion()); // new mode
        bufferMeta.set_aliuid("");
    }
    if (!bufferMeta.has_compresstype()) {
        bufferMeta.set_compresstype(sls_logs::SlsCompressType::SLS_CMP_LZ4);
    }
    if (!bufferMeta.has_telemetrytype()) {
        bufferMeta.set_telemetrytype(sls_logs::SLS_TELEMETRY_TYPE_LOGS);
    }

    buffer = new char[meta.mEncryptionSize + 1];
    nbytes = fread(buffer, sizeof(char), meta.mEncryptionSize, fin);
    if (nbytes != static_cast<size_t>(meta.mEncryptionSize)) {
        fclose(fin);
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("read encryption from file error:") + filename
                                                   + ",error:" + errorStr + ",meta.mEncryptionSize:"
                                                   + ToString(meta.mEncryptionSize) + ", nbytes:" + ToString(nbytes));
        LOG_ERROR(sLogger,
                  ("read encryption from file error",
                   filename)("error", errorStr)("meta.mEncryptionSize", meta.mEncryptionSize)("nbytes", nbytes));
        delete[] buffer;
        return true;
    }
    encryption = string(buffer, meta.mEncryptionSize);
    readResult = true;
    delete[] buffer;
    fclose(fin);
    return true;
}

void DiskBufferWriter::SendEncryptionBuffer(const std::string& filename, int32_t keyVersion) {
    string encryption;
    string logData;
    EncryptionStateMeta meta;
    bool readResult;
    bool writeBack = false;
    int32_t pos = INT32_FLAG(file_encryption_header_length);
    sls_logs::LogtailBufferMeta bufferMeta;
    int32_t discardCount = 0;
    while (ReadNextEncryption(pos, filename, encryption, meta, readResult, bufferMeta)) {
        logData.clear();
        bool sendResult = false;
        if (!readResult || bufferMeta.project().empty()) {
            if (meta.mHandled == 1)
                continue;
            sendResult = true;
            discardCount++;
        }
        if (!sendResult) {
            char* des = new char[meta.mLogDataSize];
            if (!FileEncryption::GetInstance()->Decrypt(
                    encryption.c_str(), meta.mEncryptionSize, des, meta.mLogDataSize, keyVersion)) {
                sendResult = true;
                discardCount++;
                LOG_ERROR(sLogger,
                          ("decrypt error, project_name",
                           bufferMeta.project())("key_version", keyVersion)("meta.mLogDataSize", meta.mLogDataSize));
                LogtailAlarm::GetInstance()->SendAlarm(ENCRYPT_DECRYPT_FAIL_ALARM,
                                                       string("decrypt error, project_name:" + bufferMeta.project()
                                                              + ", key_version:" + ToString(keyVersion)
                                                              + ", meta.mLogDataSize:" + ToString(meta.mLogDataSize)));
            } else {
                if (bufferMeta.has_logstore())
                    logData = string(des, meta.mLogDataSize);
                else {
                    // compatible to old buffer file (logGroup string), convert to LZ4 compressed
                    string logGroupStr = string(des, meta.mLogDataSize);
                    sls_logs::LogGroup logGroup;
                    if (!logGroup.ParseFromString(logGroupStr)) {
                        sendResult = true;
                        LOG_ERROR(sLogger,
                                  ("parse error from string to loggroup, projectName is", bufferMeta.project()));
                        discardCount++;
                        LogtailAlarm::GetInstance()->SendAlarm(
                            LOG_GROUP_PARSE_FAIL_ALARM,
                            string("projectName is:" + bufferMeta.project() + ", fileName is:" + filename));
                    } else if (!CompressLz4(logGroupStr, logData)) {
                        sendResult = true;
                        LOG_ERROR(sLogger, ("LZ4 compress loggroup fail, projectName is", bufferMeta.project()));
                        discardCount++;
                        LogtailAlarm::GetInstance()->SendAlarm(
                            SEND_COMPRESS_FAIL_ALARM,
                            string("projectName is:" + bufferMeta.project() + ", fileName is:" + filename));
                    } else {
                        bufferMeta.set_logstore(logGroup.category());
                        bufferMeta.set_datatype(int(RawDataType::EVENT_GROUP));
                        bufferMeta.set_rawsize(meta.mLogDataSize);
                        bufferMeta.set_compresstype(sls_logs::SLS_CMP_LZ4);
                        bufferMeta.set_telemetrytype(sls_logs::SLS_TELEMETRY_TYPE_LOGS);
                    }
                }
                if (!sendResult) {
                    string errorCode;
                    SendResult res = SendBufferFileData(bufferMeta, logData, errorCode);
                    if (res == SEND_OK)
                        sendResult = true;
                    else if (res == SEND_DISCARD_ERROR || res == SEND_UNAUTHORIZED) {
                        LogtailAlarm::GetInstance()->SendAlarm(SEND_DATA_FAIL_ALARM,
                                                               string("send buffer file fail, rawsize:")
                                                                   + ToString(bufferMeta.rawsize())
                                                                   + "errorCode: " + errorCode,
                                                               bufferMeta.project(),
                                                               bufferMeta.logstore(),
                                                               "");
                        sendResult = true;
                        discardCount++;
                    } else if (res == SEND_QUOTA_EXCEED && INT32_FLAG(quota_exceed_wait_interval) > 0)
                        sleep(INT32_FLAG(quota_exceed_wait_interval));
                }
            }
            delete[] des;
        }
        if (sendResult)
            meta.mHandled = 1;
        LOG_DEBUG(sLogger,
                  ("send LogGroup from local buffer file", filename)("rawsize", bufferMeta.rawsize())("sendResult",
                                                                                                      sendResult));
        WriteBackMeta(pos - meta.mEncryptionSize - sizeof(meta)
                          - (meta.mEncodedInfoSize > BUFFER_META_BASE_SIZE
                                 ? (meta.mEncodedInfoSize - BUFFER_META_BASE_SIZE)
                                 : meta.mEncodedInfoSize),
                      (char*)&meta,
                      sizeof(meta),
                      filename);
        if (!sendResult)
            writeBack = true;
    }
    if (!writeBack) {
        remove(filename.c_str());
        if (discardCount > 0) {
            LOG_ERROR(sLogger, ("send buffer file, discard LogGroup count", discardCount)("delete file", filename));
            LogtailAlarm::GetInstance()->SendAlarm(DISCARD_SECONDARY_ALARM,
                                                   "delete buffer file: " + filename + ", discard "
                                                       + ToString(discardCount) + " logGroups");
        } else
            LOG_INFO(sLogger, ("send buffer file success, delete buffer file", filename));
    }
}

// file is not really created when call CreateNewFile(), file created happened when SendToBufferFile() first called
bool DiskBufferWriter::CreateNewFile() {
    vector<string> filesToSend;
    int64_t currentTime = time(NULL);
    if (!LoadFileToSend(currentTime, filesToSend))
        return false;
    int32_t bufferFileNumValue = AppConfig::GetInstance()->GetNumOfBufferFile();
    for (int32_t i = 0; i < (int32_t)filesToSend.size() - bufferFileNumValue; ++i) {
        string fileName = GetBufferFilePath() + filesToSend[i];
        if (CheckExistance(fileName)) {
            remove(fileName.c_str());
            LOG_ERROR(sLogger,
                      ("buffer file count exceed limit",
                       "file created earlier will be cleaned, and new file will create for new log data")("delete file",
                                                                                                          fileName));
            LogtailAlarm::GetInstance()->SendAlarm(DISCARD_SECONDARY_ALARM,
                                                   "buffer file count exceed, delete file: " + fileName);
        }
    }
    mBufferDivideTime = currentTime;
    SetBufferFileName(GetBufferFilePath() + BUFFER_FILE_NAME_PREFIX + ToString(currentTime));
    return true;
}

bool DiskBufferWriter::WriteBackMeta(int32_t pos, const void* buf, int32_t length, const string& filename) {
    // TODO: Why not use fopen or fstream???
    // TODO: Make sure and merge them.
#if defined(__linux__)
    int fd = open(filename.c_str(), O_WRONLY);
    if (fd < 0) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("open secondary file for write meta fail:") + filename
                                                   + ",reason:" + errorStr);
        LOG_ERROR(sLogger, ("open file error", filename));
        return false;
    }
    lseek(fd, pos, SEEK_SET);
    if (write(fd, buf, length) < 0) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("write secondary file for write meta fail:") + filename
                                                   + ",reason:" + errorStr);
        LOG_ERROR(sLogger, ("can not write back meta", filename));
    }
    close(fd);
    return true;
#elif defined(_MSC_VER)
    FILE* f = FileWriteOnlyOpen(filename.c_str(), "wb");
    if (!f) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("open secondary file for write meta fail:") + filename
                                                   + ",reason:" + errorStr);
        LOG_ERROR(sLogger, ("open file error", filename));
        return false;
    }
    fseek(f, pos, SEEK_SET);
    auto nbytes = fwrite(buf, 1, length, f);
    if (nbytes != length) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("write secondary file for write meta fail:") + filename
                                                   + ",reason:" + errorStr);
        LOG_ERROR(sLogger, ("can not write back meta", filename));
    }
    fclose(f);
    return true;
#endif
}

string DiskBufferWriter::GetBufferFileHeader() {
    string reserve = STRING_FLAG(file_encryption_field_key_version) + STRING_FLAG(file_encryption_key_value_splitter)
        + ToString(FileEncryption::GetInstance()->GetDefaultKeyVersion());
    string nullHeader = string(
        (INT32_FLAG(file_encryption_header_length) - STRING_FLAG(file_encryption_magic_number).size() - reserve.size()),
        '\0');
    return (STRING_FLAG(file_encryption_magic_number) + reserve + nullHeader);
}

bool DiskBufferWriter::SendToBufferFile(SenderQueueItem* dataPtr) {
    auto data = static_cast<SLSSenderQueueItem*>(dataPtr);
    auto flusher = static_cast<const FlusherSLS*>(data->mFlusher);
    string bufferFileName = GetBufferFileName();
    if (bufferFileName.empty()) {
        CreateNewFile();
        bufferFileName = GetBufferFileName();
    }
    // if file not exist, create it new
    FILE* fout = FileAppendOpen(bufferFileName.c_str(), "ab");
    if (!fout) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("open file error:") + bufferFileName + ",error:" + errorStr);
        LOG_ERROR(sLogger, ("open buffer file error", bufferFileName));
        return false;
    }

    if (ftell(fout) == (streampos)0) {
        string header = GetBufferFileHeader();
        auto nbytes = fwrite(header.c_str(), 1, header.size(), fout);
        if (header.size() != nbytes) {
            string errorStr = ErrnoToString(GetErrno());
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                                   string("write file error:") + bufferFileName + ", error:" + errorStr
                                                       + ", nbytes:" + ToString(nbytes));
            LOG_ERROR(sLogger, ("error write encryption header", bufferFileName)("error", errorStr)("nbytes", nbytes));
            fclose(fout);
            return false;
        }
    }

    char* des;
    int32_t desLength;
    if (!FileEncryption::GetInstance()->Encrypt(data->mData.c_str(), data->mData.size(), des, desLength)) {
        fclose(fout);
        LOG_ERROR(sLogger, ("encrypt error, project_name", flusher->mProject));
        LogtailAlarm::GetInstance()->SendAlarm(ENCRYPT_DECRYPT_FAIL_ALARM,
                                               string("encrypt error, project_name:" + flusher->mProject));
        return false;
    }

    sls_logs::LogtailBufferMeta bufferMeta;
    bufferMeta.set_project(flusher->mProject);
    bufferMeta.set_endpoint(flusher->mRegion);
    bufferMeta.set_aliuid(flusher->mAliuid);
    bufferMeta.set_logstore(data->mLogstore);
    bufferMeta.set_datatype(int32_t(data->mType));
    bufferMeta.set_rawsize(data->mRawSize);
    bufferMeta.set_shardhashkey(data->mShardHashKey);
    bufferMeta.set_compresstype(ConvertCompressType(flusher->GetCompressType()));
    bufferMeta.set_telemetrytype(flusher->mTelemetryType);
    string encodedInfo;
    bufferMeta.SerializeToString(&encodedInfo);

    EncryptionStateMeta meta;
    int32_t encodedInfoSize = encodedInfo.size();
    meta.mEncodedInfoSize = encodedInfoSize + BUFFER_META_BASE_SIZE;
    meta.mLogDataSize = data->mData.size();
    meta.mTimeStamp = time(NULL);
    meta.mHandled = 0;
    meta.mRetryTime = 0;
    meta.mEncryptionSize = desLength;
    char* buffer = new char[sizeof(meta) + encodedInfoSize + meta.mEncryptionSize];
    memcpy(buffer, (char*)&meta, sizeof(meta));
    memcpy(buffer + sizeof(meta), encodedInfo.c_str(), encodedInfoSize);
    memcpy(buffer + sizeof(meta) + encodedInfoSize, des, desLength);
    delete[] des;
    const auto bytesToWrite = sizeof(meta) + encodedInfoSize + meta.mEncryptionSize;
    auto nbytes = fwrite(buffer, 1, bytesToWrite, fout);
    if (nbytes != bytesToWrite) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("write file error:") + bufferFileName + ", error:" + errorStr
                                                   + ", nbytes:" + ToString(nbytes));
        LOG_ERROR(
            sLogger,
            ("write meta of buffer file", "fail")("filename", bufferFileName)("errorStr", errorStr)("nbytes", nbytes));
        delete[] buffer;
        fclose(fout);
        return false;
    }
    delete[] buffer;
    if (ftell(fout) > AppConfig::GetInstance()->GetLocalFileSize())
        CreateNewFile();
    fclose(fout);
    LOG_DEBUG(sLogger, ("write buffer file", bufferFileName));
    return true;
}

SendResult DiskBufferWriter::SendBufferFileData(const sls_logs::LogtailBufferMeta& bufferMeta,
                                                const std::string& logData,
                                                std::string& errorCode) {
    RateLimiter::FlowControl(bufferMeta.rawsize(), mSendLastTime, mSendLastByte, false);
    string region = bufferMeta.endpoint();
    if (region.find("http://") == 0) // old buffer file which record the endpoint
        region = SLSClientManager::GetInstance()->GetRegionFromEndpoint(region);

    sdk::Client* sendClient = SLSClientManager::GetInstance()->GetClient(region, bufferMeta.aliuid());
    SendResult sendRes;
    const string& endpoint = sendClient->GetRawSlsHost();
    if (endpoint.empty())
        sendRes = SEND_NETWORK_ERROR;
    else {
        sendRes = SendToNetSync(sendClient, bufferMeta, logData, errorCode);
    }
    if (sendRes == SEND_NETWORK_ERROR) {
        SLSClientManager::GetInstance()->UpdateEndpointStatus(region, endpoint, false);
        SLSClientManager::GetInstance()->ResetClientEndpoint(bufferMeta.aliuid(), region, time(NULL));
        LOG_DEBUG(sLogger,
                  ("SendBufferFileData",
                   "SEND_NETWORK_ERROR")("region", region)("aliuid", bufferMeta.aliuid())("endpoint", endpoint));
    } else if (sendRes == SEND_UNAUTHORIZED) {
        int32_t lastUpdateTime;
        if (SLSControl::GetInstance()->SetSlsSendClientAuth(bufferMeta.aliuid(), false, sendClient, lastUpdateTime))
            sendRes = SendToNetSync(sendClient, bufferMeta, logData, errorCode);
    }
    return sendRes;
}

SendResult DiskBufferWriter::SendToNetSync(sdk::Client* sendClient,
                                           const sls_logs::LogtailBufferMeta& bufferMeta,
                                           const std::string& logData,
                                           std::string& errorCode) {
    int32_t retryTimes = 0;
    while (true) {
        ++retryTimes;
        try {
            if (bufferMeta.datatype() == int(RawDataType::EVENT_GROUP)) {
                if (bufferMeta.has_telemetrytype()
                    && bufferMeta.telemetrytype() == sls_logs::SLS_TELEMETRY_TYPE_METRICS) {
                    sendClient->PostMetricStoreLogs(bufferMeta.project(),
                                                    bufferMeta.logstore(),
                                                    bufferMeta.compresstype(),
                                                    logData,
                                                    bufferMeta.rawsize());
                } else if (bufferMeta.has_shardhashkey() && !bufferMeta.shardhashkey().empty())
                    sendClient->PostLogStoreLogs(bufferMeta.project(),
                                                 bufferMeta.logstore(),
                                                 bufferMeta.compresstype(),
                                                 logData,
                                                 bufferMeta.rawsize(),
                                                 bufferMeta.shardhashkey());
                else
                    sendClient->PostLogStoreLogs(bufferMeta.project(),
                                                 bufferMeta.logstore(),
                                                 bufferMeta.compresstype(),
                                                 logData,
                                                 bufferMeta.rawsize());
            } else {
                if (bufferMeta.has_shardhashkey() && !bufferMeta.shardhashkey().empty())
                    sendClient->PostLogStoreLogPackageList(bufferMeta.project(),
                                                           bufferMeta.logstore(),
                                                           bufferMeta.compresstype(),
                                                           logData,
                                                           bufferMeta.shardhashkey());
                else
                    sendClient->PostLogStoreLogPackageList(
                        bufferMeta.project(), bufferMeta.logstore(), bufferMeta.compresstype(), logData);
            }
            return SEND_OK;
        } catch (sdk::LOGException& ex) {
            errorCode = ex.GetErrorCode();
            SendResult sendRes = ConvertErrorCode(errorCode);
            if (sendRes == SEND_DISCARD_ERROR || sendRes == SEND_UNAUTHORIZED || sendRes == SEND_QUOTA_EXCEED
                || retryTimes >= INT32_FLAG(send_retrytimes)) {
                if (sendRes == SEND_QUOTA_EXCEED)
                    LogtailAlarm::GetInstance()->SendAlarm(SEND_QUOTA_EXCEED_ALARM,
                                                           "error_code: " + errorCode
                                                               + ", error_message: " + ex.GetMessage(),
                                                           bufferMeta.project(),
                                                           bufferMeta.logstore(),
                                                           "");
                // no region
                if (!GetProfileSender()->IsProfileData("", bufferMeta.project(), bufferMeta.logstore()))
                    LOG_ERROR(sLogger,
                              ("send data to SLS fail, error_code", errorCode)("error_message", ex.GetMessage())(
                                  "endpoint", sendClient->GetRawSlsHost())("projectName", bufferMeta.project())(
                                  "logstore", bufferMeta.logstore())("RetryTimes", retryTimes)("rawsize",
                                                                                               bufferMeta.rawsize()));
                return sendRes;
            } else {
                LOG_DEBUG(
                    sLogger,
                    ("send data to SLS fail", "retry later")("error_code", errorCode)("error_message", ex.GetMessage())(
                        "endpoint", sendClient->GetRawSlsHost())("projectName", bufferMeta.project())(
                        "logstore", bufferMeta.logstore())("RetryTimes", retryTimes)("rawsize", bufferMeta.rawsize()));
                usleep(INT32_FLAG(send_retry_sleep_interval));
            }
        } catch (...) {
            if (retryTimes >= INT32_FLAG(send_retrytimes)) {
                LOG_ERROR(sLogger,
                          ("send data fail", "unknown excepiton")("endpoint", sendClient->GetRawSlsHost())(
                              "projectName", bufferMeta.project())("logstore", bufferMeta.logstore())(
                              "rawsize", bufferMeta.rawsize()));
                return SEND_DISCARD_ERROR;
            } else {
                LOG_DEBUG(sLogger,
                          ("send data fail", "unknown excepiton, retry later")("endpoint", sendClient->GetRawSlsHost())(
                              "projectName", bufferMeta.project())("logstore", bufferMeta.logstore())(
                              "rawsize", bufferMeta.rawsize()));
                usleep(INT32_FLAG(send_retry_sleep_interval));
            }
        }
    }
}

} // namespace logtail