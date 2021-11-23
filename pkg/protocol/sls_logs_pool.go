package protocol

// import "sync"

// var slsLogPool sync.Pool
// var slsLogGroupPool sync.Pool

// // var slsLogContentPool sync.Pool

// // // GetSLSLogContent return a LogContent
// // func GetSLSLogContent() *Log_Content {
// // 	return slsLogContentPool.Get().(*Log_Content)
// // }

// // // CacheSLSLogContent( cache a LogContent
// // func CacheSLSLogContent(content *Log_Content) {
// // 	content.Key = ""
// // 	content.Value = ""
// // 	slsLogContentPool.Put(content)
// // }

// // GetSLSLog return a Log
// func GetSLSLog() *Log {
// 	return &Log{}
// 	return slsLogPool.Get().(*Log)
// }

// // CacheSLSLog cache a Log
// func CacheSLSLog(log *Log) {
// 	return
// 	// for _, logContents := range log.Contents {
// 	// 	CacheSLSLogContent(logContents)
// 	// }
// 	// reset log contents
// 	log.Contents = nil
// 	slsLogPool.Put(log)
// }

// // CopySLSLog deep copy for Log
// func CopySLSLog(log *Log) *Log {
// 	newLog := GetSLSLog()
// 	newLog.Time = log.Time
// 	// @todo, need to deep copy Contents
// 	newLog.Contents = log.Contents
// 	return newLog
// }

// // GetSLSLogGroup return a LogGroup
// func GetSLSLogGroup() *LogGroup {
// 	return &LogGroup{}
// 	return slsLogGroupPool.Get().(*LogGroup)
// }

// // CacheSLSLogGroup cache a LogGroup
// func CacheSLSLogGroup(logGroup *LogGroup) {
// 	return
// 	for _, log := range logGroup.Logs {
// 		CacheSLSLog(log)
// 	}
// 	// reset logs and tags
// 	logGroup.LogTags = nil
// 	logGroup.Logs = nil
// 	slsLogGroupPool.Put(logGroup)
// }

// // CopySLSLogGroup deep copy for LogGroup
// func CopySLSLogGroup(logGroup *LogGroup) *LogGroup {
// 	newLogGroup := GetSLSLogGroup()
// 	newLogGroup.Topic = logGroup.Topic
// 	// @todo, need to deep copy Tags
// 	newLogGroup.LogTags = logGroup.LogTags
// 	for _, log := range logGroup.Logs {
// 		newLogGroup.Logs = append(newLogGroup.Logs, CopySLSLog(log))
// 	}
// 	return newLogGroup
// }

// func init() {
// 	slsLogPool.New = func() interface{} {
// 		return &Log{}
// 	}
// 	slsLogGroupPool.New = func() interface{} {
// 		return &LogGroup{}
// 	}
// 	// slsLogContentPool.New = func() interface{} {
// 	// 	return &Log_Content{}
// 	// }
// }
