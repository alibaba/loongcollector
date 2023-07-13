package opentelemetry

import (
	"encoding/json"
	"fmt"
	"math"
	"sort"
	"strconv"
	"strings"
	"time"
	"unicode"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/otlp"
)

const (
	metricNameKey      = "__name__"
	labelsKey          = "__labels__"
	timeNanoKey        = "__time_nano__"
	valueKey           = "__value__"
	infinityBoundValue = "+Inf"
	bucketLabelKey     = "le"
	summaryLabelKey    = "quantile"
)

const (
	metricNameSuffixSum       = "_sum"
	metricNameSuffixCount     = "_count"
	metricNameSuffixMax       = "_max"
	metricNameSuffixMin       = "_min"
	metricNameSuffixBucket    = "_bucket"
	metricNameSuffixExemplars = "_exemplars"
)

type KeyValue struct {
	Key   string
	Value string
}

type KeyValues struct {
	keyValues []KeyValue
}

func (kv *KeyValues) Len() int {
	return len(kv.keyValues)
}

func (kv *KeyValues) Swap(i, j int) {
	kv.keyValues[i], kv.keyValues[j] = kv.keyValues[j], kv.keyValues[i]
}

func (kv *KeyValues) Less(i, j int) bool {
	return kv.keyValues[i].Key < kv.keyValues[j].Key
}

func (kv *KeyValues) Sort() {
	sort.Sort(kv)
}

func (kv *KeyValues) Replace(key, value string) {
	key = sanitize(key)
	findIndex := sort.Search(len(kv.keyValues), func(index int) bool {
		return kv.keyValues[index].Key >= key
	})
	if findIndex < len(kv.keyValues) && kv.keyValues[findIndex].Key == key {
		kv.keyValues[findIndex].Value = value
	}
}

func (kv *KeyValues) Append(key, value string) {
	key = sanitize(key)
	kv.keyValues = append(kv.keyValues, KeyValue{
		key,
		value,
	})
}

func (kv *KeyValues) Clone() KeyValues {
	var newKeyValues KeyValues
	newKeyValues.keyValues = make([]KeyValue, len(kv.keyValues))
	copy(newKeyValues.keyValues, kv.keyValues)
	return newKeyValues
}

func (kv *KeyValues) String() string {
	var builder strings.Builder
	kv.labelToStringBuilder(&builder)
	return builder.String()
}

func (kv *KeyValues) labelToStringBuilder(sb *strings.Builder) {
	for index, label := range kv.keyValues {
		sb.WriteString(label.Key)
		sb.WriteString("#$#")
		sb.WriteString(label.Value)
		if index != len(kv.keyValues)-1 {
			sb.WriteByte('|')
		}
	}
}

func min(l, r int) int {
	if l < r {
		return l
	}
	return r
}

func formatMetricName(name string) string {
	var newName []byte
	for i := 0; i < len(name); i++ {
		b := name[i]
		if (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') || b == '_' || b == ':' {
			continue
		} else {
			if newName == nil {
				newName = []byte(name)
			}
			newName[i] = '_'
		}
	}
	if newName == nil {
		return name
	}
	return string(newName)
}

func newMetricLogFromRaw(name string, labels KeyValues, nsec int64, value float64) *protocol.Log {
	labels.Sort()
	timeNs := uint32(0)
	if config.LogtailGlobalConfig.EnableTimestampNanosecond {
		timeNs = uint32(nsec % 1e9)
	}
	return &protocol.Log{
		Time:   uint32(nsec / 1e9),
		TimeNs: timeNs,
		Contents: []*protocol.Log_Content{
			{
				Key:   metricNameKey,
				Value: formatMetricName(name),
			},
			{
				Key:   labelsKey,
				Value: labels.String(),
			},
			{
				Key:   timeNanoKey,
				Value: strconv.FormatInt(nsec, 10),
			},
			{
				Key:   valueKey,
				Value: strconv.FormatFloat(value, 'g', -1, 64),
			},
		},
	}
}

