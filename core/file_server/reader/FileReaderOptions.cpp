// Copyright 2023 iLogtail Authors
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

#include "file_server/reader/FileReaderOptions.h"

#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/ParamExtractor.h"

using namespace std;

// Windows only has polling, give a bigger tail limit.
#if defined(__linux__)
DEFINE_FLAG_INT32(default_tail_limit_kb,
                  "when first open file, if offset little than this value, move offset to beginning, KB",
                  1024);
#elif defined(_MSC_VER)
DEFINE_FLAG_INT32(default_tail_limit_kb,
                  "when first open file, if offset little than this value, move offset to beginning, KB",
                  1024 * 50);
#endif
DEFINE_FLAG_INT32(default_reader_flush_timeout, "", 60);
DEFINE_FLAG_INT32(delay_bytes_upperlimit,
                  "if (total_file_size - current_readed_size) exceed uppperlimit, send READ_LOG_DELAY_ALARM, bytes",
                  200 * 1024 * 1024);
DEFINE_FLAG_INT32(reader_close_unused_file_time, "second ", 60);
DEFINE_FLAG_INT32(logreader_max_rotate_queue_size, "", 20);

namespace logtail {

FileReaderOptions::FileReaderOptions()
    : mTailSizeKB(static_cast<uint32_t>(INT32_FLAG(default_tail_limit_kb))),
      mFlushTimeoutSecs(static_cast<uint32_t>(INT32_FLAG(default_reader_flush_timeout))),
      mReadDelayAlertThresholdBytes(static_cast<uint32_t>(INT32_FLAG(delay_bytes_upperlimit))),
      mCloseUnusedReaderIntervalSec(static_cast<uint32_t>(INT32_FLAG(reader_close_unused_file_time))),
      mRotatorQueueSize(static_cast<uint32_t>(INT32_FLAG(logreader_max_rotate_queue_size))) {
}

bool FileReaderOptions::Init(const Json::Value& config, const PipelineContext& ctx, const string& pluginType) {
    string errorMsg;

    // FileEncoding
    string encoding;
    if (!GetOptionalStringParam(config, "FileEncoding", encoding, errorMsg)) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           errorMsg,
                           pluginType,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }
    encoding = ToLowerCaseString(encoding);
    if (encoding == "gbk") {
        mFileEncoding = Encoding::GBK;
    } else if (encoding == "utf16") {
        mFileEncoding = Encoding::UTF16;
    } else if (!encoding.empty() && encoding != "utf8") {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           "string param FileEncoding is not valid",
                           pluginType,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }

    // TailingAllMatchedFiles
    if (!GetOptionalBoolParam(config, "TailingAllMatchedFiles", mTailingAllMatchedFiles, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mTailingAllMatchedFiles,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    // TailSizeKB
    uint32_t tailSize = INT32_FLAG(default_tail_limit_kb);
    if (!GetOptionalUIntParam(config, "TailSizeKB", tailSize, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mTailSizeKB,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    } else if (tailSize > 100 * 1024 * 1024) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              "uint param TailSizeKB is larger than 104857600",
                              mTailSizeKB,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    } else {
        mTailSizeKB = tailSize;
    }

    // FlushTimeoutSecs
    if (!GetOptionalUIntParam(config, "FlushTimeoutSecs", mFlushTimeoutSecs, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mFlushTimeoutSecs,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    // ReadDelaySkipThresholdBytes
    if (!GetOptionalUIntParam(config, "ReadDelaySkipThresholdBytes", mReadDelaySkipThresholdBytes, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mReadDelaySkipThresholdBytes,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    // ReadDelayAlertThresholdBytes
    if (!GetOptionalUIntParam(config, "ReadDelayAlertThresholdBytes", mReadDelayAlertThresholdBytes, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mReadDelayAlertThresholdBytes,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    // CloseUnusedReaderIntervalSec
    if (!GetOptionalUIntParam(config, "CloseUnusedReaderIntervalSec", mCloseUnusedReaderIntervalSec, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mCloseUnusedReaderIntervalSec,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    // RotatorQueueSize
    if (!GetOptionalUIntParam(config, "RotatorQueueSize", mRotatorQueueSize, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mRotatorQueueSize,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    // AppendingLogPositionMeta
    if (!GetOptionalBoolParam(config, "AppendingLogPositionMeta", mAppendingLogPositionMeta, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mAppendingLogPositionMeta,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    return true;
}

} // namespace logtail
