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

#include "plugin/processor/ProcessorParseJsonNative.h"

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#if defined(__INCLUDE_SSE4_2__)
#include <cinttypes>
#include <cstdio>

#include "simdjson/simdjson.h"

// Constants for boolean string conversion
static constexpr std::string_view TRUE_STR = "true";
static constexpr std::string_view FALSE_STR = "false";
#endif

#include "collection_pipeline/plugin/instance/ProcessorInstance.h"
#include "common/ParamExtractor.h"
#include "common/StringTools.h"
#include "models/LogEvent.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

const std::string ProcessorParseJsonNative::sName = "processor_parse_json_native";

bool ProcessorParseJsonNative::Init(const Json::Value& config) {
    std::string errorMsg;

    // SourceKey
    if (!GetMandatoryStringParam(config, "SourceKey", mSourceKey, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    if (!mCommonParserOptions.Init(config, *mContext, sName)) {
        return false;
    }

    // Runtime check for SIMD support
    mUseSimdJson = false;
#if defined(__INCLUDE_SSE4_2__)
    auto my_implementation = simdjson::get_available_implementations()["westmere"];
    if (my_implementation && my_implementation->supported_by_runtime_system()) {
        mUseSimdJson = true;
        simdjson::get_active_implementation() = my_implementation;
        LOG_DEBUG(sLogger, ("simdjson active implementation : ", simdjson::get_active_implementation()->name()));
    } else {
        LOG_DEBUG(sLogger, ("westmere not supported", "fallback to rapidjson"));
    }
#endif

    LOG_DEBUG(sLogger, ("use simdjson: ", mUseSimdJson));

    mDiscardedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL);
    mOutFailedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL);
    mOutKeyNotFoundEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_KEY_NOT_FOUND_EVENTS_TOTAL);
    mOutSuccessfulEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL);

    return true;
}


void ProcessorParseJsonNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }

    const StringView& logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);
    EventsContainer& events = logGroup.MutableEvents();

    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (ProcessEvent(logPath, events[rIdx], logGroup.GetAllMetadata())) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        }
    }
    events.resize(wIdx);
}

bool ProcessorParseJsonNative::ProcessEvent(const StringView& logPath,
                                            PipelineEventPtr& e,
                                            const GroupMetadata& metadata) {
    if (!IsSupportedEvent(e)) {
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        return true;
    }
    auto& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSourceKey)) {
        ADD_COUNTER(mOutKeyNotFoundEventsTotal, 1);
        return true;
    }

    auto rawContent = sourceEvent.GetContent(mSourceKey);

    bool sourceKeyOverwritten = false;
    bool parseSuccess;
    if (mUseSimdJson) {
        parseSuccess = JsonLogLineParserSimdJson(sourceEvent, logPath, e, sourceKeyOverwritten);
    } else {
        parseSuccess = JsonLogLineParserRapidJson(sourceEvent, logPath, e, sourceKeyOverwritten);
    }

    if (!parseSuccess || !sourceKeyOverwritten) {
        sourceEvent.DelContent(mSourceKey);
    }
    if (mCommonParserOptions.ShouldAddSourceContent(parseSuccess)) {
        AddLog(mCommonParserOptions.mRenamedSourceKey, rawContent, sourceEvent, false);
    }
    if (mCommonParserOptions.ShouldAddLegacyUnmatchedRawLog(parseSuccess)) {
        AddLog(mCommonParserOptions.legacyUnmatchedRawLogKey, rawContent, sourceEvent, false);
    }
    if (mCommonParserOptions.ShouldEraseEvent(parseSuccess, sourceEvent, metadata)) {
        ADD_COUNTER(mDiscardedEventsTotal, 1);
        return false;
    }
    ADD_COUNTER(mOutSuccessfulEventsTotal, 1);
    return true;
}


