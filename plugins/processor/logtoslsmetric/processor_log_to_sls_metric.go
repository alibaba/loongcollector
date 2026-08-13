// Copyright 2023 iLogtail Authors
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

package logtoslsmetric

import (
	"errors"
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"
	"unicode"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorLogToSlsMetric struct {
	MetricTimeKey      string
	MetricLabelKeys    []string
	MetricValues       map[string]string
	CustomMetricLabels map[string]string
	IgnoreError        bool

	metricLabelKeysMap map[string]bool
	metricNamesMap     map[string]bool
	metricValuesMap    map[string]bool
	context            pipeline.Context
}

const (
	PluginName        = "processor_log_to_sls_metric"
	metricNameKey     = "__name__"
	metricLabelsKey   = "__labels__"
	metricTimeNanoKey = "__time_nano__"
	metricValueKey    = "__value__"
)

// Regex for labels and names
var (
	metricLabelKeyRegex = regexp.MustCompile(`^[a-zA-Z_][a-zA-Z0-9_]*$`)
	metricNameRegex     = regexp.MustCompile(`^[a-zA-Z_:][a-zA-Z0-9_:]*$`)
)

var (
	errInvalidMetricLabelKey      = errors.New("the Key of Label must follow the regular expression: ^[a-zA-Z_][a-zA-Z0-9_]*$")
	errInvalidMetricLabelValue    = errors.New("the value of Label can not contain '|' or '#$#'")
	errInvalidMetricLabelKeyCount = errors.New("the number of labels must be equal to the number of MetricLabelKeys")

	errEmptyMetricLabel = errors.New("MetricLabelKeys and CustomMetricLabels parameters are empty")

	errEmptyMetricValues = errors.New("MetricValues parameter is empty")

	errInvalidMetricName       = errors.New("the name of metric must follow the regular expression: ^[a-zA-Z_:][a-zA-Z0-9_:]*$")
	errInvalidMetricValue      = errors.New("the value of metric must be a number")
	errInvalidMetricNameCount  = errors.New("the number of metric names must be equal to the number of MetricValues")
	errInvalidMetricValueCount = errors.New("the number of metric values must be equal to the number of MetricValues")

	errInvalidMetricTime = errors.New("the value of MetricTime must be a valid Unix timestamp in second or millisecond or microsecond or nanosecond")

	errFieldRepeated = errors.New("the field is repeated")
)

var (
	processorLogErrorAlarmType     = selfmonitor.AlarmType("PROCESSOR_LOG_ALARM")
	processorInitErrorLogAlarmType = selfmonitor.AlarmType("PROCESSOR_INIT_ALARM")
)

func (p *ProcessorLogToSlsMetric) Init(context pipeline.Context) (err error) {
	p.context = context
	// Check if the label parameter exists
	if len(p.MetricLabelKeys) == 0 && len(p.CustomMetricLabels) == 0 {
		err = errEmptyMetricLabel
		p.logInitError(err)
		return
	}
	// Check if MetricValues is empty
	if len(p.MetricValues) == 0 {
		err = errEmptyMetricValues
		p.logInitError(err)
		return
	}
	// Check field is repeated
	existField := map[string]bool{}
	existField[metricLabelsKey] = true
	// Cache labelKey to map for quick access
	p.metricLabelKeysMap = map[string]bool{}
	for _, labelKey := range p.MetricLabelKeys {
		// The Key of Label must follow the regular expression: ^[a-zA-Z_][a-zA-Z0-9_]*$
		if !metricLabelKeyRegex.MatchString(labelKey) {
			err = errInvalidMetricLabelKey
			p.logInitError(err)
			return
		}

		// Check field is repeated
		if existField[labelKey] {
			err = errFieldRepeated
			p.logInitError(err)
			return
		}
		existField[labelKey] = true

		p.metricLabelKeysMap[labelKey] = true
	}
	// Check keys and values of CustomMetricLabels are valid
	for key, value := range p.CustomMetricLabels {
		if !metricLabelKeyRegex.MatchString(key) {
			err = errInvalidMetricLabelKey
			p.logInitError(err)
			return
		}
		// The value of Label cannot contain "|" or "#$#".
		if strings.Contains(value, converter.LabelSeparator) || strings.Contains(value, converter.KeyValueSeparator) {
			err = errInvalidMetricLabelValue
			p.logInitError(err)
			return
		}

		// Check field is repeated
		if existField[key] {
			err = errFieldRepeated
			p.logInitError(err)
			return
		}
		existField[key] = true
	}
	// Cache name and value to map for quick access
	p.metricNamesMap = map[string]bool{}
	p.metricValuesMap = map[string]bool{}
	for name, value := range p.MetricValues {
		// Check field is repeated
		if existField[name] {
			err = errFieldRepeated
			p.logInitError(err)
			return
		}
		existField[name] = true

		// Check field is repeated
		if existField[value] {
			err = errFieldRepeated
			p.logInitError(err)
			return
		}
		existField[value] = true

		p.metricNamesMap[name] = true
		p.metricValuesMap[value] = true
	}
	return nil
}

func (p *ProcessorLogToSlsMetric) Description() string {
	return "Parse fields from logs into metrics."
}

func (p *ProcessorLogToSlsMetric) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	var metricLogs []*protocol.Log
TraverseLogArray:
	for _, log := range logArray {
		names := map[string]string{}
		values := map[string]string{}
		// __time_nano__ field
		var timeNano string
		// __labels__ field
		metricLabels := converter.MetricLabels{}
		labelExisted := map[string]bool{}
		for i, cont := range log.Contents {
			if log.Contents[i] == nil {
				continue
			}

			// __labels__
			if cont.Key == metricLabelsKey {
				labels := strings.Split(cont.Value, converter.LabelSeparator)
				for _, label := range labels {
					keyValues := strings.Split(label, converter.KeyValueSeparator)
					if len(keyValues) != 2 {
						p.logError(errInvalidMetricLabelValue)
						continue TraverseLogArray
					}
					key := keyValues[0]
					if p.metricLabelKeysMap[key] {
						p.logError(errFieldRepeated)
						continue TraverseLogArray
					}
					// The Key of Label must follow the regular expression: ^[a-zA-Z_][a-zA-Z0-9_]*$
					if !metricLabelKeyRegex.MatchString(key) {
						p.logError(errInvalidMetricLabelKey)
						continue TraverseLogArray
					}
					value := keyValues[1]
					// The value of Label cannot contain "|" or "#$#".
					if strings.Contains(value, converter.LabelSeparator) || strings.Contains(value, converter.KeyValueSeparator) {
						p.logError(errInvalidMetricLabelValue)
						continue TraverseLogArray
					}
					metricLabels = append(metricLabels, converter.MetricLabel{Key: key, Value: value})
				}
				continue
			}

			// Match to the label field
			if p.metricLabelKeysMap[cont.Key] {
				if labelExisted[cont.Key] {
					p.logError(errFieldRepeated)
					continue TraverseLogArray
				}
				// The value of Label cannot contain "|" or "#$#".
				if strings.Contains(cont.Value, converter.LabelSeparator) || strings.Contains(cont.Value, converter.KeyValueSeparator) {
					p.logError(errInvalidMetricLabelValue)
					continue TraverseLogArray
				}
				labelExisted[cont.Key] = true
				metricLabels = append(metricLabels, converter.MetricLabel{Key: cont.Key, Value: cont.Value})
				continue
			}

			// Match to the name field
			if p.metricNamesMap[cont.Key] {
				// Metric name needs to follow the regular expression: ^[a-zA-Z_:][a-zA-Z0-9_:]*$
				if !metricNameRegex.MatchString(cont.Value) {
					p.logError(errInvalidMetricName)
					continue TraverseLogArray
				}
				names[cont.Key] = cont.Value
				continue
			}

			// Match to the value field
			if p.metricValuesMap[cont.Key] {
				// Metric value needs to be a float type string.
				if !canParseToFloat64(cont.Value) {
					p.logError(errInvalidMetricValue)
					continue TraverseLogArray
				}
				values[cont.Key] = cont.Value
				continue
			}

			if p.MetricTimeKey != "" && cont.Key == p.MetricTimeKey {
				if !isTimeNano(cont.Value) {
					p.logError(errInvalidMetricTime)
					continue TraverseLogArray
				}
				switch len(cont.Value) {
				case 19:
					// nanosecond
					timeNano = cont.Value
				case 16:
					// microsecond
					timeNano = cont.Value + "000"
				case 13:
					// millisecond
					timeNano = cont.Value + "000000"
				case 10:
					// second
					timeNano = cont.Value + "000000000"
				}
				continue
			}
		}

		if timeNano == "" {
			if p.MetricTimeKey != "" {
				p.logError(errInvalidMetricTime)
				continue TraverseLogArray
			}
			timeNano = GetLogTimeNano(log)
		}

		// The number of labels must be equal to the number of label fields.
		if len(labelExisted) != len(p.MetricLabelKeys) {
			p.logError(errInvalidMetricLabelKeyCount)
			continue TraverseLogArray
		}

		// The number of names must be equal to the number of name fields.
		if len(names) != len(p.metricNamesMap) {
			p.logError(errInvalidMetricNameCount)
			continue TraverseLogArray
		}

		// The number of values must be equal to the number of value fields.
		if len(values) != len(p.metricValuesMap) {
			p.logError(errInvalidMetricValueCount)
			continue TraverseLogArray
		}

		for key, value := range p.CustomMetricLabels {
			metricLabels = append(metricLabels, converter.MetricLabel{Key: key, Value: value})
		}

		metricLabel := metricLabels.GetLabel()

		for name, value := range p.MetricValues {
			metricLog := &protocol.Log{
				Time:     log.Time,
				Contents: nil,
			}
			metricLog.Contents = append(metricLog.Contents, &protocol.Log_Content{
				Key:   metricLabelsKey,
				Value: metricLabel,
			})
			metricLog.Contents = append(metricLog.Contents, &protocol.Log_Content{
				Key:   metricNameKey,
				Value: names[name],
			})
			metricLog.Contents = append(metricLog.Contents, &protocol.Log_Content{
				Key:   metricValueKey,
				Value: values[value],
			})
			metricLog.Contents = append(metricLog.Contents, &protocol.Log_Content{
				Key:   metricTimeNanoKey,
				Value: timeNano,
			})
			metricLogs = append(metricLogs, metricLog)
		}
	}
	return metricLogs
}

