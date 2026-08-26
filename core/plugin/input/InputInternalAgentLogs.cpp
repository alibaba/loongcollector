/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "plugin/input/InputInternalAgentLogs.h"

#include <cstdlib>
#include <ctime>
#include <sys/stat.h>

#include <filesystem>

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "collection_pipeline/plugin/instance/InputInstance.h"
#include "collection_pipeline/plugin/instance/ProcessorInstance.h"
#include "collection_pipeline/plugin/interface/Processor.h"
#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "common/RuntimeUtil.h"
#include "common/StringTools.h"
#include "constants/Constants.h"
#include "constants/TagConstants.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"
#include "monitor/Monitor.h"
#include "models/LogEvent.h"
#include "monitor/SelfMonitorServer.h"
#include "plugin/input/InputStaticFile.h"
#include "plugin/processor/ProcessorParseRegexNative.h"
#include "plugin/processor/ProcessorParseTimestampNative.h"
#include "plugin/processor/ProcessorTimestampFilterNative.h"

DECLARE_FLAG_BOOL(logtail_mode);

using namespace std;

namespace logtail {

namespace {

const char* kRuntimeStartPattern = R"(^(\[\d{4}-\d{2}-\d{2}|\d{4}-\d{2}-\d{2} ))";
// C++: [2026-08-26 08:52:00.890493] [info] [3396] /path/File.cpp:607<tab>rest
const char* kCppLogRegex
    = R"(^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?)\][ \t]+\[(\w+)\][ \t]+\[([^\]]+)\][ \t]+(\S+)[ \t]+([\s\S]*)$)";
// Go: 2026-08-26 08:52:00 [info] [file.go:123] [Start] rest
const char* kGoLogRegex
    = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?)[ \t]+\[(\w+)\][ \t]+\[([^\]]+)\][ \t]+\[([^\]]+)\][ \t]+([\s\S]*)$)";

string JoinFile(const string& dir, const string& name) {
    return (filesystem::path(dir) / name).lexically_normal().string();
}

bool ExistsPath(const string& path) {
    error_code ec;
    return !path.empty() && filesystem::exists(path, ec);
}

bool ExistsDir(const string& path) {
    error_code ec;
    return !path.empty() && filesystem::is_directory(path, ec);
}

void AppendIfExists(Json::Value& filePaths, const string& path) {
    if (ExistsPath(path)) {
        filePaths.append(path);
    }
}

void AppendGlobIfDirExists(Json::Value& filePaths, const string& dir, const string& pattern) {
    if (ExistsDir(dir)) {
        filePaths.append(JoinFile(dir, pattern));
    }
}

class ProcessorAgentLogTag : public Processor {
public:
    static const string sName;

    const string& Name() const override { return sName; }

    bool Init(const Json::Value& config) override {
        string errorMsg;
        if (!GetOptionalStringParam(config, "Aliuid", mAliuid, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        return true;
    }

    void Process(PipelineEventGroup& logGroup) override {
        logGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE, SelfMonitorServer::INTERNAL_DATA_TYPE_AGENT_LOG);
        if (!mAliuid.empty()) {
            logGroup.SetTag("aliuid", mAliuid);
        }
        string path;
        const auto filePathTag = logGroup.GetTag(DEFAULT_LOG_TAG_FILE_PATH);
        if (!filePathTag.empty()) {
            path.assign(filePathTag.data(), filePathTag.size());
        }
        if (path.empty()) {
            const auto resolved = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);
            if (!resolved.empty()) {
                path.assign(resolved.data(), resolved.size());
            }
        }
        const string artifact = InputInternalAgentLogs::InferArtifact(path);
        logGroup.SetTag("artifact", artifact);
        if (artifact == "pipeline_config" || artifact == "onetime_pipeline_config") {
            const string pipelineName = filesystem::path(path).stem().string();
            if (!pipelineName.empty()) {
                for (auto& e : logGroup.MutableEvents()) {
                    if (e.Is<LogEvent>()) {
                        e.Cast<LogEvent>().SetContent(string("pipeline_name"), pipelineName);
                    }
                }
            }
        }
    }

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override { return true; }

private:
    string mAliuid;
};

const string ProcessorAgentLogTag::sName = "processor_agent_log_tag";

class ProcessorAgentLogMicrotime : public Processor {
public:
    static const string sName;

    const string& Name() const override { return sName; }

    bool Init(const Json::Value&) override { return true; }

