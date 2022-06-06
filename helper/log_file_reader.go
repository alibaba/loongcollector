// Copyright 2021 iLogtail Authors
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

package helper

import (
	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	"context"
	"io"
	"os"
	"sync"
	"time"
)

type ReaderMetricTracker struct {
	OpenCounter        ilogtail.CounterMetric
	CloseCounter       ilogtail.CounterMetric
	FileSizeCounter    ilogtail.CounterMetric
	FileRotatorCounter ilogtail.CounterMetric
	ReadCounter        ilogtail.CounterMetric
	ReadSizeCounter    ilogtail.CounterMetric
	ProcessLatency     ilogtail.LatencyMetric
}

func NewReaderMetricTracker() *ReaderMetricTracker {
	return &ReaderMetricTracker{
		OpenCounter:        NewCounterMetric("open_count"),
		CloseCounter:       NewCounterMetric("close_count"),
		FileSizeCounter:    NewCounterMetric("file_size"),
		FileRotatorCounter: NewCounterMetric("file_rotate"),
		ReadCounter:        NewCounterMetric("read_count"),
		ReadSizeCounter:    NewCounterMetric("read_size"),
		ProcessLatency:     NewLatencyMetric("log_process_latency"),
	}
}

type LogFileReaderConfig struct {
	ReadIntervalMs   int
	MaxReadBlockSize int
	CloseFileSec     int
	Tracker          *ReaderMetricTracker
}

var DefaultLogFileReaderConfig = LogFileReaderConfig{
	ReadIntervalMs:   1000,
	MaxReadBlockSize: 512 * 1024,
	CloseFileSec:     60,
	Tracker:          nil,
}

type LogFileReaderCheckPoint struct {
	Path   string
	Offset int64
	State  StateOS
}

// IsSame check if the checkpoints is same
func (checkpoint *LogFileReaderCheckPoint) IsSame(other *LogFileReaderCheckPoint) bool {
	if checkpoint.Path != other.Path || checkpoint.Offset != other.Offset {
		return false
	}
	return !checkpoint.State.IsChange(other.State)
}

type LogFileReader struct {
	Config LogFileReaderConfig

	file           *os.File
	lastCheckpoint LogFileReaderCheckPoint
	checkpoint     LogFileReaderCheckPoint
	processor      LogFileProcessor
	nowBlock       []byte
	lastBufferSize int
	lastBufferTime time.Time
	readWhenStart  bool
	checkpointLock sync.Mutex
	shutdown       chan struct{}
	logContext     context.Context
	waitgroup      sync.WaitGroup
	foundFile      bool
}

func NewLogFileReader(context context.Context, checkpoint LogFileReaderCheckPoint, config LogFileReaderConfig, processor LogFileProcessor) (*LogFileReader, error) {
	readWhenStart := false
	foundFile := true
	if checkpoint.State.IsEmpty() {
		if newStat, err := os.Stat(checkpoint.Path); err == nil {
			checkpoint.State = GetOSState(newStat)
		} else {
			if os.IsNotExist(err) {
				foundFile = false
			}
			logger.Warning(context, "STAT_FILE_ALARM", "stat file error when create reader, file", checkpoint.Path, "error", err.Error())
		}
	}
	if !checkpoint.State.IsEmpty() {
		if deltaNano := time.Now().UnixNano() - int64(checkpoint.State.ModifyTime); deltaNano >= 0 && deltaNano < 180*1e9 {
			readWhenStart = true
			logger.Warning(context, "first read this file, need read when start flag", readWhenStart)
		}
	}
	return &LogFileReader{
		checkpoint:     checkpoint,
		Config:         config,
		processor:      processor,
		logContext:     context,
		lastBufferSize: 0,
		nowBlock:       nil,
		readWhenStart:  readWhenStart,
		foundFile:      foundFile,
	}, nil
}

