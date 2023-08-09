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
	"regexp"
	"strconv"
	"strings"
	"time"
	"unicode"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
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
	processorLogErrorAlarmType     = "PROCESSOR_LOG_ALARM"
	processorInitErrorLogAlarmType = "PROCESSOR_INIT_ALARM"
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
		logger.Error(p.context.GetRuntimeContext(), processorLogErrorAlarmType, "process log error", err)
	}
}

func (p *ProcessorLogToSlsMetric) logInitError(err error) {
	logger.Error(p.context.GetRuntimeContext(), processorInitErrorLogAlarmType, "init processor_log_to_sls_metric error", err)
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