    void Process(PipelineEventGroup& logGroup) override {
        for (auto& e : logGroup.MutableEvents()) {
            if (!e.Is<LogEvent>()) {
                continue;
            }
            auto& ev = e.Cast<LogEvent>();
            const StringView timeStr = ev.GetContent("time");
            if (timeStr.empty()) {
                continue;
            }
            int64_t fracUs = 0;
            const char* begin = timeStr.data();
            const char* end = begin + timeStr.size();
            const char* dot = nullptr;
            for (const char* p = begin; p < end; ++p) {
                if (*p == '.') {
                    dot = p;
                    break;
                }
            }
            if (dot != nullptr && dot + 1 < end) {
                string digits;
                for (const char* p = dot + 1; p < end && *p >= '0' && *p <= '9'; ++p) {
                    digits.push_back(*p);
                }
                if (!digits.empty()) {
                    while (digits.size() < 6) {
                        digits.push_back('0');
                    }
                    if (digits.size() > 6) {
                        digits.resize(6);
                    }
                    fracUs = stoll(digits);
                }
            }
            const int64_t us = static_cast<int64_t>(ev.GetTimestamp()) * 1000000 + fracUs;
            ev.SetContent(string("microtime"), to_string(us));
        }
    }

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override { return e.Is<LogEvent>(); }
};

const string ProcessorAgentLogMicrotime::sName = "processor_agent_log_microtime";

} // namespace

const string InputInternalAgentLogs::sName = "input_internal_agent_logs_onetime";

string InputInternalAgentLogs::InferArtifact(const string& path) {
    const string norm = filesystem::path(path).generic_string();
    const string name = filesystem::path(path).filename().string();
    if (name.find("loongcollector.LOG") == 0 || name.find("ilogtail.LOG") == 0) {
        return "cpp_log";
    }
    if (name.find("go_plugin.LOG") == 0 || name.find("logtail_plugin.LOG") == 0) {
        return "go_log";
    }
    if (name == "app_info.json") {
        return "app_info";
    }
    if (name == "inotify_watcher_dirs") {
        return "inotify_watcher_dirs";
    }
    if (name == "file_check_point" || name == "logtail_check_point") {
        return "file_checkpoint";
    }
    if (name == "loongcollector_config.json" || name == "ilogtail_config.json") {
        return "main_config";
    }
    if (name == "docker_path_config.json") {
        return "docker_path_config";
    }
    if (name == "onetime_config_info.json") {
        return "onetime_config_info";
    }
    if (name == "apsara_log_conf.json") {
        return "apsara_log_conf";
    }
    if (name == "plugin_logger.xml") {
        return "plugin_logger";
    }
    if (name == "user_defined_id") {
        return "user_defined_id";
    }
    if (name == "logger_initialization.log") {
        return "logger_initialization";
    }
    if (name == "self_metrics.log") {
        return "self_metrics";
    }
    if (name == "backtrace.dat") {
        return "backtrace";
    }
    if (name == "container.json") {
        return "static_container_info";
    }
    if (name == "user_log_config.json") {
        return "legacy_config";
    }
    if (norm.find("continuous_pipeline_config") != string::npos || norm.find("/config/local/") != string::npos
        || norm.find("/config/remote/") != string::npos) {
        return "pipeline_config";
    }
    if (norm.find("onetime_pipeline_config") != string::npos) {
        return "onetime_pipeline_config";
    }
    if (norm.find("instance_config") != string::npos) {
        return "instance_config";
    }
    if (norm.find("config.d") != string::npos || norm.find("user_config.d") != string::npos) {
        return "legacy_config";
    }
    if (norm.find("input_static_file") != string::npos) {
        return "input_static_file_checkpoint";
    }
    return "agent_file";
}