// GetLastEndOfLine return new read bytes end with '\n'
// @note will return n + r.lastBufferSize when n + r.lastBufferSize == len(r.nowBlock)
func (r *LogFileReader) GetLastEndOfLine(n int) int {
	blockSize := n + r.lastBufferSize
	// if block size >= r.Config.MaxReadBlockSize, return n
	if blockSize == len(r.nowBlock) {
		return n
	}
	for i := blockSize - 1; i >= r.lastBufferSize; i-- {
		if r.nowBlock[i] == '\n' {
			return i - r.lastBufferSize + 1
		}
	}
	return 0
}

func (r *LogFileReader) CheckFileChange() bool {
	if newStat, err := os.Stat(r.checkpoint.Path); err == nil {
		newOsStat := GetOSState(newStat)
		// logger.Debug("check file change", newOsStat.String())
		if r.checkpoint.State.IsChange(newOsStat) {
			needResetOffset := false
			if r.checkpoint.State.IsFileChange(newOsStat) {
				needResetOffset = true
				logger.Info(r.logContext, "file dev inode changed, read to end and force read from beginning, file", r.checkpoint.Path, "old", r.checkpoint.State.String(), "new", newOsStat.String(), "offset", r.checkpoint.Offset)
				// read to end or shutdown
				if r.file != nil {
					r.ReadAndProcess(false)
					r.CloseFile("open file and dev inode changed")
				}
				if r.Config.Tracker != nil {
					r.Config.Tracker.FileRotatorCounter.Add(1)
				}
				// if file change, force flush last buffer
				if r.lastBufferSize > 0 {
					processSize := r.processor.Process(r.nowBlock[0:r.lastBufferSize], time.Hour)
					r.UpdateProcessResult(0, processSize)
				}
			}
			r.checkpointLock.Lock()
			r.checkpoint.State = newOsStat
			if needResetOffset {
				r.checkpoint.Offset = 0
			}
			r.checkpointLock.Unlock()
			return true
		}
		r.foundFile = true
	} else {
		if os.IsNotExist(err) {
			if r.foundFile {
				logger.Warning(r.logContext, "STAT_FILE_ALARM", "stat file error, file", r.checkpoint.Path, "error", err.Error())
				r.foundFile = false
			}
		} else {
			logger.Warning(r.logContext, "STAT_FILE_ALARM", "stat file error, file", r.checkpoint.Path, "error", err.Error())
		}

	}
	return false
}

func (r *LogFileReader) GetProcessor() LogFileProcessor {
	return r.processor
}

func (r *LogFileReader) GetCheckpoint() (checkpoint LogFileReaderCheckPoint, updateFlag bool) {
	r.checkpointLock.Lock()
	defer func() {
		r.lastCheckpoint = r.checkpoint
		r.checkpointLock.Unlock()
	}()
	r.lastCheckpoint = r.checkpoint
	return r.checkpoint, r.lastCheckpoint.IsSame(&r.checkpoint)
}

func (r *LogFileReader) UpdateProcessResult(readN, processedN int) {
	if readN+r.lastBufferSize == processedN {
		r.lastBufferSize = 0
		r.lastBufferTime = time.Now()
	} else {
		if processedN != 0 {
			// need move buffer
			copy(r.nowBlock, r.nowBlock[processedN:readN+r.lastBufferSize])
			r.lastBufferTime = time.Now()
		}
		r.lastBufferSize = readN + r.lastBufferSize - processedN
	}
	r.checkpointLock.Lock()
	defer r.checkpointLock.Unlock()
	r.checkpoint.Offset += int64(processedN)
}