func sanitize(s string) string {
	if len(s) == 0 {
		return s
	}

	s = strings.Map(sanitizeRune, s)
	if unicode.IsDigit(rune(s[0])) {
		s = "key_" + s
	}

	if s[0] == '_' {
		s = "key" + s
	}

	return s
}

func sanitizeRune(r rune) rune {
	if unicode.IsLetter(r) || unicode.IsDigit(r) {
		return r
	}

	return '_'
}

func attrs2Labels(labels *KeyValues, attrs pcommon.Map) {
	attrs.Range(func(k string, v pcommon.Value) bool {
		labels.Append(k, v.AsString())
		return true
	})
}

func newExemplarMetricLogFromRaw(name string, exemplar pmetric.Exemplar, labels KeyValues) *protocol.Log {
	metricName := name + metricNameSuffixExemplars
	if !exemplar.TraceID().IsEmpty() {
		labels.Append("traceId", exemplar.TraceID().String())
	}

	if !exemplar.SpanID().IsEmpty() {
		labels.Append("spanId", exemplar.SpanID().String())
	}

	filterAttributeMap := pcommon.NewMap()
	exemplar.FilteredAttributes().CopyTo(filterAttributeMap)

	for key, value := range filterAttributeMap.AsRaw() {
		labels.Append(key, fmt.Sprintf("%v", value))
	}

	labels.Sort()
	log := &protocol.Log{
		Time: uint32(exemplar.Timestamp() / 1e9),
		Contents: []*protocol.Log_Content{
			{
				Key:   metricNameKey,
				Value: formatMetricName(metricName),
			},
			{
				Key:   labelsKey,
				Value: labels.String(),
			},
			{
				Key:   timeNanoKey,
				Value: strconv.FormatInt(exemplar.Timestamp().AsTime().Unix(), 10),
			}, {
				Key:   valueKey,
				Value: strconv.FormatFloat(exemplar.DoubleValue(), 'g', -1, 64),
			},
		},
	}
	if config.LogtailGlobalConfig.EnableTimestampNanosecond {
		log.TimeNs = uint32(exemplar.Timestamp() % 1e9)
	}
	return log
}

func GaugeToLogs(name string, data pmetric.NumberDataPointSlice, defaultLabels KeyValues) (logs []*protocol.Log) {
	for i := 0; i < data.Len(); i++ {
		dataPoint := data.At(i)

		labels := defaultLabels.Clone()
		attrs2Labels(&labels, dataPoint.Attributes())

		for j := 0; j < dataPoint.Exemplars().Len(); j++ {
			logs = append(logs, newExemplarMetricLogFromRaw(name, dataPoint.Exemplars().At(j), labels.Clone()))
		}

		value := dataPoint.DoubleValue()
		if dataPoint.IntValue() != 0 {
			value = float64(dataPoint.IntValue())
		}
		logs = append(logs, newMetricLogFromRaw(name, labels, int64(dataPoint.Timestamp()), value))
	}
	return logs
}

func SumToLogs(name string, aggregationTemporality pmetric.AggregationTemporality, isMonotonic string, data pmetric.NumberDataPointSlice, defaultLabels KeyValues) (logs []*protocol.Log) {
	for i := 0; i < data.Len(); i++ {
		dataPoint := data.At(i)

		labels := defaultLabels.Clone()
		attrs2Labels(&labels, dataPoint.Attributes())
		labels.Append(otlp.TagKeyMetricIsMonotonic, isMonotonic)
		labels.Append(otlp.TagKeyMetricAggregationTemporality, aggregationTemporality.String())

		for j := 0; j < dataPoint.Exemplars().Len(); j++ {
			logs = append(logs, newExemplarMetricLogFromRaw(name, dataPoint.Exemplars().At(j), labels.Clone()))
		}

		value := dataPoint.DoubleValue()
		if dataPoint.IntValue() != 0 {
			value = float64(dataPoint.IntValue())
		}
		logs = append(logs, newMetricLogFromRaw(name, labels, int64(dataPoint.Timestamp()), value))
	}
	return logs
}