bool InputInternalAgentLogs::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;
    if (!GetOptionalStringParam(config, "Aliuid", mAliuid, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    if (!GetOptionalListParam<string>(config, "IPList", mIPList, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
        mIPList.clear();
    }
    for (auto& ip : mIPList) {
        ip = Trim(ip, " \t\r\n");
    }

    int64_t startTime = 0;
    int64_t endTime = 0;
    if (!GetOptionalInt64Param(config, "StartTime", startTime, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    if (!GetOptionalInt64Param(config, "EndTime", endTime, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    if (startTime > 0 && endTime > 0 && startTime < endTime) {
        mStartTime = startTime;
        mEndTime = endTime;
        mHasTimeWindow = true;
    }

    if (!mIPList.empty()) {
        const string localIP = Trim(LoongCollectorMonitor::mIpAddr, " \t\r\n");
        bool matched = false;
        for (const auto& ip : mIPList) {
            if (!ip.empty() && ip == localIP) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            mSkipCollect = true;
            LOG_INFO(sLogger,
                     ("input_internal_agent_logs_onetime skip collect",
                      "local ip not in IPList")("local_ip", localIP)("config", mContext->GetConfigName()));
        }
    }
    return true;
}

bool InputInternalAgentLogs::Start() {
    if (mSkipCollect) {
        return true;
    }
    alarmCompressedRuntimeLogs();
    return true;
}

bool InputInternalAgentLogs::Stop(bool isPipelineRemoving) {
    return true;
}

bool InputInternalAgentLogs::ExpandAdditionalInputs(size_t startIdx, vector<unique_ptr<InputInstance>>& extras) {
    if (mSkipCollect) {
        return true;
    }
    // Inner processors consume LogEvent. Force this flag so InputStaticFile does not emit RawContent.
    mContext->SetHasNativeProcessorsFlag(true);

    struct Group {
        Json::Value config;
        RuntimeLogKind runtimeKind;
    };
    vector<Group> groups;
    const size_t kMaxFilePaths = 10;
    auto appendSplit = [&](Json::Value cfg, RuntimeLogKind runtimeKind) {
        const Json::Value paths = cfg["FilePaths"];
        if (paths.size() == 0) {
            return;
        }
        for (Json::ArrayIndex i = 0; i < paths.size(); i += kMaxFilePaths) {
            Json::Value part = cfg;
            Json::Value slice(Json::arrayValue);
            for (Json::ArrayIndex j = i; j < paths.size() && j < i + kMaxFilePaths; ++j) {
                slice.append(paths[j]);
            }
            part["FilePaths"] = slice;
            groups.push_back({std::move(part), runtimeKind});
        }
    };
    appendSplit(buildRuntimeLogsConfig(RuntimeLogKind::Cpp), RuntimeLogKind::Cpp);
    appendSplit(buildRuntimeLogsConfig(RuntimeLogKind::Go), RuntimeLogKind::Go);
    appendSplit(buildWholeSmallConfig(), RuntimeLogKind::None);
    appendSplit(buildWholeDirsConfig(), RuntimeLogKind::None);
    appendSplit(buildFileCheckpointConfig(), RuntimeLogKind::None);

    size_t idx = startIdx;
    for (auto& group : groups) {
        unique_ptr<InputInstance> extra;
        if (!createStaticFileInput(idx, group.config, group.runtimeKind, extra)) {
            return false;
        }
        extras.emplace_back(std::move(extra));
        ++idx;
    }
    return true;
}

bool InputInternalAgentLogs::createStaticFileInput(size_t inputIdx,
                                                   const Json::Value& groupConfig,
                                                   RuntimeLogKind runtimeKind,
                                                   unique_ptr<InputInstance>& extra) {
    extra = PluginRegistry::GetInstance()->CreateInput(
        InputStaticFile::sName, true, mContext->GetPipeline().GenNextPluginMeta(false));
    if (!extra) {
        LOG_ERROR(sLogger, ("create input_static_file_onetime failed", mContext->GetConfigName()));
        return false;
    }
    Json::Value optionalGoPipeline;
    if (!extra->Init(groupConfig, *mContext, inputIdx, optionalGoPipeline)) {
        return false;
    }
    auto& processors = extra->GetInnerProcessors();
    if (!appendAgentLogTagProcessor(processors)) {
        return false;
    }
    if (runtimeKind != RuntimeLogKind::None && !appendRuntimeLogProcessors(processors, runtimeKind)) {
        return false;
    }
    return true;
}

bool InputInternalAgentLogs::appendAgentLogTagProcessor(vector<unique_ptr<ProcessorInstance>>& processors) {
    auto instance
        = make_unique<ProcessorInstance>(new ProcessorAgentLogTag(), mContext->GetPipeline().GenNextPluginMeta(false));
    Json::Value detail;
    if (!mAliuid.empty()) {
        detail["Aliuid"] = mAliuid;
    }
    if (!instance->Init(detail, *mContext)) {
        return false;
    }
    processors.emplace_back(std::move(instance));
    return true;
}

bool InputInternalAgentLogs::appendAgentLogMicrotimeProcessor(vector<unique_ptr<ProcessorInstance>>& processors) {
    auto instance = make_unique<ProcessorInstance>(new ProcessorAgentLogMicrotime(),
                                                   mContext->GetPipeline().GenNextPluginMeta(false));
    Json::Value detail;
    if (!instance->Init(detail, *mContext)) {
        return false;
    }
    processors.emplace_back(std::move(instance));
    return true;
}

bool InputInternalAgentLogs::appendRuntimeLogProcessors(vector<unique_ptr<ProcessorInstance>>& processors,
                                                        RuntimeLogKind runtimeKind) {
    Json::Value regex;
    regex["SourceKey"] = "content";
    regex["KeepingSourceWhenParseFail"] = true;
    Json::Value keys(Json::arrayValue);
    if (runtimeKind == RuntimeLogKind::Cpp) {
        regex["Regex"] = kCppLogRegex;
        keys.append("time");
        keys.append("level");
        keys.append("__THREAD__");
        keys.append("__FILE__");
        keys.append("content");
    } else {
        regex["Regex"] = kGoLogRegex;
        keys.append("time");
        keys.append("level");
        keys.append("__FILE__");
        keys.append("function");
        keys.append("content");
    }
    regex["Keys"] = keys;
    if (!appendProcessor(processors, ProcessorParseRegexNative::sName, regex)) {
        return false;
    }

    Json::Value ts;
    ts["SourceKey"] = "time";
    ts["SourceFormat"] = "%Y-%m-%d %H:%M:%S";
    if (!appendProcessor(processors, ProcessorParseTimestampNative::sName, ts)) {
        return false;
    }
    if (!appendAgentLogMicrotimeProcessor(processors)) {
        return false;
    }

    if (!mHasTimeWindow) {
        return true;
    }

    Json::Value filter;
    filter["TimestampPrecision"] = "second";
    filter["LowerBound"] = Json::Value(static_cast<Json::Int64>(mStartTime));
    filter["UpperBound"] = Json::Value(static_cast<Json::Int64>(mEndTime));
    return appendProcessor(processors, ProcessorTimestampFilterNative::sName, filter);
}

bool InputInternalAgentLogs::appendProcessor(vector<unique_ptr<ProcessorInstance>>& processors,
                                             const string& type,
                                             const Json::Value& detail) {
    auto processor
        = PluginRegistry::GetInstance()->CreateProcessor(type, mContext->GetPipeline().GenNextPluginMeta(false));
    if (!processor) {
        LOG_ERROR(sLogger, ("create processor failed", type)("config", mContext->GetConfigName()));
        return false;
    }
    if (!processor->Init(detail, *mContext)) {
        return false;
    }
    processors.emplace_back(std::move(processor));
    return true;
}

Json::Value InputInternalAgentLogs::buildRuntimeLogsConfig(RuntimeLogKind runtimeKind) const {
    Json::Value cfg;
    cfg["Type"] = InputStaticFile::sName;
    Json::Value filePaths(Json::arrayValue);
    const string logDir = GetAgentLogDir();
    if (ExistsDir(logDir)) {
        if (runtimeKind == RuntimeLogKind::Cpp) {
            filePaths.append(JoinFile(logDir, GetAgentLogName() + "*"));
        } else if (runtimeKind == RuntimeLogKind::Go) {
            filePaths.append(JoinFile(logDir, GetPluginLogName() + "*"));
        }
    }
    cfg["FilePaths"] = filePaths;
    Json::Value exclude(Json::arrayValue);
    exclude.append("*.gz");
    cfg["ExcludeFiles"] = exclude;
    cfg["Multiline"]["Mode"] = "custom";
    cfg["Multiline"]["StartPattern"] = kRuntimeStartPattern;
    cfg["Multiline"]["UnmatchedContentTreatment"] = "single_line";
    return cfg;
}

Json::Value InputInternalAgentLogs::buildWholeSmallConfig() const {
    Json::Value cfg;
    cfg["Type"] = InputStaticFile::sName;
    Json::Value filePaths(Json::arrayValue);
    AppendIfExists(filePaths, GetAgentAppInfoFile());
    AppendIfExists(filePaths, GetInotifyWatcherDirsDumpFileName());
    AppendIfExists(filePaths, GetCrashStackFileName());

    const string confDir = AppConfig::GetInstance()->GetLoongcollectorConfDir();
    if (BOOL_FLAG(logtail_mode)) {
        AppendIfExists(filePaths, GetAgentConfigFile());
        AppendIfExists(filePaths, JoinFile(GetProcessExecutionDir(), "docker_path_config.json"));
        AppendIfExists(filePaths, JoinFile(GetProcessExecutionDir(), "checkpoint/docker_path_config.json"));
    } else {
        AppendIfExists(filePaths, JoinFile(JoinFile(confDir, "instance_config/local"), LOONGCOLLECTOR_CONFIG));
        AppendIfExists(filePaths, JoinFile(GetAgentDataDir(), "docker_path_config.json"));
    }
    AppendIfExists(filePaths, JoinFile(GetAgentDataDir(), "onetime_config_info.json"));
    AppendIfExists(filePaths, JoinFile(confDir, "apsara_log_conf.json"));
    AppendIfExists(filePaths, JoinFile(confDir, "plugin_logger.xml"));
    AppendIfExists(filePaths, JoinFile(confDir, "user_defined_id"));
    AppendIfExists(filePaths, JoinFile(GetAgentLogDir(), "logger_initialization.log"));
    AppendIfExists(filePaths, JoinFile(JoinFile(GetAgentLogDir(), "self_metrics"), "self_metrics.log"));
    AppendIfExists(filePaths, JoinFile(GetLegacyUserLocalConfigFilePath(), "user_log_config.json"));

    const char* staticContainer = getenv("ALIYUN_LOG_STATIC_CONTAINER_INFO");
    if (staticContainer != nullptr && staticContainer[0] != '\0') {
        AppendIfExists(filePaths, staticContainer);
    }

    cfg["FilePaths"] = filePaths;
    cfg["Multiline"]["Mode"] = "whole_file";
    return cfg;
}

Json::Value InputInternalAgentLogs::buildWholeDirsConfig() const {
    Json::Value cfg;
    cfg["Type"] = InputStaticFile::sName;
    Json::Value filePaths(Json::arrayValue);
    const string confDir = AppConfig::GetInstance()->GetLoongcollectorConfDir();
    AppendGlobIfDirExists(filePaths, JoinFile(confDir, GetContinuousPipelineConfigDir()), "**/*");
    AppendGlobIfDirExists(filePaths, JoinFile(confDir, "onetime_pipeline_config"), "**/*");
    AppendGlobIfDirExists(filePaths, JoinFile(confDir, "instance_config"), "**/*");
    AppendGlobIfDirExists(filePaths, JoinFile(confDir, "config.d"), "*.json");
    AppendGlobIfDirExists(filePaths, JoinFile(confDir, "user_config.d"), "*.json");
    AppendGlobIfDirExists(filePaths, JoinFile(GetAgentDataDir(), "input_static_file"), "*.json");
    cfg["FilePaths"] = filePaths;
    cfg["MaxDirSearchDepth"] = 3;
    cfg["Multiline"]["Mode"] = "whole_file";
    return cfg;
}

Json::Value InputInternalAgentLogs::buildFileCheckpointConfig() const {
    Json::Value cfg;
    cfg["Type"] = InputStaticFile::sName;
    Json::Value filePaths(Json::arrayValue);
    AppendIfExists(filePaths, GetCheckPointFileName());
    cfg["FilePaths"] = filePaths;
    cfg["Multiline"]["Mode"] = "whole_file";
    return cfg;
}

void InputInternalAgentLogs::alarmCompressedRuntimeLogs() const {
    const filesystem::path logDir(GetAgentLogDir());
    error_code ec;
    if (!filesystem::is_directory(logDir, ec)) {
        return;
    }
    const vector<string> prefixes{GetAgentLogName(), GetPluginLogName()};
    for (const auto& entry : filesystem::directory_iterator(logDir, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        const string name = entry.path().filename().string();
        if (name.size() < 3 || name.compare(name.size() - 3, 3, ".gz") != 0) {
            continue;
        }
        bool match = false;
        for (const auto& prefix : prefixes) {
            if (name.compare(0, prefix.size(), prefix) == 0) {
                match = true;
                break;
            }
        }
        if (!match) {
            continue;
        }
        struct stat st {};
        if (stat(entry.path().string().c_str(), &st) != 0) {
            continue;
        }
        time_t mtime = st.st_mtime;
        tm tmBuf{};
#if defined(_MSC_VER)
        localtime_s(&tmBuf, &mtime);
#else
        localtime_r(&mtime, &tmBuf);
#endif
        char timeBuf[32];
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tmBuf);
        const string msg = string("file already compressed, cannot collect: ") + entry.path().string()
            + ", last modify time: " + timeBuf;
        LOG_WARNING(sLogger, ("skip compressed agent log", msg)("config", mContext->GetConfigName()));
        AlarmManager::GetInstance()->SendAlarmWarning(SKIP_READ_LOG_ALARM,
                                                      msg,
                                                      mContext->GetRegion(),
                                                      mContext->GetProjectName(),
                                                      mContext->GetConfigName(),
                                                      mContext->GetLogstoreName());
    }
}

} // namespace logtail
