/*
 * Copyright 2023 iLogtail Authors
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

#include "processor/ProcessorSPL.h"

#include <curl/curl.h>
#ifdef FMT_HEADER_ONLY
#undef FMT_HEADER_ONLY
#endif
#include <spl/logger/Logger.h>
#include <spl/pipeline/SplPipeline.h>

#include <iostream>

#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "logger/Logger.h"
#include "monitor/MetricConstants.h"
#include "spl/PipelineEventGroupInput.h"
#include "spl/PipelineEventGroupOutput.h"
#include "spl/SplConstants.h"

DEFINE_FLAG_INT32(logtail_spl_pipeline_quota, "", 16);
DEFINE_FLAG_INT32(logtail_spl_query_max_size, "", 65536);

using namespace apsara::sls::spl;

namespace logtail {

const std::string ProcessorSPL::sName = "processor_spl";

bool ProcessorSPL::Init(const Json::Value& config) {
    std::string errorMsg;
    if (!GetMandatoryStringParam(config, "Script", mSpl, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (!GetOptionalUIntParam(config, "TimeoutMilliSeconds", mTimeoutMills, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           mTimeoutMills,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (!GetOptionalUIntParam(config, "MaxMemoryBytes", mMaxMemoryBytes, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           mMaxMemoryBytes,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    PipelineOptions splOptions;
    // different parse mode support different spl operators
    splOptions.parserMode = parser::ParserMode::LOGTAIL;
    // spl pipeline的长度，多少管道
    splOptions.pipelineQuota = INT32_FLAG(logtail_spl_pipeline_quota);
    // spl pipeline语句的最大长度
    splOptions.queryMaxSize = INT32_FLAG(logtail_spl_query_max_size);
    // sampling for error
    splOptions.errorSampling = true;

    // this function is void and has no return
    initSPL(&splOptions);

    LoggerPtr logger;
    logger = sLogger;
    Error error;
    mSPLPipelinePtr = std::make_shared<apsara::sls::spl::SplPipeline>(
        mSpl, error, (u_int64_t)mTimeoutMills, (int64_t)mMaxMemoryBytes, logger);
    if (error.code_ != StatusCode::OK) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "failed to parse spl: " + mSpl + " error: " + error.msg_,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());                                       
    }

    mSplExcuteErrorCount = GetMetricsRecordRef().CreateCounter("proc_spl_excute_error_count");
    mSplExcuteTimeoutErrorCount = GetMetricsRecordRef().CreateCounter("proc_spl_excute_timeout_error_count");
    mSplExcuteMemoryExceedErrorCount = GetMetricsRecordRef().CreateCounter("proc_spl_excute_memory_exceed_error_count");

    // spl raw statistic
    mProcessMicros = GetMetricsRecordRef().CreateCounter("proc_spl_process_micros");
    mInputMicros = GetMetricsRecordRef().CreateCounter("proc_spl_input_micros");
    mOutputMicros = GetMetricsRecordRef().CreateCounter("proc_spl_output_micros");
    mMemPeakBytes = GetMetricsRecordRef().CreateGauge("proc_spl_mem_peak_bytes");
    mTotalTaskCount = GetMetricsRecordRef().CreateCounter("proc_spl_total_task_count");
    mSuccTaskCount = GetMetricsRecordRef().CreateCounter("proc_spl_succ_task_count");
    mFailTaskCount = GetMetricsRecordRef().CreateCounter("proc_spl_fail_task_count");

    return true;
}


void ProcessorSPL::Process(PipelineEventGroup& logGroup) {
    LOG_ERROR(
        sLogger,
        ("ProcessorSPL error", "unexpected enter in ProcessorSPL::Process(PipelineEventGroup& logGroup)")("project", mContext->GetProjectName())("logstore", mContext->GetLogstoreName())(
            "region", mContext->GetRegion())("configName", mContext->GetConfigName()));
}


void ProcessorSPL::Process(std::vector<PipelineEventGroup>& logGroupList) {
    std::string errorMsg;
    if (logGroupList.empty()) {
        return;
    }
    PipelineEventGroup logGroup = std::move(logGroupList[0]);
    std::vector<PipelineEventGroup>().swap(logGroupList);

    std::vector<std::string> colNames{FIELD_CONTENT};
    // 根据spip->getInputSearches()，设置input数组
    std::vector<Input*> inputs;
    for (const auto& search : mSPLPipelinePtr->getInputSearches()) {
        (void)search; //-Wunused-variable
        PipelineEventGroupInput* input = new PipelineEventGroupInput(colNames, logGroup, *mContext);
        if (!input) {
            logGroupList.emplace_back(std::move(logGroup));
            for (auto& input : inputs) {
                delete input;
            }
            return;
        }
        inputs.push_back(input);
    }
    // 根据spip->getOutputLabels()，设置output数组
    std::vector<Output*> outputs;
    for (const auto& resultTaskLabel : mSPLPipelinePtr->getOutputLabels()) {
        PipelineEventGroupOutput* output = new PipelineEventGroupOutput(logGroup, logGroupList, *mContext, resultTaskLabel); 
        if (!output) {
            logGroupList.emplace_back(std::move(logGroup));
            for (auto& input : inputs) {
                delete input;
            }
            for (auto& output : outputs) {
                delete output;
            }
            return;
        }
        outputs.emplace_back(output);
    }

    // 开始调用pipeline.execute
    // 传入inputs, outputs
    // 输出pipelineStats, error
    PipelineStats pipelineStats;
    auto errCode = mSPLPipelinePtr->execute(inputs, outputs, &errorMsg, &pipelineStats);

    if (errCode != StatusCode::OK) {
        LOG_ERROR(
            sLogger,
            ("execute error", errorMsg)("project", mContext->GetProjectName())("logstore", mContext->GetLogstoreName())(
                "region", mContext->GetRegion())("configName", mContext->GetConfigName()));
        mSplExcuteErrorCount->Add(1);
        // 出现错误，把原数据放回来
        logGroupList.emplace_back(std::move(logGroup));
        if (errCode == StatusCode::TIMEOUT_ERROR) {
            mSplExcuteTimeoutErrorCount->Add(1);
        } else if (errCode == StatusCode::MEM_EXCEEDED) {
            mSplExcuteMemoryExceedErrorCount->Add(1);
        }
    } else {
        mProcessMicros->Add(pipelineStats.processMicros_);
        mInputMicros->Add(pipelineStats.inputMicros_);
        mOutputMicros->Add(pipelineStats.outputMicros_);
        mMemPeakBytes->Set(pipelineStats.memPeakBytes_);
        mTotalTaskCount->Add(pipelineStats.totalTaskCount_);
        mSuccTaskCount->Add(pipelineStats.succTaskCount_);
        mFailTaskCount->Add(pipelineStats.failTaskCount_);
    }

    for (auto& input : inputs) {
        delete input;
    }
    for (auto& output : outputs) {
        delete output;
    }
    return;
}


bool ProcessorSPL::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}
} // namespace logtail
