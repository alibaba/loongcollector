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
	"encoding/json"
	"fmt"
	"strconv"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/cihub/seelog"
	jsoniter "github.com/json-iterator/go"
)

const flushMsg = `
<seelog minlevel="info" >
<outputs formatid="common">
	 %s
 </outputs>
 <formats>
	 <format id="common" format="%%Date %%Time %%Msg%%n" />
 </formats>
</seelog>
`

// FlusherStdout is a flusher plugin in plugin system.
// It has two usages:
// 1. flusher the logGroup to the stdout
// 2. flusher the logGroup to the file. If the specified file name is not configured,
// the logGroups would append to the global log file.
type FlusherStdout struct {
	FileName      string
	MaxSize       int
	MaxRolls      int
	KeyValuePairs bool
	Tags          bool
	OnlyStdout    bool

	context   pipeline.Context
	outLogger seelog.LoggerInterface
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
		pattern = `<rollingfile type="size" filename="%s" maxsize="%d" maxrolls="%d"/>`
		if p.MaxSize <= 0 {
			p.MaxSize = 1024 * 1024
		}
		if p.MaxRolls <= 0 {
			p.MaxRolls = 1
		}
		pattern = fmt.Sprintf(pattern, p.FileName, p.MaxSize, p.MaxRolls)
	}
	if pattern != "" {
		var err error
		p.outLogger, err = seelog.LoggerFromConfigAsString(fmt.Sprintf(flushMsg, pattern))
		if err != nil {
			logger.Error(p.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init stdout flusher fail, error", err)
			p.outLogger = seelog.Disabled
		}
	}
	return nil
}

func (*FlusherStdout) Description() string {
	return "stdout flusher for logtail"
}

// Flush the logGroup list to stdout or files.
func (p *FlusherStdout) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		if p.Tags {
			if p.outLogger != nil {
				p.outLogger.Infof("[LogGroup] topic %s, logstore %s, logcount %d, tags %v", logGroup.Topic, logGroup.Category, len(logGroup.Logs), logGroup.LogTags)
			} else {
				logger.Info(p.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)
			}
		}

		if p.KeyValuePairs {
			for _, log := range logGroup.Logs {
				writer := jsoniter.NewStream(jsoniter.ConfigDefault, nil, 128)
				writer.WriteObjectStart()
				for _, c := range log.Contents {
					writer.WriteObjectField(c.Key)
					writer.WriteString(c.Value)
					_, _ = writer.Write([]byte{','})
				}
				writer.WriteObjectField("__time__")
				writer.WriteString(strconv.Itoa(int(log.Time)))
				writer.WriteObjectEnd()

				if p.outLogger != nil {
					p.outLogger.Infof("%s", writer.Buffer())
				} else {
					logger.Info(p.context.GetRuntimeContext(), string(writer.Buffer()))
				}
			}

		} else {
			for _, log := range logGroup.Logs {
				buf, _ := json.Marshal(log)
				if p.outLogger != nil {
					p.outLogger.Infof("%s", buf)
				} else {
					logger.Info(p.context.GetRuntimeContext(), string(buf))
				}
			}
		}

	}
	return nil
}

func (p *FlusherStdout) Export(in []*models.PipelineGroupEvents, context pipeline.PipelineContext) error {
	for _, groupEvents := range in {

		if p.Tags {
			metadata := fmt.Sprintf("%v", groupEvents.Group.GetMetadata().Iterator())
			tags := fmt.Sprintf("%v", groupEvents.Group.GetTags().Iterator())
			if p.outLogger != nil {
				p.outLogger.Infof("[Event] event %d, metadata %s, tags %s", len(groupEvents.Events), metadata, tags)
			} else {
				logger.Info(p.context.GetRuntimeContext(), "[Event] event", len(groupEvents.Events), "metadata", metadata, "tags", tags)
			}
		}

		for _, event := range groupEvents.Events {
			writer := jsoniter.NewStream(jsoniter.ConfigDefault, nil, 128)
			writer.WriteObjectStart()
			writer.WriteObjectField("eventType")
			switch event.GetType() {
			case models.EventTypeMetric:
				writer.WriteString("metric")
			case models.EventTypeSpan:
				writer.WriteString("span")
			case models.EventTypeLogging:
				writer.WriteString("log")
			case models.EventTypeByteArray:
				writer.WriteString("byteArray")
			}
			_, _ = writer.Write([]byte{','})
			writer.WriteObjectField("name")
			writer.WriteString(event.GetName())
			_, _ = writer.Write([]byte{','})
			writer.WriteObjectField("timestamp")
			writer.WriteUint64(event.GetTimestamp())
			_, _ = writer.Write([]byte{','})
			writer.WriteObjectField("observedTimestamp")
			writer.WriteUint64(event.GetObservedTimestamp())
			_, _ = writer.Write([]byte{','})
			writer.WriteObjectField("tags")
			writer.WriteObjectStart()
			i := 0
			for k, v := range event.GetTags().Iterator() {
				writer.WriteObjectField(k)
				writer.WriteString(v)
				if i < event.GetTags().Len()-1 {
					_, _ = writer.Write([]byte{','})
				}
				i++
			}
			writer.WriteObjectEnd()
			_, _ = writer.Write([]byte{','})
			switch event.GetType() {
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

			if p.outLogger != nil {
				p.outLogger.Infof("%s", writer.Buffer())
			} else {
				logger.Info(p.context.GetRuntimeContext(), string(writer.Buffer()))
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
	_, _ = writer.Write([]byte{','})
	writer.WriteObjectField("level")
	writer.WriteString(log.GetLevel())
	_, _ = writer.Write([]byte{','})
	writer.WriteObjectField("traceID")
	writer.WriteString(log.GetTraceID())
	_, _ = writer.Write([]byte{','})
	writer.WriteObjectField("traceID")
	writer.WriteString(log.GetTraceID())
	_, _ = writer.Write([]byte{','})
	writer.WriteObjectField("spanID")
	writer.WriteString(log.GetSpanID())
	contents := log.GetIndices()
	for key, value := range contents.Iterator() {
		_, _ = writer.Write([]byte{','})
		writer.WriteObjectField(key)
		_, _ = writer.Write([]byte(fmt.Sprintf("%#v", value)))
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
	if p.outLogger != nil {
		p.outLogger.Close()
	}
	return nil
}

// Register the plugin to the Flushers array.
func init() {
	pipeline.Flushers["flusher_stdout"] = func() pipeline.Flusher {
		return &FlusherStdout{
			KeyValuePairs: true,
		}
	}
}
