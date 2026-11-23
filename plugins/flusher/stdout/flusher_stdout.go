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

package stdout

import (
	"fmt"
	"sync/atomic"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"

	"github.com/cihub/seelog"
	jsoniter "github.com/json-iterator/go"
)

const flushMsg = `
<seelog minlevel="info" >
<outputs formatid="common">
	 %s
 </outputs>
 <formats>
	 <format id="common" format="%s %%Msg%%n" />
 </formats>
</seelog>
`

// FlusherStdout writes events to stdout, a configured file or the global log.
type FlusherStdout struct {
	FileName      string
	MaxSize       int
	MaxRolls      int
	KeyValuePairs bool
	Tags          bool
	OnlyStdout    bool

	context   pipeline.Context
	outLogger seelog.LoggerInterface
	file      *stdoutFileOutput
}

// Init method would be trigger before working. For the plugin, init method choose the log output
// channel.
func (p *FlusherStdout) Init(context pipeline.Context) error {
	p.context = context

	pattern := ""
	if p.OnlyStdout {
		pattern = "<console/>"
		logger.CloseCatchStdout()
	} else if p.FileName != "" {
		if p.MaxSize <= 0 {
			p.MaxSize = 1024 * 1024
		}
		if p.MaxRolls <= 0 {
			p.MaxRolls = 1
		}
		file, err := newStdoutFileOutput(p.FileName, p.MaxSize, p.MaxRolls, p.reportWriteError)
		if err != nil {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.FlusherInitAlarm, "init stdout flusher fail, error", err)
			p.outLogger = seelog.Disabled
		} else {
			p.file = file
		}
	}
	if pattern != "" {
		var err error
		p.outLogger, err = seelog.LoggerFromConfigAsString(fmt.Sprintf(flushMsg, pattern, stdoutTimeFormat))
		if err != nil {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.FlusherInitAlarm, "init stdout flusher fail, error", err)
			p.outLogger = seelog.Disabled
		}
	}
	return nil
}

func (*FlusherStdout) Description() string {
	return "stdout flusher for logtail"
}

func (p *FlusherStdout) Export(in []*models.PipelineGroupEvents, context pipeline.PipelineContext) error {
	writer := jsoniter.ConfigDefault.BorrowStream(nil)
	defer func() {
		if p.file != nil || cap(writer.Buffer()) > maxStdoutBatchBytes {
			writer.SetBuffer(nil)
		}
		jsoniter.ConfigDefault.ReturnStream(writer)
	}()
	var batch *stdoutFileBatch
	if p.file != nil {
		batch = p.file.getBatch()
		defer func() { p.file.enqueue(batch) }()
	}

	for _, groupEvents := range in {
		if p.Tags {
			metadata := fmt.Sprintf("%v", groupEvents.Group.GetMetadata().Iterator())
			tags := fmt.Sprintf("%v", groupEvents.Group.GetTags().Iterator())
			if batch != nil {
				message := fmt.Sprintf("[Event] event %d, metadata %s, tags %s", len(groupEvents.Events), metadata, tags)
				start := batch.beginLine()
				batch.data = append(batch.data, message...)
				batch = p.file.finishLine(batch, start)
			} else if p.outLogger != nil {
				p.outLogger.Infof("[Event] event %d, metadata %s, tags %s", len(groupEvents.Events), metadata, tags)
			} else {
				logger.Info(p.context.GetRuntimeContext(), "[Event] event", len(groupEvents.Events), "metadata", metadata, "tags", tags)
			}
		}

		for _, event := range groupEvents.Events {
			writer.Reset(nil)
			lineStart := 0
			if batch != nil {
				lineStart = batch.beginLine()
				writer.SetBuffer(batch.data)
			}
			writer.WriteObjectStart()
			writer.WriteObjectField("eventType")
			eventType := event.GetType()
			switch eventType {
			case models.EventTypeMetric:
				writer.WriteString("metric")
			case models.EventTypeSpan:
				writer.WriteString("span")
			case models.EventTypeLogging:
				writer.WriteString("log")
			case models.EventTypeByteArray:
				writer.WriteString("byteArray")
			}
			writer.WriteMore()
			writer.WriteObjectField("name")
			writer.WriteString(event.GetName())
			writer.WriteMore()
			writer.WriteObjectField("timestamp")
			writer.WriteUint64(event.GetTimestamp())
			writer.WriteMore()
			writer.WriteObjectField("observedTimestamp")
			writer.WriteUint64(event.GetObservedTimestamp())
			writer.WriteMore()
			writer.WriteObjectField("tags")
			writer.WriteObjectStart()
			firstTag := true
			if tags := event.GetTags(); tags != nil && tags.Len() > 0 {
				for k, v := range tags.Iterator() {
					if !firstTag {
						writer.WriteMore()
					}
					firstTag = false
					writer.WriteObjectField(k)
					writer.WriteString(v)
				}
			}
			writer.WriteObjectEnd()
			writer.WriteMore()
			switch eventType {
			case models.EventTypeMetric:
				p.writeMetricValues(writer, event.(*models.Metric))
			case models.EventTypeSpan:
				p.writeSpan(writer, nil)
			case models.EventTypeLogging:
				p.writeLogBody(writer, event.(*models.Log))
			case models.EventTypeByteArray:
				p.writeByteArray(writer, event.(models.ByteArray))
			}

			writer.WriteObjectEnd()

			if batch != nil {
				batch.data = writer.Buffer()
				writer.SetBuffer(nil)
				batch = p.file.finishLine(batch, lineStart)
				continue
			}
			// The logger may retain the message after Export reuses the buffer.
			message := string(writer.Buffer())
			if p.outLogger != nil {
				p.outLogger.Info(message)
			} else {
				logger.Info(p.context.GetRuntimeContext(), message)
			}
		}
	}
	return nil
}

