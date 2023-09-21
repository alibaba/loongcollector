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

#include "processor/ProcessorFillGroupInfoNative.h"

#include "common/Constants.h"
#include "common/FileSystemUtil.h"
#include "reader/LogFileReader.h"
#include "plugin/ProcessorInstance.h"

namespace logtail {

bool ProcessorFillGroupInfoNative::Init(const ComponentConfig& componentConfig) {
    const PipelineConfig& config = componentConfig.GetConfig();
    mTopicFormat = config.mTopicFormat;
    if (config.mLogType != APSARA_LOG && ToLowerCaseString(mTopicFormat) == "default") {
        mTopicFormat = "none";
    }
    mGroupTopic = config.mGroupTopic;
    if ("customized" == config.mTopicFormat) {
        mStaticTopic = config.mCustomizedTopic;
        mIsStaticTopic = true;
    }
    SetMetricsRecordRef(Name(), componentConfig.GetId());
    return true;
}

void ProcessorFillGroupInfoNative::Process(PipelineEventGroup& logGroup) {
    // LogBuffer* logBuffer = dynamic_cast<LogBuffer*>(logGroup.GetSourceBuffer().get());
    std::string agentTag = ConfigManager::GetInstance()->GetUserDefinedIdSet();
    if (!agentTag.empty()) {
        logGroup.SetMetadata(EVENT_META_AGENT_TAG, ConfigManager::GetInstance()->GetUserDefinedIdSet());
    }
    logGroup.SetMetadataNoCopy(EVENT_META_HOST_IP, LogFileProfiler::mIpAddr);
    logGroup.SetMetadataNoCopy(EVENT_META_HOST_NAME, LogFileProfiler::mHostname);
    /* inode和topic还有docker的tags在reader中混杂在一起，后续剥离开后再改
    // topic
    // TODO: 除了default/GetTopicName外，预计算的场景直接在Init中可以算好放在mStaticTopicName
    const std::string lowerConfig = ToLowerCaseString(mTopicFormat);
    std::string topicName;
    auto& logPath = logGroup.GetMetadata(EVENT_META_LOG_FILE_PATH);
    if (lowerConfig == "customized") {
        topicName = mStaticTopic;
    } else if (lowerConfig == "global_topic") {
        static LogtailGlobalPara* sGlobalPara = LogtailGlobalPara::Instance();
        topicName = sGlobalPara->GetTopic();
    } else if (lowerConfig == "group_topic") {
        topicName = mGroupTopic;
    } else if (lowerConfig == "default") { // only for APSARA_LOG
        size_t pos_dot = logPath.rfind("."); // the "." must be founded
        size_t pos = logPath.find("@");
        if (pos != std::string::npos) {
            size_t pos_slash = logPath.find(PATH_SEPARATOR, pos);
            if (pos_slash != std::string::npos) {
                topicName = logPath.substr(0, pos) + logPath.substr(pos_slash, pos_dot - pos_slash);
            }
        }
        if (topicName.empty()) {
            topicName = logPath.substr(0, pos_dot);
        }
        std::string lowTopic = ToLowerCaseString(topicName);
        std::string logSuffix = ".log";

        size_t suffixPos = lowTopic.rfind(logSuffix);
        if (suffixPos == lowTopic.size() - logSuffix.size()) {
            topicName = topicName.substr(0, suffixPos);
        }
    } else if (lowerConfig != "none") {
        std::vector<sls_logs::LogTag> extraTags;
        topicName = GetTopicName(logBuffer->logFileReader->GetConvertedPath(), extraTags);
    }
    if (!topicName.empty()) {
        logGroup.SetMetadata(EVENT_META_LOG_TOPIC, topicName);
    }
    */
    return;
}

bool ProcessorFillGroupInfoNative::IsSupportedEvent(const PipelineEventPtr& /*e*/) {
    return true;
}

std::string ProcessorFillGroupInfoNative::GetTopicName(const std::string& path, std::vector<sls_logs::LogTag>& extraTags) {
    std::string finalPath = path;
    size_t len = finalPath.size();
    // ignore the ".1" like suffix when the log file is roll back
    if (len > 2 && finalPath[len - 2] == '.' && finalPath[len - 1] > '0' && finalPath[len - 1] < '9') {
        finalPath = finalPath.substr(0, len - 2);
    }

    {
        std::string res;
        // use xpressive
        std::vector<std::string> keys;
        std::vector<std::string> values;
        if (ExtractTopics(finalPath, mTopicFormat, keys, values)) {
            size_t matchedSize = values.size();
            if (matchedSize == (size_t)1) {
                // != default topic name
                if (keys[0] != "__topic_1__") {
                    sls_logs::LogTag tag;
                    tag.set_key(keys[0]);
                    tag.set_value(values[0]);
                    extraTags.push_back(tag);
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
                    extraTags.push_back(tag);
                }
            }
            return res;
        }
    }

    boost::match_results<const char*> what;
    std::string res, exception;
    // catch exception
    boost::regex topicRegex;
    try {
        topicRegex = boost::regex(mTopicFormat.c_str());
        if (BoostRegexMatch(finalPath.c_str(), finalPath.length(), topicRegex, exception, what, boost::match_default)) {
            size_t matchedSize = what.size();
            for (size_t i = 1; i < matchedSize; ++i) {
                if (res.empty()) {
                    res = what[i];
                } else {
                    res = res + "_" + what[i];
                }
                if (matchedSize > 2) {
                    sls_logs::LogTag tag;
                    tag.set_key(std::string("__topic_") + ToString(i) + "__");
                    tag.set_value(what[i]);
                    extraTags.push_back(tag);
                }
            }
        } else {
            if (!exception.empty())
                LOG_ERROR(GetContext().GetLogger(),
                          ("extract topic by regex", "fail")("exception", exception)("project",
                                                                                     GetContext().GetProjectName())(
                              "logstore", GetContext().GetLogstoreName())("path", finalPath)("regx", mTopicFormat));
            else
                LOG_WARNING(GetContext().GetLogger(),
                            ("extract topic by regex", "fail")("project", GetContext().GetProjectName())(
                                "logstore", GetContext().GetLogstoreName())("path", finalPath)("regx", mTopicFormat));

            GetContext().GetAlarm().SendAlarm(CATEGORY_CONFIG_ALARM,
                                              std::string("extract topic by regex fail, exception:") + exception
                                                  + ", path:" + finalPath + ", regex:" + mTopicFormat,
                                              GetContext().GetProjectName(),
                                              GetContext().GetLogstoreName(),
                                              GetContext().GetRegion());
        }
    } catch (...) {
        LOG_ERROR(GetContext().GetLogger(),
                  ("extract topic by regex", "fail")("exception", exception)("project", GetContext().GetProjectName())(
                      "logstore", GetContext().GetLogstoreName())("path", finalPath)("regx", mTopicFormat));
        GetContext().GetAlarm().SendAlarm(CATEGORY_CONFIG_ALARM,
                                          std::string("extract topic by regex fail, exception:") + exception
                                              + ", path:" + finalPath + ", regex:" + mTopicFormat,
                                          GetContext().GetProjectName(),
                                          GetContext().GetLogstoreName(),
                                          GetContext().GetRegion());
    }

    return res;
}

} // namespace logtail