func (p *ProcessorLogToSlsMetric) logError(err error) {
	if !p.IgnoreError {
		logger.Warning(p.context.GetRuntimeContext(), processorLogErrorAlarmType, "process log error", err)
	}
}

func (p *ProcessorLogToSlsMetric) logInitError(err error) {
	logger.Warning(p.context.GetRuntimeContext(), processorInitErrorLogAlarmType, "init processor_log_to_sls_metric error", err)
}

func GetLogTimeNano(log *protocol.Log) string {
	nanoTime := int64(log.Time) * int64(time.Second)
	if log.TimeNs != nil {
		nanoTime += int64(*log.TimeNs)
	}
	nanosecondsStr := strconv.FormatInt(nanoTime, 10)
	return nanosecondsStr
}

func canParseToFloat64(s string) bool {
	_, err := strconv.ParseFloat(s, 64)
	return err == nil
}

func isTimeNano(t string) bool {
	length := len(t)
	if length != 19 && length != 16 && length != 13 && length != 10 {
		return false
	}
	for _, c := range t {
		if !unicode.IsDigit(c) {
			return false
		}
	}
	return true
}

func init() {
	pipeline.Processors[PluginName] = func() pipeline.Processor {
		return &ProcessorLogToSlsMetric{
			IgnoreError: false,
		}
	}
}