func SummaryToLogs(name string, data pmetric.SummaryDataPointSlice, defaultLabels KeyValues) (logs []*protocol.Log) {
	for i := 0; i < data.Len(); i++ {
		dataPoint := data.At(i)

		labels := defaultLabels.Clone()
		attrs2Labels(&labels, dataPoint.Attributes())

		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixSum, labels, int64(dataPoint.Timestamp()), dataPoint.Sum()))
		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixCount, labels, int64(dataPoint.Timestamp()), float64(dataPoint.Count())))

		summaryLabels := labels.Clone()
		summaryLabels.Append(summaryLabelKey, "")
		summaryLabels.Sort()

		values := dataPoint.QuantileValues()
		for j := 0; j < values.Len(); j++ {
			value := values.At(j)
			summaryLabels.Replace(summaryLabelKey, strconv.FormatFloat(value.Quantile(), 'g', -1, 64))
			logs = append(logs, newMetricLogFromRaw(name, summaryLabels, int64(dataPoint.Timestamp()), value.Value()))
		}
	}
	return logs
}

func HistogramToLogs(name string, data pmetric.HistogramDataPointSlice, aggregationTemporality pmetric.AggregationTemporality, defaultLabels KeyValues) (logs []*protocol.Log) {
	for i := 0; i < data.Len(); i++ {
		dataPoint := data.At(i)

		labels := defaultLabels.Clone()
		attrs2Labels(&labels, dataPoint.Attributes())
		labels.Append(otlp.TagKeyMetricAggregationTemporality, aggregationTemporality.String())
		labels.Append(otlp.TagKeyMetricHistogramType, pmetric.MetricTypeHistogram.String())

		if dataPoint.HasSum() {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixSum, labels, int64(dataPoint.Timestamp()), dataPoint.Sum()))
		}
		if dataPoint.HasMin() {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixMin, labels, int64(dataPoint.Timestamp()), dataPoint.Min()))
		}
		if dataPoint.HasMax() {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixMax, labels, int64(dataPoint.Timestamp()), dataPoint.Max()))
		}
		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixCount, labels, int64(dataPoint.Timestamp()), float64(dataPoint.Count())))

		for j := 0; j < dataPoint.Exemplars().Len(); j++ {
			logs = append(logs, newExemplarMetricLogFromRaw(name, dataPoint.Exemplars().At(j), labels.Clone()))
		}

		bounds := dataPoint.ExplicitBounds()
		boundsStr := make([]string, bounds.Len()+1)
		for j := 0; j < bounds.Len(); j++ {
			boundsStr[j] = strconv.FormatFloat(bounds.At(j), 'g', -1, 64)
		}
		boundsStr[len(boundsStr)-1] = infinityBoundValue

		bucketCount := min(len(boundsStr), dataPoint.BucketCounts().Len())

		bucketLabels := labels.Clone()
		bucketLabels.Append(bucketLabelKey, "")
		bucketLabels.Sort()

		sumCount := uint64(0)
		for j := 0; j < bucketCount; j++ {
			bucket := dataPoint.BucketCounts().At(j)
			bucketLabels.Replace(bucketLabelKey, boundsStr[j])
			sumCount += bucket
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixBucket, bucketLabels, int64(dataPoint.Timestamp()), float64(sumCount)))
		}
	}
	return logs
}

