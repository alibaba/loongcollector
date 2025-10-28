// Copyright 2025 LoongCollector Authors
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

package doris

import (
	"bytes"
	"context"
	"fmt"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/selectdb/go-doris-sdk/pkg/load"
)

// FlusherDoris implements a data flusher that sends logs to Apache Doris via Stream Load.
// It provides efficient buffering and batch processing capabilities to optimize
// the performance of data loading into Doris.
type FlusherDoris struct {
	// Basic connection configuration
	Addresses []string // List of Doris FE addresses in format "host:port"
	Database  string   // Target Doris database name
	// Authentication related configuration
	Authentication Authentication
	// Table name configuration
	Table          string            // Target Doris table name
	LoadProperties map[string]string // Additional Stream Load properties to set in header
	// Progress log interval in seconds, default 10s, set to 0 to disable
	LogProgressInterval int
	// Group commit mode: "sync", "async", or "off" (default: "off")
	GroupCommit string
	// Concurrency controls how many goroutines are used to send data concurrently
	Concurrency int
	// QueueCapacity controls the capacity of the task queue
	QueueCapacity int

	dorisClient *load.DorisLoadClient
	context     pipeline.Context
	converter   *converter.Converter
	Convert     convertConfig

	// Statistics for progress logging
	stats          *statistics
	progressTicker *time.Ticker
	stopChan       chan struct{}
	progressWg     sync.WaitGroup // Separate WaitGroup for progress logging

	// Buffer pool for reusing buffers to reduce memory allocations
	bufferPool sync.Pool

	// Async task queue for concurrent flushing
	queue     chan []*protocol.LogGroup
	counter   sync.WaitGroup
	workersWg sync.WaitGroup // Separate WaitGroup for async workers

	// Ensure Stop() is only called once
	stopOnce sync.Once
}

// statistics holds the metrics for progress logging
type statistics struct {
	startTime       time.Time
	totalBytes      uint64 // atomic
	totalRows       uint64 // atomic
	lastBytes       uint64 // atomic
	lastRows        uint64 // atomic
	lastReportTime  time.Time
	lastReportBytes uint64
	lastReportRows  uint64
	mu              sync.Mutex
}

type convertConfig struct {
	// Rename one or more fields from tags
	TagFieldsRename map[string]string
	// Rename one or more fields, The protocol field options can only be: contents, tags, time
	ProtocolFieldsRename map[string]string
	// Convert protocol, default value: custom_single
	Protocol string
	// Convert encoding, default value: json
	Encoding string
}

type FlusherFunc func(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error

func NewFlusherDoris() *FlusherDoris {
	return &FlusherDoris{
		Addresses: []string{},
		Authentication: Authentication{
			PlainText: &PlainTextConfig{
				Username: "",
				Password: "",
				Database: "",
			},
		},
		Table:               "",
		LogProgressInterval: 10,    // Default 10 seconds
		GroupCommit:         "off", // Default: disable group commit
		Concurrency:         1,     // Default: synchronous (no concurrency)
		QueueCapacity:       1024,  // Default queue capacity
		Convert: convertConfig{
			Protocol: converter.ProtocolCustomSingle,
			Encoding: converter.EncodingJSON,
		},
		stats: &statistics{
			startTime: time.Now(),
		},
		stopChan: make(chan struct{}),
		bufferPool: sync.Pool{
			New: func() interface{} {
				// Pre-allocate buffer with reasonable initial capacity
				// Will grow dynamically as needed
				buf := new(bytes.Buffer)
				buf.Grow(256 * 1024) // 256KB initial capacity (more reasonable default)
				return buf
			},
		},
	}
}

func (f *FlusherDoris) Init(context pipeline.Context) error {
	f.context = context
	// Validate config of flusher
	if err := f.Validate(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris flusher fail, error", err)
		return err
	}
	// Set default value while not set
	if f.Convert.Encoding == "" {
		f.Convert.Encoding = converter.EncodingJSON
	}
	if f.Convert.Protocol == "" {
		f.Convert.Protocol = converter.ProtocolCustomSingle
	}
	// Init converter
	convert, err := f.getConverter()
	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris flusher converter fail, error", err)
		return err
	}
	f.converter = convert

	// Init Doris client
	if err := f.initDorisClient(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris client fail, error", err)
		return err
	}

	// Init async queue and worker pool if concurrency > 1
	if f.Concurrency > 1 {
		if f.QueueCapacity <= 0 {
			f.QueueCapacity = 1024
		}
		f.queue = make(chan []*protocol.LogGroup, f.QueueCapacity)

		// Start worker goroutines
		for i := 0; i < f.Concurrency; i++ {
			f.workersWg.Add(1)
			go f.runFlushWorker()
		}

		logger.Info(f.context.GetRuntimeContext(), "Doris flusher async mode enabled",
			"concurrency", f.Concurrency, "queueCapacity", f.QueueCapacity)
	}

	// Start progress logging if enabled
	if f.LogProgressInterval > 0 {
		f.startProgressLogging()
	}

	return nil
}

