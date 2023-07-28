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
#include <sstream>
#include <fstream>
#include <map>
#include <spdlog/spdlog.h>

namespace logtail {

class Logger {
    const std::string DEFAULT_LOGGER_NAME = "/";
    const std::string DEFAULT_PATTERN = "[%Y-%m-%d %H:%M:%S.%f]\t[%l]\t[%t]\t%v";

public:
    using logger = std::shared_ptr<spdlog::logger>;

    static Logger& Instance();

    void InitGlobalLoggers();

    logger CreateLogger(const std::string& loggerName,
                        const std::string& logFilePath,
                        unsigned int maxLogFileNum,
                        unsigned long long maxLogFileSize,
                        unsigned int maxDaysFromModify = 0, // Not supported
                        const std::string compress = ""); // Not supported

    // If not found, return the default logger.
    logger GetLogger(const std::string& loggerName);

private:
    Logger();
    ~Logger();

    // Use it to log something during intiliazing logger, it will
    // be closed if logger has already been initialized successfully.
    std::ofstream mInnerLogger;
    void LogMsg(const std::string& msg);

    struct SinkConfig {
        std::string type;
        unsigned int maxLogFileNum;
        unsigned long long maxLogFileSize;
        unsigned int maxDaysFromModify; // Not supported.
        std::string logFilePath;
        std::string compress; // Not supported
    };

    struct LoggerConfig {
        std::string sinkName;
        spdlog::level::level_enum level;
    };

    // Try to load config, if failed, use default config.
    void LoadConfig(const std::string& filePath);
    // Save config in JSON to @filePath.
    void SaveConfig(const std::string& filePath,
                    std::map<std::string, LoggerConfig>& loggerCfgs,
                    std::map<std::string, SinkConfig>& sinkCfgs);

    // LoadDefaultConfig only loads the default ("/") into @loggerCfgs and @sinkCfgs.
    void LoadDefaultConfig(std::map<std::string, LoggerConfig>& loggerCfgs,
                           std::map<std::string, SinkConfig>& sinkCfgs);
    // LoadAllDefaultConfigs loads all default configs into @loggerCfgs and @sinkCfgs.
    void LoadAllDefaultConfigs(std::map<std::string, LoggerConfig>& loggerCfgs,
                               std::map<std::string, SinkConfig>& sinkCfgs);

    // EnsureSnapshotDirExist ensures the snapshot dir exists.
    void EnsureSnapshotDirExist(std::map<std::string, SinkConfig>& sinkCfgs);
};

} // namespace logtail

class LogMaker {
    std::ostringstream mOStringStream;

public:
    template <typename T>
    LogMaker& operator()(const std::string& key, const T& value) {
        return this->operator()(key.c_str(), value);
    }

    template <typename T>
    LogMaker& operator()(const char* key, const T& value) {
        mOStringStream << "\t" << key << ":" << value;
        return *this;
    }

    LogMaker& operator()(const std::string& key, bool value) {
        return this->operator()(key.c_str(), value ? "true" : "false");
    }
    LogMaker& operator()(const char* key, bool value) { return this->operator()(key, value ? "true" : "false"); }

    const std::string GetContent() const { return mOStringStream.str(); }
};

#define LOG_X_IF(logger, condition, fields, level) \
    do { \
        if (condition && logger->should_log(level)) { \
            LogMaker maker; \
            (void)maker fields; \
            logger->log(level, "{}:{}\t{}", __FILE__, __LINE__, maker.GetContent()); \
        } \
    } while (0)

#ifndef LOG_INFO
#define LOG_TRACE(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::trace)
#define LOG_DEBUG(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::debug)
#define LOG_INFO(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::info)
#define LOG_WARNING(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::warn)
#define LOG_ERROR(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::err)
#define LOG_FATAL(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::info)
#endif
// For compatibility with syslog.
#define APSARA_LOG_DEBUG(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::debug)
#define APSARA_LOG_INFO(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::info)
#define APSARA_LOG_WARNING(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::warn)
#define APSARA_LOG_ERROR(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::err)
#define APSARA_LOG_FATAL(logger, fields) LOG_X_IF(logger, true, fields, spdlog::level::info)

// Global loggers.
// NOTE: Please call Logger::Instance().InitGlobalLoggers() to init these loggers in main().
extern logtail::Logger::logger sLogger;