func (p *FlusherStdout) writeMetricValues(writer *jsoniter.Stream, metric *models.Metric) {
	writer.WriteObjectField("metricType")
	writer.WriteString(models.MetricTypeTexts[metric.GetMetricType()])
	_, _ = writer.Write([]byte{','})
	if metric.GetValue().IsSingleValue() {
		writer.WriteObjectField("value")
		writer.WriteFloat64(metric.GetValue().GetSingleValue())
	} else {
		writer.WriteObjectField("values")
		writer.WriteObjectStart()
		values := metric.GetValue().GetMultiValues()
		i := 0
		for k, v := range values.Iterator() {
			writer.WriteObjectField(k)
			writer.WriteFloat64(v)
			if i < values.Len()-1 {
				_, _ = writer.Write([]byte{','})
			}
			i++
		}
		if metric.GetTypedValue().Len() > 0 {
			_, _ = writer.Write([]byte{','})
			i = 0
			for k, v := range metric.GetTypedValue().Iterator() {
				writer.WriteObjectField(k)
				switch v.Type {
				case models.ValueTypeString:
					writer.WriteString(v.Value.(string))
				case models.ValueTypeBoolean:
					writer.WriteBool(v.Value.(bool))
				}
				if i < metric.GetTypedValue().Len()-1 {
					_, _ = writer.Write([]byte{','})
				}
				i++
			}
		}
		writer.WriteObjectEnd()
	}
}

func (p *FlusherStdout) writeSpan(writer *jsoniter.Stream, metric *models.Span) {
	// TODO
}

func (p *FlusherStdout) writeLogBody(writer *jsoniter.Stream, log *models.Log) {
	writer.WriteObjectField("offset")
	writer.WriteInt64(int64(log.GetOffset()))
	writer.WriteMore()
	writer.WriteObjectField("level")
	writer.WriteString(log.GetLevel())
	writer.WriteMore()
	writer.WriteObjectField("traceID")
	writer.WriteString(log.GetTraceID())
	writer.WriteMore()
	writer.WriteObjectField("spanID")
	writer.WriteString(log.GetSpanID())
	contents := log.GetIndices()
	for key, value := range models.LogContentEntries(contents) {
		writer.WriteMore()
		writer.WriteObjectField(key)
		if field, ok := value.(string); ok {
			writer.WriteString(field)
		} else {
			writer.WriteRaw(fmt.Sprintf("%#v", value))
		}
	}
}

func (p FlusherStdout) writeByteArray(writer *jsoniter.Stream, bytes models.ByteArray) {
	writer.WriteObjectField("byteArray")
	_, _ = writer.Write([]byte{'"'})
	_, _ = writer.Write(bytes)
	_, _ = writer.Write([]byte{'"'})
}

func (p *FlusherStdout) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (*FlusherStdout) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return true
}

// Stop ...
func (p *FlusherStdout) Stop() error {
	if p.file != nil {
		p.file.close()
	}
	if p.outLogger != nil {
		p.outLogger.Close()
	}
	return nil
}

func (p *FlusherStdout) reportWriteError(err error) {
	logger.Warning(p.context.GetRuntimeContext(), selfmonitor.PluginAlarm, "stdout file write failed", err)
}

const stdoutTimestampFormatter = "LoongCollectorStdoutTimestamp"

var stdoutTimeFormat = "%Date %Time"

type timestampCacheEntry struct {
	second   int64
	location *time.Location
	text     string
	boxed    any
}

// newStdoutTimestampFormatter caches the formatted timestamp per logger.
// The prefix has one-second precision and depends on the time's location.
func newStdoutTimestampFormatter(_ string) seelog.FormatterFunc {
	var cached atomic.Pointer[timestampCacheEntry]
	return func(_ string, _ seelog.LogLevel, ctx seelog.LogContextInterface) any {
		return stdoutTimestamp(&cached, ctx.CallTime()).boxed
	}
}

func stdoutTimestamp(cache *atomic.Pointer[timestampCacheEntry], now time.Time) *timestampCacheEntry {
	second, location := now.Unix(), now.Location()
	if entry := cache.Load(); entry != nil && entry.second == second && entry.location == location {
		return entry
	}
	text := now.Format("2006-01-02 15:04:05")
	entry := &timestampCacheEntry{second: second, location: location, text: text, boxed: text}
	cache.Store(entry)
	return entry
}

// Register the plugin to the Flushers array.
func init() {
	if err := seelog.RegisterCustomFormatter(stdoutTimestampFormatter, newStdoutTimestampFormatter); err == nil {
		stdoutTimeFormat = "%" + stdoutTimestampFormatter
	}
	pipeline.Flushers["flusher_stdout"] = func() pipeline.Flusher {
		return &FlusherStdout{
			KeyValuePairs: true,
		}
	}
}