func (f *FlusherDoris) getConverter() (*converter.Converter, error) {
	logger.Debug(f.context.GetRuntimeContext(), "[ilogtail data convert config] Protocol", f.Convert.Protocol,
		"Encoding", f.Convert.Encoding, "TagFieldsRename", f.Convert.TagFieldsRename, "ProtocolFieldsRename", f.Convert.ProtocolFieldsRename)
	return converter.NewConverter(f.Convert.Protocol, f.Convert.Encoding, f.Convert.TagFieldsRename, f.Convert.ProtocolFieldsRename, f.context.GetPipelineScopeConfig())
}

func (f *FlusherDoris) Description() string {
	return "Doris flusher for logtail"
}

// parseGroupCommitMode converts string to GroupCommitMode
func parseGroupCommitMode(mode string) load.GroupCommitMode {
	switch strings.ToLower(mode) {
	case "sync":
		return load.SYNC
	case "async":
		return load.ASYNC
	case "off", "":
		return load.OFF
	default:
		logger.Warningf(context.Background(), "Unknown group commit mode: %s, using 'off'", mode)
		return load.OFF
	}
}

// initDorisClient initializes the Doris Stream Load client
func (f *FlusherDoris) initDorisClient() error {
	// Get authentication credentials
	username, password, err := f.Authentication.GetUsernamePassword()
	if err != nil {
		return fmt.Errorf("failed to get authentication credentials: %w", err)
	}

	// Create Doris SDK configuration
	config := &load.Config{
		Endpoints:   f.Addresses,
		User:        username,
		Password:    password,
		Database:    f.Database,
		Table:       f.Table,
		Format:      load.DefaultJSONFormat(),
		Retry:       load.DefaultRetry(),
		GroupCommit: parseGroupCommitMode(f.GroupCommit),
		LabelPrefix: "LoongCollector_doris_flusher",
		Options:     f.LoadProperties,
	}

	// Create Doris client
	client, err := load.NewLoadClient(config)
	if err != nil {
		return fmt.Errorf("failed to create doris client: %w", err)
	}

	f.dorisClient = client
	logger.Infof(f.context.GetRuntimeContext(), "Doris client initialized successfully, endpoints: %v, database: %s, table: %s",
		f.Addresses, f.Database, f.Table)

	return nil
}

func (f *FlusherDoris) Validate() error {
	if len(f.Addresses) == 0 {
		var err = fmt.Errorf("doris addrs is nil")
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris flusher error", err)
		return err
	}
	if f.Table == "" {
		var err = fmt.Errorf("doris table is nil")
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris flusher error", err)
		return err
	}
	return nil
}

func (f *FlusherDoris) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	if f.dorisClient == nil {
		return fmt.Errorf("doris client not initialized")
	}

	if len(logGroupList) == 0 {
		return nil
	}

	// Async mode: add task to queue and return immediately
	if f.Concurrency > 1 {
		return f.addTask(logGroupList)
	}

	// Sync mode: process immediately
	return f.flushSync(logGroupList)
}