func ExponentialHistogramToLogs(name string, data pmetric.ExponentialHistogramDataPointSlice, aggregationTemporality pmetric.AggregationTemporality, defaultLabels KeyValues) (logs []*protocol.Log) {
	for i := 0; i < data.Len(); i++ {
		dataPoint := data.At(i)

		labels := defaultLabels.Clone()
		attrs2Labels(&labels, dataPoint.Attributes())
		labels.Append(otlp.TagKeyMetricAggregationTemporality, aggregationTemporality.String())
		labels.Append(otlp.TagKeyMetricHistogramType, pmetric.MetricTypeExponentialHistogram.String())

		if dataPoint.HasSum() {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixSum, labels, int64(dataPoint.Timestamp()), dataPoint.Sum()))
		}
		if dataPoint.HasMin() {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixMin, labels, int64(dataPoint.Timestamp()), dataPoint.Min()))
		}
		if dataPoint.HasMax() {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixMax, labels, int64(dataPoint.Timestamp()), dataPoint.Max()))
		}
		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixCount, labels, int64(dataPoint.Timestamp()), float64(dataPoint.Count())))

		for j := 0; j < dataPoint.Exemplars().Len(); j++ {
			logs = append(logs, newExemplarMetricLogFromRaw(name, dataPoint.Exemplars().At(j), labels.Clone()))
		}

		scale := dataPoint.Scale()
		base := math.Pow(2, math.Pow(2, float64(-scale)))
		postiveFields := genExponentialHistogramValues(true, base, dataPoint.Positive())
		negativeFields := genExponentialHistogramValues(false, base, dataPoint.Negative())

		bucketLabels := labels.Clone()
		bucketLabels.Append(bucketLabelKey, "")
		bucketLabels.Sort()
		for k, v := range postiveFields {
			bucketLabels.Replace(bucketLabelKey, k)
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixBucket, bucketLabels, int64(dataPoint.Timestamp()), v))
		}
		bucketLabels.Replace(bucketLabelKey, otlp.FieldZeroCount)
		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixBucket, bucketLabels, int64(dataPoint.Timestamp()), float64(dataPoint.ZeroCount())))
		for k, v := range negativeFields {
			bucketLabels.Replace(bucketLabelKey, k)
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixBucket, bucketLabels, int64(dataPoint.Timestamp()), v))
		}
	}
	return logs
}

func ConvertOtlpLogRequestV1(otlpLogReq plogotlp.ExportRequest) (logs []*protocol.Log, err error) {
	return ConvertOtlpLogV1(otlpLogReq.Logs())
}

func ConvertOtlpLogV1(otlpLogs plog.Logs) (logs []*protocol.Log, err error) {
	resLogs := otlpLogs.ResourceLogs()
	for i := 0; i < resLogs.Len(); i++ {
		resourceLog := resLogs.At(i)
		sLogs := resourceLog.ScopeLogs()
		for j := 0; j < sLogs.Len(); j++ {
			scopeLog := sLogs.At(j)
			lRecords := scopeLog.LogRecords()
			for k := 0; k < lRecords.Len(); k++ {
				logRecord := lRecords.At(k)

				protoContents := []*protocol.Log_Content{
					{
						Key:   "time_unix_nano",
						Value: strconv.FormatInt(logRecord.Timestamp().AsTime().UnixNano(), 10),
					},
					{
						Key:   "severity_number",
						Value: strconv.FormatInt(int64(logRecord.SeverityNumber()), 10),
					},
					{
						Key:   "severity_text",
						Value: logRecord.SeverityText(),
					},
					{
						Key:   "content",
						Value: logRecord.Body().AsString(),
					},
				}

				if logRecord.Attributes().Len() != 0 {
					if d, err := json.Marshal(logRecord.Attributes().AsRaw()); err == nil {
						protoContents = append(protoContents, &protocol.Log_Content{
							Key:   "attributes",
							Value: string(d),
						})
					}
				}

				if resourceLog.Resource().Attributes().Len() != 0 {
					if d, err := json.Marshal(resourceLog.Resource().Attributes().AsRaw()); err == nil {
						protoContents = append(protoContents, &protocol.Log_Content{
							Key:   "resources",
							Value: string(d),
						})
					}
				}

				protoLog := &protocol.Log{
					Time:     uint32(logRecord.Timestamp().AsTime().Unix()),
					Contents: protoContents,
				}
				if config.LogtailGlobalConfig.EnableTimestampNanosecond {
					protoLog.TimeNs = uint32(logRecord.Timestamp().AsTime().Nanosecond())
				}
				logs = append(logs, protoLog)
			}
		}
	}

	return logs, nil
}