func (r *LogFileReader) ProcessAfterRead(n int) {
	// if no more file, check and process last buffer
	if n == 0 {
		if r.lastBufferSize > 0 {
			// logger.Debug("no more file, check and process last buffer", string(r.nowBlock[0:r.lastBufferSize]))
			processSize := r.processor.Process(r.nowBlock[0:r.lastBufferSize], time.Since(r.lastBufferTime))
			if processSize > n+r.lastBufferSize {
				processSize = n + r.lastBufferSize
			}
			r.UpdateProcessResult(n, processSize)
		} else {
			// if no data, just call process and give a empty []byte array
			r.processor.Process(r.nowBlock[0:0], time.Since(r.lastBufferTime))
		}
	} else {
		processSize := r.processor.Process(r.nowBlock[0:r.lastBufferSize+n], time.Duration(0))
		if processSize > n+r.lastBufferSize {
			processSize = n + r.lastBufferSize
		}
		// logger.Debug("process file, len", r.lastBufferSize+n, "processed size", processSize)
		r.UpdateProcessResult(n, processSize)
	}
}

func (r *LogFileReader) ReadOpen() error {
	if r.file == nil {
		var err error
		r.file, err = ReadOpen(r.checkpoint.Path)
		if r.Config.Tracker != nil {
			r.Config.Tracker.OpenCounter.Add(1)
		}
		logger.Debug(r.logContext, "open file for read, file", r.checkpoint.Path, "offset", r.checkpoint.Offset, "status", r.checkpoint.State)
		return err
	}
	return nil
}

func (r *LogFileReader) ReadAndProcess(once bool) {
	if r.nowBlock == nil {
		// once only be true when shutdown, and we don't need to init r.nowBlock when shutdown because this file is never readed
		if once {
			return
		}
		// lazy init
		r.nowBlock = make([]byte, r.Config.MaxReadBlockSize)
	}
	if err := r.ReadOpen(); err == nil {
		file := r.file
		// double check
		if newStat, statErr := file.Stat(); statErr == nil {
			newOsStat := GetOSState(newStat)

			// logger.Debug("check file dev inode, file", r.checkpoint.Path, "old", r.checkpoint.State.String(), "new", newOsStat.String(), "offset", r.checkpoint.Offset)

			// check file dev+inode changed
			if r.checkpoint.State.IsFileChange(newOsStat) {
				logger.Info(r.logContext, "file dev inode changed, force read from beginning, file", r.checkpoint.Path, "old", r.checkpoint.State.String(), "new", newOsStat.String(), "offset", r.checkpoint.Offset)
				// if file dev+inode changed, force flush last buffer
				if r.lastBufferSize > 0 {
					processSize := r.processor.Process(r.nowBlock[0:r.lastBufferSize], time.Hour)
					r.UpdateProcessResult(0, processSize)
				}
				if r.Config.Tracker != nil {
					r.Config.Tracker.FileRotatorCounter.Add(1)
				}
				r.checkpointLock.Lock()
				r.checkpoint.Offset = 0
				r.checkpoint.State = newOsStat
				r.checkpointLock.Unlock()
				r.CloseFile("file changed(rotate)")
				return
			}

			// check file truncated
			if newOsStat.Size < r.checkpoint.Offset {
				logger.Info(r.logContext, "file truncated, force read from beginning, file", r.checkpoint.Path, "old", r.checkpoint.State.String(), "new", newOsStat.String(), "offset", r.checkpoint.Offset)
				// if file dev+inode changed, force flush last buffer
				if r.lastBufferSize > 0 {
					processSize := r.processor.Process(r.nowBlock[0:r.lastBufferSize], time.Hour)
					r.UpdateProcessResult(0, processSize)
				}
				if r.Config.Tracker != nil {
					r.Config.Tracker.FileRotatorCounter.Add(1)
				}
				r.checkpointLock.Lock()
				if newOsStat.Size < 10*1024*1024 {
					r.checkpoint.Offset = 0
				} else {
					r.checkpoint.Offset = newOsStat.Size - 1024*1024
				}
				r.checkpoint.State = newOsStat
				r.checkpointLock.Unlock()
				r.CloseFile("file changed(truncate)")
				return
			}
		} else {
			logger.Warning(r.logContext, "STAT_FILE_ALARM", "stat file error, file", r.checkpoint.Path, "error", statErr.Error())
		}
		for {
			n, readErr := file.ReadAt(r.nowBlock[r.lastBufferSize:], int64(r.lastBufferSize)+r.checkpoint.Offset)
			needBreak := false
			if r.Config.Tracker != nil {
				r.Config.Tracker.ReadCounter.Add(1)
				r.Config.Tracker.ReadSizeCounter.Add(int64(n))
			}
			if once || n < r.Config.MaxReadBlockSize-r.lastBufferSize {
				needBreak = true
			}
			if readErr != nil {
				if readErr != io.EOF {
					logger.Warning(r.logContext, "READ_FILE_ALARM", "read file error, file", r.checkpoint.Path, "error", readErr.Error())
					break
				}
				logger.Debug(r.logContext, "read end of file", r.checkpoint.Path, "offset", r.checkpoint.Offset, "last buffer size", r.lastBufferSize, "read n", n, "stat", r.checkpoint.State.String())
				needBreak = true
			}
			// only accept buffer end of '\n'
			n = r.GetLastEndOfLine(n)
			// logger.Debug("after check last end of line, n", n)
			r.ProcessAfterRead(n)
			if !needBreak {
				// check shutdown
				select {
				case <-r.shutdown:
					logger.Info(r.logContext, "receive stop signal when read data, path", r.checkpoint.Path)
					needBreak = true
				default:
				}
			}
			if needBreak {
				break
			}
		}
	} else {
		logger.Warning(r.logContext, "READ_FILE_ALARM", "open file for read error, file", r.checkpoint.Path, "error", err.Error())
	}
}