// addTask adds a flush task to the queue for async processing
// This method will BLOCK if the queue is full, ensuring NO DATA LOSS
func (f *FlusherDoris) addTask(logGroupList []*protocol.LogGroup) error {
	f.counter.Add(1)

	// First, try non-blocking send to detect queue congestion
	select {
	case f.queue <- logGroupList:
		// Successfully sent without blocking
		return nil
	default:
		// Queue is full, log warning and then block
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_QUEUE_FULL",
			"doris flusher queue is full, blocking until space available",
			"queueCapacity", f.QueueCapacity,
			"concurrency", f.Concurrency,
			"suggestion", "consider increasing Concurrency or QueueCapacity")

		// Now block until queue has space - NEVER drop data
		// This creates backpressure to upstream components when system is overloaded
		f.queue <- logGroupList
		return nil
	}
}

// runFlushWorker is the worker goroutine that processes flush tasks from the queue
func (f *FlusherDoris) runFlushWorker() {
	defer f.workersWg.Done()

	for logGroupList := range f.queue {
		err := f.flushSync(logGroupList)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM",
				"worker failed to flush data to doris, error", err)
		}
		f.counter.Done()
	}
}

// flushSync performs synchronous flush operation
func (f *FlusherDoris) flushSync(logGroupList []*protocol.LogGroup) error {
	// Get buffer from pool to reduce allocations
	buffer := f.bufferPool.Get().(*bytes.Buffer)
	buffer.Reset() // Reset buffer for reuse
	defer func() {
		// Shrink buffer if it's too large to prevent memory waste
		// Keep buffers under 10MB to balance between performance and memory
		if buffer.Cap() > 10*1024*1024 {
			buffer = nil // Let GC reclaim large buffers
		}
		if buffer != nil {
			f.bufferPool.Put(buffer)
		}
	}()

	totalLogCount := 0

	// Merge all LogGroups into a single batch
	for _, logGroup := range logGroupList {
		logger.Debug(f.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)

		// Convert log group to byte stream
		serializedLogs, err := f.converter.ToByteStream(logGroup)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush doris convert log fail, error", err)
			continue
		}

		// Append all logs to the same buffer
		for _, log := range serializedLogs.([][]byte) {
			buffer.Write(log)
			buffer.WriteByte('\n') // Add newline separator for JSON object line format
			totalLogCount++
		}
	}

	if buffer.Len() == 0 {
		logger.Debug(f.context.GetRuntimeContext(), "No logs to flush")
		return nil
	}

	// Create a bytes.Reader from buffer data to support seeking
	// bytes.Reader supports io.Seeker, so SDK won't buffer internally
	dataToLoad := buffer.Bytes()
	reader := bytes.NewReader(dataToLoad)

	response, err := f.dorisClient.Load(reader)

	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush doris load fail, error", err)
		return fmt.Errorf("failed to load data to doris: %w", err)
	}

	if response.Status == load.SUCCESS {
		logger.Infof(f.context.GetRuntimeContext(), "Doris load success, loadedRows: %d, loadBytes: %d, loadTimeMs: %d, label: %s",
			response.Resp.NumberLoadedRows,
			response.Resp.LoadBytes,
			response.Resp.LoadTimeMs,
			response.Resp.Label)

		// Update statistics
		f.updateStatistics(uint64(response.Resp.LoadBytes), uint64(response.Resp.NumberLoadedRows))
	} else {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM",
			"doris load failed with status", response.Status,
			"message", response.ErrorMessage)
		return fmt.Errorf("doris load failed: %s", response.ErrorMessage)
	}

	return nil
}

func (f *FlusherDoris) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return f.dorisClient != nil
}

func (f *FlusherDoris) SetUrgent(flag bool) {}