#if defined(__INCLUDE_SSE4_2__)
// Optimized number processing function using stack buffer
static StringBuffer
ProcessNumberValueOptimized(simdjson::ondemand::value& value, LogEvent& sourceEvent, bool& success) {
    // Use stack buffer to avoid heap allocation
    constexpr size_t BUFFER_SIZE = 32; // Sufficient for largest number string
    char buffer[BUFFER_SIZE];

    success = false;
    if (value.is_integer()) {
        if (value.is_negative()) {
            auto int_result = value.get_int64();
            if (!int_result.error()) {
                // Use snprintf directly to avoid std::to_string allocation
                int len = snprintf(buffer, BUFFER_SIZE, "%" PRId64, int_result.value());
                success = true;
                return sourceEvent.GetSourceBuffer()->CopyString(buffer, len);
            }
        } else {
            auto uint_result = value.get_uint64();
            if (!uint_result.error()) {
                int len = snprintf(buffer, BUFFER_SIZE, "%" PRIu64, uint_result.value());
                success = true;
                return sourceEvent.GetSourceBuffer()->CopyString(buffer, len);
            }
        }
    } else {
        auto double_result = value.get_double();
        if (!double_result.error()) {
            // Use std::to_string for consistency with rapidjson/ToString implementation
            std::string doubleStr = std::to_string(double_result.value());
            success = true;
            return sourceEvent.GetSourceBuffer()->CopyString(doubleStr);
        }
    }
    // Return empty buffer on error
    return sourceEvent.GetSourceBuffer()->CopyString("", 0);
}
#endif

#if defined(__INCLUDE_SSE4_2__)
// Optimized value to StringBuffer conversion function
static StringBuffer
OptimizedValueToStringBuffer(simdjson::ondemand::value& value, LogEvent& sourceEvent, bool& success) {
    success = false;
    switch (value.type()) {
        case simdjson::ondemand::json_type::null: {
            // Directly allocate empty string StringBuffer
            success = true;
            return sourceEvent.GetSourceBuffer()->CopyString("", 0);
        }
        case simdjson::ondemand::json_type::boolean: {
            auto bool_result = value.get_bool();
            if (!bool_result.error()) {
                const bool boolValue = bool_result.value();
                const auto& boolStr = boolValue ? TRUE_STR : FALSE_STR;
                success = true;
                return sourceEvent.GetSourceBuffer()->CopyString(boolStr.data(), boolStr.size());
            }
            break;
        }
        case simdjson::ondemand::json_type::string: {
            auto str_result = value.get_string();
            if (!str_result.error()) {
                // Directly copy from simdjson string view, avoiding std::string intermediary
                std::string_view str_view = str_result.value();
                success = true;
                return sourceEvent.GetSourceBuffer()->CopyString(str_view.data(), str_view.size());
            }
            break;
        }
        case simdjson::ondemand::json_type::number: {
            return ProcessNumberValueOptimized(value, sourceEvent, success);
        }
        case simdjson::ondemand::json_type::object:
        case simdjson::ondemand::json_type::array: {
            auto json_str = simdjson::to_json_string(value);
            if (!json_str.error()) {
                std::string_view json_view = json_str.value();
                success = true;
                return sourceEvent.GetSourceBuffer()->CopyString(json_view.data(), json_view.size());
            }
            break;
        }
        default: {
            break;
        }
    }
    // Return empty buffer on error
    return sourceEvent.GetSourceBuffer()->CopyString("", 0);
}
#endif

bool ProcessorParseJsonNative::JsonLogLineParser(LogEvent& sourceEvent,
                                                 const StringView& logPath,
                                                 PipelineEventPtr& e,
                                                 bool& sourceKeyOverwritten) {
    if (mUseSimdJson) {
        return JsonLogLineParserSimdJson(sourceEvent, logPath, e, sourceKeyOverwritten);
    } else {
        return JsonLogLineParserRapidJson(sourceEvent, logPath, e, sourceKeyOverwritten);
    }
}