func (r *LogFileReader) CloseFile(reason string) {
	if r.file != nil {
		_ = r.file.Close()
		r.file = nil
		if r.Config.Tracker != nil {
			r.Config.Tracker.CloseCounter.Add(1)
		}
		logger.Debug(r.logContext, "close file, reason", reason, "file", r.checkpoint.Path, "offset", r.checkpoint.Offset, "status", r.checkpoint.State)
	}
}

func (r *LogFileReader) Run() {
	defer func() {
		r.CloseFile("run done")
		r.waitgroup.Done()
		panicRecover(r.logContext, r.checkpoint.Path)
	}()
	lastReadTime := time.Now()
	tracker := r.Config.Tracker
	for {
		startProcessTime := time.Now()
		if tracker != nil {
			tracker.ProcessLatency.Begin()
		}
		if r.readWhenStart || r.CheckFileChange() {
			r.readWhenStart = false
			r.ReadAndProcess(false)
			lastReadTime = startProcessTime
		} else {
			r.ProcessAfterRead(0)
			if startProcessTime.Sub(lastReadTime) > time.Second*time.Duration(r.Config.CloseFileSec) {
				r.CloseFile("no read timeout")
				lastReadTime = startProcessTime
			}
		}
		if tracker != nil {
			tracker.ProcessLatency.End()
		}
		endProcessTime := time.Now()
		sleepDuration := time.Millisecond*time.Duration(r.Config.ReadIntervalMs) - endProcessTime.Sub(startProcessTime)
		logger.Debug(r.logContext, "sleep duration", sleepDuration, "normal", r.Config.ReadIntervalMs)
		if util.RandomSleep(sleepDuration, 0.1, r.shutdown) {
			r.ReadAndProcess(true)
			if r.lastBufferSize > 0 {
				processSize := r.processor.Process(r.nowBlock[0:r.lastBufferSize], time.Hour)
				r.UpdateProcessResult(0, processSize)
			}
			// [to #37527076] force flush last log buffer in processor
			r.processor.Process(r.nowBlock[0:0], time.Hour)
			break
		}
	}
}

// SetForceRead force read file when reader start
func (r *LogFileReader) SetForceRead() {
	r.readWhenStart = true
}

func (r *LogFileReader) Start() {
	r.shutdown = make(chan struct{})
	r.waitgroup.Add(1)
	go r.Run()
}

func (r *LogFileReader) Stop() {
	close(r.shutdown)
	r.waitgroup.Wait()
}
