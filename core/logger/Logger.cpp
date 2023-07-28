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

#include "Logger.h"
#include <set>
#include <json/json.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include "common/RuntimeUtil.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/ErrorUtil.h"
#include "common/FileSystemUtil.h"
#include <boost/filesystem.hpp>

DEFINE_FLAG_STRING(logtail_snapshot_dir, "snapshot dir on local disk", "snapshot");
DEFINE_FLAG_BOOL(logtail_async_logger_enable, "", true);
DEFINE_FLAG_INT32(logtail_async_logger_queue_size, "", 1024);
DEFINE_FLAG_INT32(logtail_async_logger_thread_num, "", 1);

// Global loggers, set in InitGlobalLoggers.
logtail::Logger::logger sLogger;

namespace logtail {

namespace level = spdlog::level;

// TODO: There is a CALL level in apsara, have to mapping it??
static bool MapStringToLevel(const std::string& str, level::level_enum& level) {
    if ("TRACE" == str)
        level = level::trace;
    else if ("DEBUG" == str)
        level = level::debug;
    else if ("INFO" == str)
        level = level::info;
    else if ("WARNING" == str)
        level = level::warn;
    else if ("ERROR" == str)
        level = level::err;
    else if ("FATAL" == str)
        level = level::critical;
    else
        return false;

    return true;
}
static std::string MapLevelToString(level::level_enum level) {
    switch (level) {
        case level::trace:
            return "TRACE";
        case level::debug:
            return "DEBUG";
        case level::info:
            return "INFO";
        case level::warn:
            return "WARNING";
        case level::err:
            return "ERROR";
        case level::critical:
            return "FATAL";
        default:
            return "INFO";
    }
}

Logger& Logger::Instance() {
    // Works fine after C++11.
    static Logger logger;
    return logger;
}

Logger::Logger() {
    if (BOOL_FLAG(logtail_async_logger_enable)) {
        spdlog::init_thread_pool(INT32_FLAG(logtail_async_logger_queue_size),
                                 INT32_FLAG(logtail_async_logger_thread_num));
    }

    auto execDir = GetProcessExecutionDir();
    mInnerLogger.open(execDir + "logger_initialization.log");
    LoadConfig(execDir + "apsara_log_conf.json");
    mInnerLogger.close();
}

Logger::~Logger() {
}

void Logger::LogMsg(const std::string& msg) {
    if (!mInnerLogger)
        return;

    mInnerLogger << msg << std::endl;
}

void Logger::InitGlobalLoggers() {
    if (!sLogger)
        sLogger = GetLogger("/apsara/sls/ilogtail");
}

Logger::logger Logger::CreateLogger(const std::string& loggerName,
                                    const std::string& logFilePath,
                                    unsigned int maxLogFileNum,
                                    unsigned long long maxLogFileSize,
                                    unsigned int maxDaysFromModify, // Not supported
                                    const std::string compress) // Not supported
{
    auto absoluteFilePath = logFilePath;
#if defined(_MSC_VER)
    if (std::string::npos == absoluteFilePath.find(":"))
#elif defined(__linux__)
    if (!absoluteFilePath.empty() && absoluteFilePath[0] != '/')
#endif
    {
        absoluteFilePath = GetProcessExecutionDir() + absoluteFilePath;
    }
    LogMsg("Path of logger named " + loggerName + ": " + absoluteFilePath);

    // TODO: Add supports for @maxDaysFromModify and @compress.
    using rotating_sink = spdlog::sinks::rotating_file_sink_mt;
    static std::map<std::string, std::shared_ptr<rotating_sink>> sinksCache;

    try {
        auto cacheIter = sinksCache.find(absoluteFilePath);
        std::shared_ptr<rotating_sink> sink = nullptr;
        if (sinksCache.end() == cacheIter) {
            sink = std::make_shared<rotating_sink>(absoluteFilePath, maxLogFileSize, maxLogFileNum);
            if (nullptr == sink) {
                LogMsg(std::string("Create sink failed, params: ") + absoluteFilePath + ", "
                       + std::to_string(maxLogFileSize) + std::to_string(maxLogFileNum));
                return nullptr;
            }
            sinksCache.insert(std::make_pair(absoluteFilePath, sink));
        } else
            sink = cacheIter->second;

        if (BOOL_FLAG(logtail_async_logger_enable)) {
            return std::make_shared<spdlog::async_logger>(
                loggerName, sink, spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
        } else {
            return std::make_shared<spdlog::logger>(loggerName, sink);
        }
    } catch (std::exception& e) {
        LogMsg(std::string("CreateLogger exception, logger name: ") + loggerName + ", exception: " + e.what());
    } catch (...) {
        LogMsg(std::string("CreateLogger exception, logger name: ") + loggerName + ", exception: unknown");
    }
    return nullptr;
}

Logger::logger Logger::GetLogger(const std::string& loggerName) {
    auto logger = spdlog::get(loggerName);
    return (logger != nullptr) ? logger : spdlog::get(DEFAULT_LOGGER_NAME);
}

// Config file schema.
// "Loggers": {
//     "/": { // Key to get the logger, the default logger.
//          // Attributes for the logger, (sink_name, log_level).
//          "AsyncFileSink": "WARNING"
//     },
//     "/apsara/sls/ilogtail": { // Another logger.
//          // ....
//     }
// },
// "Sinks": {
//     ”AsyncFileSink": { // Sink name
//          // Attributes for the sink
//     },
//     "AsyncFileSinkProfile": { // Another sink
//     }
// }
// If no default logger is provided, we will create one (with default sink if it is not exist too).
// If the sink of a logger is not exist, it will not be created.
// If sink is defined, all parameters must be provided except for Compress (empty by default).
void Logger::LoadConfig(const std::string& filePath) {
    std::map<std::string, LoggerConfig> loggerConfigs;
    std::map<std::string, SinkConfig> sinkConfigs;
    std::string logConfigInfo;

    // Load config file, check if it is valid or not.
    do {
        std::ifstream in(filePath);
        if (!in.good())
            break;

        in.seekg(0, std::ios::end);
        size_t len = in.tellg();
        in.seekg(0, std::ios::beg);
        std::vector<char> buffer(len + 1, '\0');
        in.read(buffer.data(), len);
        in.close();

        Json::Value jsonRoot;
        Json::Reader jsonReader;
        bool parseRet = jsonReader.parse(buffer.data(), jsonRoot);
        if (!parseRet)
            break;

        // Parse Sinks at first, so that we can find them when parsing Loggers.
        if (!jsonRoot.isMember("Sinks") || !jsonRoot["Sinks"].isObject() || jsonRoot["Sinks"].empty()) {
            break;
        }
        auto& jsonSinks = jsonRoot["Sinks"];
        auto sinkNames = jsonSinks.getMemberNames();
        for (auto& name : sinkNames) {
            auto& jsonSink = jsonSinks[name];
            if (!jsonSink.isObject()
                || !(jsonSink.isMember("Type") && jsonSink["Type"].isString() && !jsonSink["Type"].asString().empty())
                || !(jsonSink.isMember("MaxLogFileNum") && jsonSink["MaxLogFileNum"].isIntegral()
                     && jsonSink["MaxLogFileNum"].asInt() > 0)
                || !(jsonSink.isMember("MaxLogFileSize") && jsonSink["MaxLogFileSize"].isIntegral()
                     && jsonSink["MaxLogFileSize"].asInt64() > 0)
                || !(jsonSink.isMember("MaxDaysFromModify") && jsonSink["MaxDaysFromModify"].isIntegral()
                     && jsonSink["MaxDaysFromModify"].asInt() > 0)
                || !(jsonSink.isMember("LogFilePath") && jsonSink["LogFilePath"].isString()
                     && !jsonSink["LogFilePath"].asString().empty())) {
                continue;
            }

            SinkConfig sinkCfg;
            sinkCfg.type = jsonSink["Type"].asString();
            sinkCfg.maxLogFileNum = jsonSink["MaxLogFileNum"].asInt();
            sinkCfg.maxLogFileSize = jsonSink["MaxLogFileSize"].asInt64();
            sinkCfg.maxDaysFromModify = jsonSink["MaxDaysFromModify"].asInt();
            sinkCfg.logFilePath = jsonSink["LogFilePath"].asString();
            if (jsonSink.isMember("Compress") && jsonSink["Compress"].isString()) {
                sinkCfg.compress = jsonSink["Compress"].asString();
            }
            sinkConfigs[name] = std::move(sinkCfg);
        }
        if (sinkConfigs.empty())
            break;

        if (!jsonRoot.isMember("Loggers") || !jsonRoot["Loggers"].isObject() || jsonRoot["Loggers"].empty()) {
            break;
        }
        auto& jsonLoggers = jsonRoot["Loggers"];
        auto loggerNames = jsonLoggers.getMemberNames();
        for (auto& name : loggerNames) {
            auto& jsonLogger = jsonLoggers[name];
            if (!jsonLogger.isObject() || jsonLogger.empty())
                continue;

            auto sinkName = jsonLogger.getMemberNames()[0];
            if (sinkName.empty() || !jsonLogger[sinkName].isString())
                continue;
            if (sinkConfigs.find(sinkName) == sinkConfigs.end())
                continue;
            LoggerConfig logCfg;
            if (!MapStringToLevel(jsonLogger[sinkName].asString(), logCfg.level))
                continue;
            logCfg.sinkName = std::move(sinkName);
            loggerConfigs[name] = std::move(logCfg);
        }

        logConfigInfo = "Load log config from " + filePath;
    } while (0);

    // Add or supply default config(s).
    bool needSave = true;
    if (loggerConfigs.empty()) {
        logConfigInfo = "Load log config file failed, use default configs.";
        LoadAllDefaultConfigs(loggerConfigs, sinkConfigs);
    } else if (loggerConfigs.end() == loggerConfigs.find(DEFAULT_LOGGER_NAME)) {
        logConfigInfo += ", and add default config.";
        LoadDefaultConfig(loggerConfigs, sinkConfigs);
    } else
        needSave = false;

    EnsureSnapshotDirExist(sinkConfigs);

    LogMsg(logConfigInfo);
    LogMsg(std::string("Logger size in config: ") + std::to_string(loggerConfigs.size()));

    // Create loggers, record failed loggers.
    std::set<std::string> failedLoggers;
    for (auto& loggerIter : loggerConfigs) {
        auto& name = loggerIter.first;
        auto& loggerCfg = loggerIter.second;
        auto& sinkCfg = sinkConfigs[loggerCfg.sinkName];

        auto logger = CreateLogger(name,
                                   sinkCfg.logFilePath,
                                   sinkCfg.maxLogFileNum,
                                   sinkCfg.maxLogFileSize,
                                   sinkCfg.maxDaysFromModify,
                                   sinkCfg.compress);
        if (nullptr == logger) {
            failedLoggers.insert(name);
            LogMsg(std::string("[ERROR] logger named ") + name + " created failed.");
            continue;
        }

        spdlog::register_logger(logger);
        logger->set_level(loggerCfg.level);
        logger->set_pattern(DEFAULT_PATTERN);
        logger->flush_on(loggerCfg.level);
        LogMsg(std::string("logger named ") + name + " created.");
    }
    if (failedLoggers.empty()) {
        if (needSave)
            SaveConfig(filePath, loggerConfigs, sinkConfigs);
        return;
    }

    // If the default logger failed, downgrade to console logger.
    // It is useless, but we need to offer at least one available logger.
    if (failedLoggers.find(DEFAULT_LOGGER_NAME) != failedLoggers.end()) {
        try {
            spdlog::stdout_logger_mt(DEFAULT_LOGGER_NAME);
        } catch (...) {
            LogMsg("Create console logger for default logger failed, we have no idea now.");
            throw "Initialize logger failed......";
        }

        failedLoggers.erase(DEFAULT_LOGGER_NAME);
    }
}

void Logger::SaveConfig(const std::string& filePath,
                        std::map<std::string, LoggerConfig>& loggerCfgs,
                        std::map<std::string, SinkConfig>& sinkCfgs) {
    std::ofstream out(filePath);
    if (!out) {
        LogMsg("SaveConfig open file " + filePath + " failed");
        return;
    }

    Json::Value rootVal(Json::objectValue);

    // Loggers.
    Json::Value loggerVal(Json::objectValue);
    for (auto& lgIter : loggerCfgs) {
        auto& name = lgIter.first;
        auto& loggerCfg = lgIter.second;

        Json::Value lgValue(Json::objectValue);
        lgValue[loggerCfg.sinkName] = MapLevelToString(loggerCfg.level);
        loggerVal[name] = lgValue;
    }
    rootVal["Loggers"] = loggerVal;

    // Sinks.
    Json::Value sinkVal(Json::objectValue);
    for (auto& sinkIter : sinkCfgs) {
        auto& name = sinkIter.first;
        auto& sinkCfg = sinkIter.second;

        Json::Value skValue(Json::objectValue);
        skValue["Type"] = sinkCfg.type;
        skValue["MaxLogFileNum"] = sinkCfg.maxLogFileNum;
        skValue["MaxLogFileSize"] = Json::UInt64(sinkCfg.maxLogFileSize);
        skValue["MaxDaysFromModify"] = sinkCfg.maxDaysFromModify;
        skValue["LogFilePath"] = sinkCfg.logFilePath;
        skValue["Compress"] = sinkCfg.compress;
        sinkVal[name] = skValue;
    }
    rootVal["Sinks"] = sinkVal;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "\t";
    out << Json::writeString(builder, rootVal);
}

void Logger::LoadDefaultConfig(std::map<std::string, LoggerConfig>& loggerCfgs,
                               std::map<std::string, SinkConfig>& sinkCfgs) {
    loggerCfgs.insert({DEFAULT_LOGGER_NAME, LoggerConfig{"AsyncFileSink", level::warn}});
    if (sinkCfgs.find("AsyncFileSink") != sinkCfgs.end())
        return;
    sinkCfgs.insert({"AsyncFileSink",
                     SinkConfig{"AsyncFile", 10, 20000000, 300, GetProcessExecutionDir() + "ilogtail.LOG", "Gzip"}});
}

void Logger::LoadAllDefaultConfigs(std::map<std::string, LoggerConfig>& loggerCfgs,
                                   std::map<std::string, SinkConfig>& sinkCfgs) {
    LoadDefaultConfig(loggerCfgs, sinkCfgs);

    loggerCfgs.insert({"/apsara/sls/ilogtail", LoggerConfig{"AsyncFileSink", level::info}});
    loggerCfgs.insert({"/apsara/sls/ilogtail/profile", LoggerConfig{"AsyncFileSinkProfile", level::info}});
    loggerCfgs.insert({"/apsara/sls/ilogtail/status", LoggerConfig{"AsyncFileSinkStatus", level::info}});

    std::string dirPath = GetProcessExecutionDir() + STRING_FLAG(logtail_snapshot_dir);
    if (!Mkdir(dirPath)) {
        LogMsg(std::string("Create snapshot dir error ") + dirPath + ", error" + ErrnoToString(GetErrno()));
    }
    sinkCfgs.insert(
        {"AsyncFileSinkProfile", SinkConfig{"AsyncFile", 61, 1, 1, dirPath + PATH_SEPARATOR + "ilogtail_profile.LOG"}});
    sinkCfgs.insert(
        {"AsyncFileSinkStatus", SinkConfig{"AsyncFile", 61, 1, 1, dirPath + PATH_SEPARATOR + "ilogtail_status.LOG"}});
}

void Logger::EnsureSnapshotDirExist(std::map<std::string, SinkConfig>& sinkCfgs) {
    if (sinkCfgs.size() == 0) {
        return;
    }

    for (const auto& sink : sinkCfgs) {
        std::string sinkName = sink.first;
        SinkConfig sinkCfg = sink.second;

        if (sinkName == "AsyncFileSinkStatus" || sinkName == "AsyncFileSinkProfile") {
            boost::filesystem::path logFilePath(sinkCfg.logFilePath);
            try {
                boost::filesystem::create_directories(logFilePath.parent_path());
            } catch (const boost::filesystem::filesystem_error& e) {
                LogMsg(std::string("Create snapshot dir error ") + logFilePath.parent_path().string() + ", error"
                       + std::string(e.what()));
            }
        }
    }
}

} // namespace logtail