func ConvertOtlpMetricRequestV1(otlpMetricReq pmetricotlp.ExportRequest) (logs []*protocol.Log, err error) {
	return ConvertOtlpMetricV1(otlpMetricReq.Metrics())
}

func ConvertOtlpMetricV1(otlpMetrics pmetric.Metrics) (logs []*protocol.Log, err error) {
	resMetrics := otlpMetrics.ResourceMetrics()
	resMetricsLen := resMetrics.Len()

	if 0 == resMetricsLen {
		return
	}

	for i := 0; i < resMetricsLen; i++ {
		resMetricsSlice := resMetrics.At(i)
		var labels KeyValues
		attrs2Labels(&labels, resMetricsSlice.Resource().Attributes())

		scopeMetrics := resMetricsSlice.ScopeMetrics()
		for j := 0; j < scopeMetrics.Len(); j++ {
			otMetrics := scopeMetrics.At(j).Metrics()

			for k := 0; k < otMetrics.Len(); k++ {
				otMetric := otMetrics.At(k)
				metricName := otMetric.Name()

				switch otMetric.Type() {
				case pmetric.MetricTypeGauge:
					otGauge := otMetric.Gauge()
					otDatapoints := otGauge.DataPoints()
					logs = append(logs, GaugeToLogs(metricName, otDatapoints, labels)...)
				case pmetric.MetricTypeSum:
					otSum := otMetric.Sum()
					isMonotonic := strconv.FormatBool(otSum.IsMonotonic())
					aggregationTemporality := otSum.AggregationTemporality()
					otDatapoints := otSum.DataPoints()
					logs = append(logs, SumToLogs(metricName, aggregationTemporality, isMonotonic, otDatapoints, labels)...)
				case pmetric.MetricTypeSummary:
					otSummary := otMetric.Summary()
					otDatapoints := otSummary.DataPoints()
					logs = append(logs, SummaryToLogs(metricName, otDatapoints, labels)...)
				case pmetric.MetricTypeHistogram:
					otHistogram := otMetric.Histogram()
					aggregationTemporality := otHistogram.AggregationTemporality()
					otDatapoints := otHistogram.DataPoints()
					logs = append(logs, HistogramToLogs(metricName, otDatapoints, aggregationTemporality, labels)...)
				case pmetric.MetricTypeExponentialHistogram:
					otExponentialHistogram := otMetric.ExponentialHistogram()
					aggregationTemporality := otExponentialHistogram.AggregationTemporality()
					otDatapoints := otExponentialHistogram.DataPoints()
					logs = append(logs, ExponentialHistogramToLogs(metricName, otDatapoints, aggregationTemporality, labels)...)
				default:
					// TODO:
					// find a better way to handle metric with type MetricTypeEmpty.
					nowTime := time.Now()
					log := &protocol.Log{
						Time: uint32(nowTime.Unix()),
						Contents: []*protocol.Log_Content{
							{
								Key:   metricNameKey,
								Value: otMetric.Name(),
							},
							{
								Key:   labelsKey,
								Value: otMetric.Type().String(),
							},
							{
								Key:   timeNanoKey,
								Value: strconv.FormatInt(nowTime.UnixNano(), 10),
							},
							{
								Key:   valueKey,
								Value: otMetric.Description(),
							},
						},
					}
					if config.LogtailGlobalConfig.EnableTimestampNanosecond {
						log.TimeNs = uint32(nowTime.Nanosecond())
					}
					logs = append(logs, log)
				}
			}
		}
	}

	return logs, err
}

func ConvertOtlpTraceRequestV1(otlpTraceReq ptraceotlp.ExportRequest) (logs []*protocol.Log, err error) {
	return ConvertOtlpTraceV1(otlpTraceReq.Traces())
}

func ConvertOtlpTraceV1(otlpTrace ptrace.Traces) (logs []*protocol.Log, err error) {
	return logs, fmt.Errorf("does_not_support_otlptraces")
}