bool ProcessorParseJsonNative::JsonLogLineParserSimdJson(LogEvent& sourceEvent,
                                                         const StringView& logPath,
                                                         PipelineEventPtr& e,
                                                         bool& sourceKeyOverwritten) {
#if defined(__INCLUDE_SSE4_2__)
    StringView buffer = sourceEvent.GetContent(mSourceKey);

    if (buffer.empty())
        return false;

    simdjson::ondemand::parser parser;
    simdjson::padded_string bufStr(buffer.data(), buffer.size());
    simdjson::ondemand::document doc;
    simdjson::ondemand::object object;

    // Use try-catch to handle all simdjson parsing errors generically
    // This maintains compatibility with rapidjson's error handling approach
    try {
        auto error = parser.iterate(bufStr).get(doc);
        if (error) {
            if (AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(
                    sLogger,
                    ("parse json log fail, log", buffer)("simdjson error", simdjson::simdjson_error(error).what())(
                        "project", GetContext().GetProjectName())("logstore", GetContext().GetLogstoreName())("file",
                                                                                                              logPath));
                AlarmManager::GetInstance()->SendAlarmWarning(PARSE_LOG_FAIL_ALARM,
                                                              std::string("parse json fail:") + buffer.to_string(),
                                                              GetContext().GetRegion(),
                                                              GetContext().GetProjectName(),
                                                              GetContext().GetConfigName(),
                                                              GetContext().GetLogstoreName());
            }
            ADD_COUNTER(mOutFailedEventsTotal, 1);
            return false;
        }

        object = doc.get_object();
    } catch (simdjson::simdjson_error& error) {
        if (AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_WARNING(sLogger,
                        ("parse json log fail, log", buffer)("simdjson error", error.what())(
                            "project", GetContext().GetProjectName())("logstore",
                                                                      GetContext().GetLogstoreName())("file", logPath));
            AlarmManager::GetInstance()->SendAlarmWarning(PARSE_LOG_FAIL_ALARM,
                                                          std::string("parse json fail:") + buffer.to_string(),
                                                          GetContext().GetRegion(),
                                                          GetContext().GetProjectName(),
                                                          GetContext().GetConfigName(),
                                                          GetContext().GetLogstoreName());
        }
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        return false;
    }

    // Store parsed fields temporarily - reserve more space to avoid reallocations
    std::vector<std::pair<StringView, StringView>> tempFields;
    tempFields.reserve(32); // Increased capacity for better performance

    // Pre-check mSourceKey to avoid string comparison in loop
    std::string_view sourceKeyView(mSourceKey);

    // Wrap the entire field iteration in try-catch as simdjson can throw during iteration
    try {
        for (auto field : object) {
            // Use simdjson error handling mechanism, reduce exception overhead
            std::string_view keyv;
            if (auto key_result = field.unescaped_key(); !key_result.error()) {
                keyv = key_result.value();
            } else {
                continue; // Skip field with error
            }

            StringBuffer contentKeyBuffer = sourceEvent.GetSourceBuffer()->CopyString(keyv.data(), keyv.size());

            // Get value
            simdjson::ondemand::value value;
            if (auto value_result = field.value(); !value_result.error()) {
                value = value_result.value();
            } else {
                continue; // Skip field with error
            }

            // Use optimized value conversion function
            bool conversionSuccess = false;
            StringBuffer contentValueBuffer = OptimizedValueToStringBuffer(value, sourceEvent, conversionSuccess);

            // If conversion failed, the function already returns an appropriate fallback buffer
            // No need for additional fallback logic here

            // Optimized string comparison
            if (keyv == sourceKeyView) {
                sourceKeyOverwritten = true;
            }

            // Store temporarily instead of adding directly
            tempFields.emplace_back(StringView(contentKeyBuffer.data, contentKeyBuffer.size),
                                    StringView(contentValueBuffer.data, contentValueBuffer.size));
        }
    } catch (simdjson::simdjson_error& error) {
        if (AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_WARNING(sLogger,
                        ("parse json log fail during iteration, log", buffer)("simdjson error", error.what())(
                            "project", GetContext().GetProjectName())("logstore",
                                                                      GetContext().GetLogstoreName())("file", logPath));
            AlarmManager::GetInstance()->SendAlarmWarning(PARSE_LOG_FAIL_ALARM,
                                                          std::string("parse json fail:") + buffer.to_string(),
                                                          GetContext().GetRegion(),
                                                          GetContext().GetProjectName(),
                                                          GetContext().GetConfigName(),
                                                          GetContext().GetLogstoreName());
        }
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        return false;
    }

    // Only add fields if all parsing succeeded
    for (const auto& field : tempFields) {
        AddLog(field.first, field.second, sourceEvent);
    }
    return true;
#endif
    // If SIMD not supported at compile time, this function should not be called
    // But we provide a fallback to ensure compilation
    return false;
}

