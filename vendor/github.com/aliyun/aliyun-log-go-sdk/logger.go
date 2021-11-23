package sls

import (
	"os"

	"github.com/go-kit/kit/log"
	"github.com/go-kit/kit/log/level"
	"gopkg.in/natefinch/lumberjack.v2"
)

var Logger = initDefaultSLSLogger()

func initDefaultSLSLogger() log.Logger {
	logFileName := os.Getenv("SLS_GO_SDK_LOG_FILE_NAME")
	isJsonType := os.Getenv("SLS_GO_SDK_IS_JSON_TYPE")
	logMaxSize := os.Getenv("SLS_GO_SDK_LOG_MAX_SIZE")
	logFileBackupCount := os.Getenv("SLS_GO_SDK_LOG_FILE_BACKUP_COUNT")
	allowLogLevel := os.Getenv("SLS_GO_SDK_ALLOW_LOG_LEVEL")
	return GenerateInnerLogger(logFileName, isJsonType, logMaxSize, logFileBackupCount, allowLogLevel)
}

func GenerateInnerLogger(logFileName, isJsonType, logMaxSize, logFileBackupCount, allowLogLevel string) log.Logger {
	var logger log.Logger

	if logFileName == "" {
		logger = log.NewLogfmtLogger(log.NewSyncWriter(os.Stdout))
		return logger
	} else if logFileName == "stdout" {
		if isJsonType == "true" {
			logger = log.NewLogfmtLogger(initLogFlusher(logFileBackupCount, logMaxSize, logFileName))
		} else {
			logger = log.NewJSONLogger(initLogFlusher(logFileBackupCount, logMaxSize, logFileName))
		}
	} else if logFileName != "stdout" {
		if isJsonType == "true" {
			logger = log.NewJSONLogger(log.NewSyncWriter(os.Stdout))
		} else {
			logger = log.NewLogfmtLogger(log.NewSyncWriter(os.Stdout))
		}
	}
	switch allowLogLevel {
	case "debug":
		logger = level.NewFilter(logger, level.AllowDebug())
	case "info":
		logger = level.NewFilter(logger, level.AllowInfo())
	case "warn":
		logger = level.NewFilter(logger, level.AllowWarn())
	case "error":
		logger = level.NewFilter(logger, level.AllowError())
	default:
		logger = level.NewFilter(logger, level.AllowInfo())
	}

	logger = log.With(logger, "time", log.DefaultTimestampUTC, "caller", log.DefaultCaller)
	return logger
}

func initLogFlusher(logFileBackupCount, logMaxSize, logFileName string) *lumberjack.Logger {
	var newLogMaxSize, newLogFileBackupCount int
	if logMaxSize == "0" {
		newLogMaxSize = 10
	}
	if logFileBackupCount == "0" {
		newLogFileBackupCount = 10
	}
	return &lumberjack.Logger{
		Filename:   logFileName,
		MaxSize:    newLogMaxSize,
		MaxBackups: newLogFileBackupCount,
		Compress:   true,
	}
}