// Process implements the v2 ProcessorV2 interface. Mirroring the v1 semantics
// (v1 inputs Log and outputs SLS-metric-format Logs), the v2 path inputs Log
// events and OUTPUTS models.Metric events: each matched Log is converted into
// one *models.Metric per configured (name, value) pair, all sharing the same
// tags and timestamp.
//
// Just like v1 (see ProcessLogs), a source Log is NEVER preserved: on success
// it is replaced by the produced Metric events; on any validation error it is
// logged and dropped entirely (v1 does `continue TraverseLogArray`, emitting
// only metric logs). Logs that do not carry the configured name/value/label
// fields fail the count checks and are likewise dropped. Metric and Span events
// (and any other non-Log kind) pass through unchanged.
func (p *ProcessorLogToSlsMetric) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil || len(in.Events) == 0 {
		return
	}
	outEvents := make([]models.PipelineEvent, 0, len(in.Events))
	for _, event := range in.Events {
		if event.GetType() == models.EventTypeLogging {
			log, ok := event.(*models.Log)
			if !ok {
				continue
			}
			for _, metric := range p.logToMetrics(log) {
				outEvents = append(outEvents, metric)
			}
			// v1 does not preserve the source log; only metrics are emitted.
			continue
		}
		// Non-Log events (Metric/Span/...) pass through unchanged.
		outEvents = append(outEvents, event)
	}
	if len(outEvents) == 0 {
		return
	}
	context.Collector().Collect(in.Group, outEvents...)
}