static std::string RapidjsonValueToString(const rapidjson::Value& value) {
    if (value.IsString())
        return std::string(value.GetString(), value.GetStringLength());
    else if (value.IsBool())
        return ToString(value.GetBool());
    else if (value.IsInt())
        return ToString(value.GetInt());
    else if (value.IsUint())
        return ToString(value.GetUint());
    else if (value.IsInt64())
        return ToString(value.GetInt64());
    else if (value.IsUint64())
        return ToString(value.GetUint64());
    else if (value.IsDouble())
        return ToString(value.GetDouble());
    else if (value.IsNull())
        return "";
    else // if (value.IsObject() || value.IsArray())
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
        return std::string(buffer.GetString(), buffer.GetLength());
    }
}

bool ProcessorParseJsonNative::JsonLogLineParserRapidJson(LogEvent& sourceEvent,
                                                          const StringView& logPath,
                                                          PipelineEventPtr& e,
                                                          bool& sourceKeyOverwritten) {
    StringView buffer = sourceEvent.GetContent(mSourceKey);

    if (buffer.empty())
        return false;

    bool parseSuccess = true;
    rapidjson::Document doc;
    doc.Parse(buffer.data(), buffer.size());
    if (doc.HasParseError()) {
        if (AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_WARNING(sLogger,
                        ("parse json log fail, log", buffer)("rapidjson offset", doc.GetErrorOffset())(
                            "rapidjson error", doc.GetParseError())("project", GetContext().GetProjectName())(
                            "logstore", GetContext().GetLogstoreName())("file", logPath));
            AlarmManager::GetInstance()->SendAlarmWarning(PARSE_LOG_FAIL_ALARM,
                                                          std::string("parse json fail:") + buffer.to_string(),
                                                          GetContext().GetRegion(),
                                                          GetContext().GetProjectName(),
                                                          GetContext().GetConfigName(),
                                                          GetContext().GetLogstoreName());
        }
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        parseSuccess = false;
    } else if (!doc.IsObject()) {
        if (AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_WARNING(sLogger,
                        ("invalid json object, log", buffer)("project", GetContext().GetProjectName())(
                            "logstore", GetContext().GetLogstoreName())("file", logPath));
            AlarmManager::GetInstance()->SendAlarmWarning(PARSE_LOG_FAIL_ALARM,
                                                          std::string("invalid json object:") + buffer.to_string(),
                                                          GetContext().GetRegion(),
                                                          GetContext().GetProjectName(),
                                                          GetContext().GetConfigName(),
                                                          GetContext().GetLogstoreName());
        }
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        parseSuccess = false;
    }
    if (!parseSuccess) {
        return false;
    }

    for (rapidjson::Value::ConstMemberIterator itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
        std::string contentKey = RapidjsonValueToString(itr->name);
        std::string contentValue = RapidjsonValueToString(itr->value);

        StringBuffer contentKeyBuffer = sourceEvent.GetSourceBuffer()->CopyString(contentKey);
        StringBuffer contentValueBuffer = sourceEvent.GetSourceBuffer()->CopyString(contentValue);

        if (contentKey.c_str() == mSourceKey) {
            sourceKeyOverwritten = true;
        }

        AddLog(StringView(contentKeyBuffer.data, contentKeyBuffer.size),
               StringView(contentValueBuffer.data, contentValueBuffer.size),
               sourceEvent);
    }
    return true;
}

void ProcessorParseJsonNative::AddLog(const StringView& key,
                                      const StringView& value,
                                      LogEvent& targetEvent,
                                      bool overwritten) {
    if (!overwritten && targetEvent.HasContent(key)) {
        return;
    }
    targetEvent.SetContentNoCopy(key, value);
}

bool ProcessorParseJsonNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

} // namespace logtail