func (f *FlusherDoris) Stop() error {
	// Ensure Stop() is only executed once to avoid panic from closing channels twice
	f.stopOnce.Do(func() {
		// Stop progress logging first
		if f.progressTicker != nil {
			close(f.stopChan)
			f.progressTicker.Stop()
			f.progressWg.Wait() // Wait for progress logging goroutine to exit
			logger.Debug(f.context.GetRuntimeContext(), "Doris flusher progress logging stopped")
		}

		// Stop async workers if running
		if f.Concurrency > 1 && f.queue != nil {
			// Wait for all pending tasks to be added
			f.counter.Wait()

			// Close queue to signal workers to exit
			close(f.queue)

			// Wait for all workers to finish
			f.workersWg.Wait()

			logger.Info(f.context.GetRuntimeContext(), "Doris flusher async workers stopped")
		}
	})

	return nil
}

// startProgressLogging starts a goroutine that periodically logs progress statistics
func (f *FlusherDoris) startProgressLogging() {
	f.progressTicker = time.NewTicker(time.Duration(f.LogProgressInterval) * time.Second)
	f.progressWg.Add(1)

	go func() {
		defer f.progressWg.Done()
		for {
			select {
			case <-f.progressTicker.C:
				f.logProgress()
			case <-f.stopChan:
				return
			}
		}
	}()
}

// updateStatistics updates the statistics with new load results
func (f *FlusherDoris) updateStatistics(bytes, rows uint64) {
	atomic.AddUint64(&f.stats.totalBytes, bytes)
	atomic.AddUint64(&f.stats.totalRows, rows)
	atomic.AddUint64(&f.stats.lastBytes, bytes)
	atomic.AddUint64(&f.stats.lastRows, rows)
}

// logProgress logs the current progress statistics
func (f *FlusherDoris) logProgress() {
	f.stats.mu.Lock()
	defer f.stats.mu.Unlock()

	now := time.Now()
	totalBytes := atomic.LoadUint64(&f.stats.totalBytes)
	totalRows := atomic.LoadUint64(&f.stats.totalRows)

	// Calculate total elapsed time since start
	totalElapsed := now.Sub(f.stats.startTime).Seconds()
	if totalElapsed == 0 {
		totalElapsed = 1
	}

	// Calculate total speed
	totalMB := float64(totalBytes) / 1024 / 1024
	totalSpeedMBps := totalMB / totalElapsed
	totalSpeedRps := float64(totalRows) / totalElapsed

	// Calculate speed since last report
	lastBytes := atomic.SwapUint64(&f.stats.lastBytes, 0)
	lastRows := atomic.SwapUint64(&f.stats.lastRows, 0)

	intervalElapsed := float64(f.LogProgressInterval)
	if !f.stats.lastReportTime.IsZero() {
		intervalElapsed = now.Sub(f.stats.lastReportTime).Seconds()
	}
	if intervalElapsed == 0 {
		intervalElapsed = 1
	}

	lastMB := float64(lastBytes) / 1024 / 1024
	lastSpeedMBps := lastMB / intervalElapsed
	lastSpeedRps := float64(lastRows) / intervalElapsed

	f.stats.lastReportTime = now
	f.stats.lastReportBytes = totalBytes
	f.stats.lastReportRows = totalRows

	// Format: total 11 MB 18978 ROWS, total speed 0 MB/s 632 R/s, last 10 seconds speed 1 MB/s 1897 R/s
	logger.Info(f.context.GetRuntimeContext(),
		fmt.Sprintf("total %.0f MB %d ROWS, total speed %.0f MB/s %.0f R/s, last %d seconds speed %.0f MB/s %.0f R/s",
			totalMB, totalRows,
			totalSpeedMBps, totalSpeedRps,
			f.LogProgressInterval,
			lastSpeedMBps, lastSpeedRps))
}

// Register the plugin to the Flushers array.
func init() {
	pipeline.Flushers["flusher_doris"] = func() pipeline.Flusher {
		f := NewFlusherDoris()
		return f
	}
}