// logToMetrics reproduces the v1 ProcessLogs extraction + validation for a
// single v2 models.Log, returning the produced metrics or nil when the log
// fails any validation (in which case it is logged and dropped, matching the
// v1 `continue TraverseLogArray` skip semantics).
func (p *ProcessorLogToSlsMetric) logToMetrics(log *models.Log) []*models.Metric {
	contents := log.GetIndices()

	names := map[string]string{}
	values := map[string]string{}
	// __time_nano__ (nanoseconds, as a decimal string) parsed from MetricTimeKey.
	var timeNano string
	// Collected labels shared by every produced metric.
	labels := map[string]string{}
	labelExisted := map[string]bool{}

	for key, rawValue := range contents.Iterator() {
		value := contentToString(rawValue)

		// __labels__
		if key == metricLabelsKey {
			splitLabels := strings.Split(value, converter.LabelSeparator)
			for _, label := range splitLabels {
				keyValues := strings.Split(label, converter.KeyValueSeparator)
				if len(keyValues) != 2 {
					p.logError(errInvalidMetricLabelValue)
					return nil
				}
				labelKey := keyValues[0]
				if p.metricLabelKeysMap[labelKey] {
					p.logError(errFieldRepeated)
					return nil
				}
				// The Key of Label must follow the regular expression: ^[a-zA-Z_][a-zA-Z0-9_]*$
				if !metricLabelKeyRegex.MatchString(labelKey) {
					p.logError(errInvalidMetricLabelKey)
					return nil
				}
				labelValue := keyValues[1]
				// The value of Label cannot contain "|" or "#$#".
				if strings.Contains(labelValue, converter.LabelSeparator) || strings.Contains(labelValue, converter.KeyValueSeparator) {
					p.logError(errInvalidMetricLabelValue)
					return nil
				}
				labels[labelKey] = labelValue
			}
			continue
		}

		// Match to the label field
		if p.metricLabelKeysMap[key] {
			// The value of Label cannot contain "|" or "#$#".
			if strings.Contains(value, converter.LabelSeparator) || strings.Contains(value, converter.KeyValueSeparator) {
				p.logError(errInvalidMetricLabelValue)
				return nil
			}
			labelExisted[key] = true
			labels[key] = value
			continue
		}

		// Match to the name field
		if p.metricNamesMap[key] {
			// Metric name needs to follow the regular expression: ^[a-zA-Z_:][a-zA-Z0-9_:]*$
			if !metricNameRegex.MatchString(value) {
				p.logError(errInvalidMetricName)
				return nil
			}
			names[key] = value
			continue
		}

		// Match to the value field
		if p.metricValuesMap[key] {
			// Metric value needs to be a float type string.
			if !canParseToFloat64(value) {
				p.logError(errInvalidMetricValue)
				return nil
			}
			values[key] = value
			continue
		}

		if p.MetricTimeKey != "" && key == p.MetricTimeKey {
			if !isTimeNano(value) {
				p.logError(errInvalidMetricTime)
				return nil
			}
			switch len(value) {
			case 19:
				// nanosecond
				timeNano = value
			case 16:
				// microsecond
				timeNano = value + "000"
			case 13:
				// millisecond
				timeNano = value + "000000"
			case 10:
				// second
				timeNano = value + "000000000"
			}
			continue
		}
	}

	if timeNano == "" {
		if p.MetricTimeKey != "" {
			p.logError(errInvalidMetricTime)
			return nil
		}
		// models.Log.Timestamp is already in nanoseconds (see plugin_runner_v2
		// conversion), matching v1 GetLogTimeNano.
		timeNano = strconv.FormatUint(log.GetTimestamp(), 10)
	}

	// The number of labels must be equal to the number of label fields.
	if len(labelExisted) != len(p.MetricLabelKeys) {
		p.logError(errInvalidMetricLabelKeyCount)
		return nil
	}

	// The number of names must be equal to the number of name fields.
	if len(names) != len(p.metricNamesMap) {
		p.logError(errInvalidMetricNameCount)
		return nil
	}

	// The number of values must be equal to the number of value fields.
	if len(values) != len(p.metricValuesMap) {
		p.logError(errInvalidMetricValueCount)
		return nil
	}

	for key, value := range p.CustomMetricLabels {
		labels[key] = value
	}

	timestamp, err := strconv.ParseInt(timeNano, 10, 64)
	if err != nil {
		p.logError(errInvalidMetricTime)
		return nil
	}

	metrics := make([]*models.Metric, 0, len(p.MetricValues))
	for name, value := range p.MetricValues {
		// Already validated by canParseToFloat64 above.
		metricValue, _ := strconv.ParseFloat(values[value], 64)
		metrics = append(metrics, models.NewSingleValueMetric(
			names[name],
			models.MetricTypeGauge,
			newTagsFromMap(labels),
			timestamp,
			metricValue,
		))
	}
	return metrics
}

// contentToString normalizes a v2 log content value (interface{}) into a string
// for the string-oriented extraction/validation reused from v1.
func contentToString(value interface{}) string {
	switch v := value.(type) {
	case string:
		return v
	case []byte:
		return string(v)
	default:
		return fmt.Sprintf("%v", v)
	}
}

// newTagsFromMap builds a fresh models.Tags so each produced metric owns an
// independent copy of the shared labels.
func newTagsFromMap(labels map[string]string) models.Tags {
	tags := models.NewTags()
	for key, value := range labels {
		tags.Add(key, value)
	}
	return tags
}
