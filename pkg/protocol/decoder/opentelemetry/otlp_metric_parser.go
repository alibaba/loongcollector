package opentelemetry

import (
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"math"
	"strconv"

	"go.opentelemetry.io/collector/pdata/pcommon"
	v1Common "go.opentelemetry.io/proto/otlp/common/v1"
	v1 "go.opentelemetry.io/proto/otlp/metrics/v1"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/otlp"
)

func attrsToLabels(labels *helper.MetricLabels, attrs []*v1Common.KeyValue) {
	for _, attribute := range attrs {
		labels.Append(attribute.GetKey(), anyValueToString(attribute.GetValue()))
	}
}

func anyValueToString(value *v1Common.AnyValue) string {
	switch value.Value.(type) {
	case *v1Common.AnyValue_StringValue:
		return value.GetStringValue()
	case *v1Common.AnyValue_IntValue:
		return strconv.FormatInt(value.GetIntValue(), 10)
	case *v1Common.AnyValue_DoubleValue:
		return strconv.FormatFloat(value.GetDoubleValue(), 'f', -1, 64)
	case *v1Common.AnyValue_BoolValue:
		return strconv.FormatBool(value.GetBoolValue())
	case *v1Common.AnyValue_KvlistValue:
		data, _ := json.Marshal(value.GetKvlistValue())
		return string(data)
	case *v1Common.AnyValue_ArrayValue:
		data, _ := json.Marshal(value.GetArrayValue())
		return string(data)
	case *v1Common.AnyValue_BytesValue:
		return base64.StdEncoding.EncodeToString(value.GetBytesValue())
	}
	return ""
}

func ConvertOtlpMetrics(metrics *v1.ResourceMetrics) (logs []*protocol.Log, err error) {
	if metrics == nil || len(metrics.GetScopeMetrics()) == 0 {
		return nil, nil
	}

	var labels helper.MetricLabels
	attrsToLabels(&labels, metrics.GetResource().GetAttributes())

	for _, scopeMetrics := range metrics.GetScopeMetrics() {

		for _, metric := range scopeMetrics.GetMetrics() {
			switch metric.GetData().(type) {
			case *v1.Metric_Gauge:
				logs = append(logs, gauge2Logs(metric.GetName(), metric.GetGauge(), &labels)...)
			case *v1.Metric_Histogram:
				logs = append(logs, histogram2Logs(metric.GetName(), metric.GetHistogram(), &labels)...)
			case *v1.Metric_Sum:
				logs = append(logs, sum2Logs(metric.GetName(), metric.GetSum(), &labels)...)
			case *v1.Metric_Summary:
				logs = append(logs, summary2Logs(metric.GetName(), metric.GetSummary(), &labels)...)
			case *v1.Metric_ExponentialHistogram:
				logs = append(logs, exponentialHistogram2Logs(metric.GetName(), metric.GetExponentialHistogram(), &labels)...)
			}
		}
	}

	return logs, err
}

func exponentialHistogram2Logs(name string, histogram *v1.ExponentialHistogram, defaultLabels *helper.MetricLabels) (logs []*protocol.Log) {

	for _, dataPoint := range histogram.GetDataPoints() {
		labels := defaultLabels.Clone()
		attrsToLabels(labels, dataPoint.GetAttributes())
		// labels.Append(otlp.TagKeyMetricAggregationTemporality, histogram.GetAggregationTemporality().String())
		// labels.Append(otlp.TagKeyMetricHistogramType, pmetric.MetricTypeExponentialHistogram.String())

		if dataPoint.GetSum() != 0 {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixSum, labels, int64(dataPoint.GetTimeUnixNano()), dataPoint.GetSum()))
		}
		if dataPoint.GetMin() != 0 {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixMin, labels, int64(dataPoint.GetTimeUnixNano()), dataPoint.GetMin()))
		}
		if dataPoint.GetMax() != 0 {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixMax, labels, int64(dataPoint.GetTimeUnixNano()), dataPoint.GetMax()))
		}
		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixCount, labels, int64(dataPoint.GetTimeUnixNano()), float64(dataPoint.GetCount())))

		for _, exemplar := range dataPoint.GetExemplars() {
			logs = append(logs, exemplarMetricToLogs(name, exemplar, labels.Clone()))
		}

		scale := dataPoint.GetScale()
		base := math.Pow(2, math.Pow(2, float64(-scale)))
		postiveFields := genExponentialHistogramValuesToValues(true, base, dataPoint.GetPositive())
		negativeFields := genExponentialHistogramValuesToValues(false, base, dataPoint.GetNegative())

		bucketLabels := labels.Clone()
		bucketLabels.Append(bucketLabelKey, "")
		for k, v := range postiveFields {
			bucketLabels.Replace(bucketLabelKey, k)
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixBucket, bucketLabels, int64(dataPoint.GetTimeUnixNano()), v))
		}
		bucketLabels.Replace(bucketLabelKey, otlp.FieldZeroCount)
		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixBucket, bucketLabels, int64(dataPoint.GetTimeUnixNano()), float64(dataPoint.GetZeroCount())))
		for k, v := range negativeFields {
			bucketLabels.Replace(bucketLabelKey, k)
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixBucket, bucketLabels, int64(dataPoint.GetTimeUnixNano()), v))
		}
	}
	return logs
}

func genExponentialHistogramValuesToValues(isPositive bool, base float64, buckets *v1.ExponentialHistogramDataPoint_Buckets) map[string]float64 {
	offset := buckets.GetOffset()
	rawbucketCounts := buckets.GetBucketCounts()
	res := make(map[string]float64, len(rawbucketCounts))
	for i := 0; i < len(rawbucketCounts); i++ {
		bucketCount := rawbucketCounts[i]
		lowerBoundary := math.Pow(base, float64(int(offset)+i))
		upperBoundary := lowerBoundary * base
		fieldKey := otlp.ComposeBucketFieldName(lowerBoundary, upperBoundary, isPositive)
		res[fieldKey] = float64(bucketCount)
	}
	if isPositive {
		res[otlp.FieldPositiveOffset] = float64(offset)
	} else {
		res[otlp.FieldNegativeOffset] = float64(offset)
	}
	return res
}

func summary2Logs(name string, summary *v1.Summary, defaultLabels *helper.MetricLabels) (logs []*protocol.Log) {

	for _, dataPoint := range summary.GetDataPoints() {
		labels := defaultLabels.Clone()

		attrsToLabels(labels, dataPoint.GetAttributes())

		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixSum, labels, int64(toTimestamp(dataPoint.GetTimeUnixNano())), dataPoint.GetSum()))
		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixCount, labels, int64(toTimestamp(dataPoint.GetTimeUnixNano())), float64(dataPoint.GetCount())))

		summaryLabels := labels.Clone()
		summaryLabels.Append(summaryLabelKey, "")

		for _, quantileValue := range dataPoint.GetQuantileValues() {
			summaryLabels.Replace(summaryLabelKey, strconv.FormatFloat(quantileValue.GetQuantile(), 'g', -1, 64))
			logs = append(logs, newMetricLogFromRaw(name, summaryLabels, int64(dataPoint.GetTimeUnixNano()), quantileValue.GetValue()))
		}
	}

	return logs
}

func sum2Logs(name string, sum *v1.Sum, defaultLabels *helper.MetricLabels) (logs []*protocol.Log) {
	for _, dataPoint := range sum.GetDataPoints() {
		labels := defaultLabels.Clone()
		attrsToLabels(labels, dataPoint.GetAttributes())

		labels.Append(otlp.TagKeyMetricIsMonotonic, strconv.FormatBool(sum.GetIsMonotonic()))
		// labels.Append(otlp.TagKeyMetricAggregationTemporality, sum.GetAggregationTemporality().String())

		for _, exemplar := range dataPoint.GetExemplars() {
			logs = append(logs, exemplarMetricToLogs(name, exemplar, labels.Clone()))
		}

		logs = append(logs, newMetricLogFromRaw(name, labels, int64(toTimestamp(dataPoint.TimeUnixNano)), value2Float(dataPoint)))
	}

	return logs
}

func histogram2Logs(name string, histogram *v1.Histogram, defaultLabels *helper.MetricLabels) (logs []*protocol.Log) {

	for _, dataPoint := range histogram.GetDataPoints() {
		labels := defaultLabels.Clone()

		attrsToLabels(labels, dataPoint.GetAttributes())
		// labels.Append(otlp.TagKeyMetricAggregationTemporality, histogram.GetAggregationTemporality().String())
		// labels.Append(otlp.TagKeyMetricHistogramType, pmetric.MetricTypeHistogram.String())

		if dataPoint.GetSum() != 0 {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixSum, labels, int64(dataPoint.GetTimeUnixNano()), dataPoint.GetSum()))
		}
		if dataPoint.GetMin() != 0 {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixMin, labels, int64(dataPoint.GetTimeUnixNano()), dataPoint.GetMin()))
		}
		if dataPoint.GetMax() != 0 {
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixMax, labels, int64(dataPoint.GetTimeUnixNano()), dataPoint.GetMax()))
		}

		logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixCount, labels, int64(dataPoint.GetTimeUnixNano()), float64(dataPoint.GetCount())))

		for _, exemplar := range dataPoint.GetExemplars() {
			logs = append(logs, exemplarMetricToLogs(name, exemplar, labels.Clone()))
		}

		bounds := dataPoint.GetExplicitBounds()
		boundsStr := make([]string, len(bounds)+1)

		for i, bound := range bounds {
			boundsStr[i] = strconv.FormatFloat(bound, 'g', -1, 64)
		}
		boundsStr[len(boundsStr)-1] = infinityBoundValue

		bucketCount := min(len(boundsStr), len(dataPoint.GetBucketCounts()))

		bucketLabels := labels.Clone()
		bucketLabels.Append(bucketLabelKey, "")

		sumCount := uint64(0)

		for j := 0; j < bucketCount; j++ {
			bucket := dataPoint.GetBucketCounts()[j]
			bucketLabels.Replace(bucketLabelKey, boundsStr[j])
			sumCount += bucket
			logs = append(logs, newMetricLogFromRaw(name+metricNameSuffixBucket, bucketLabels, int64(dataPoint.GetTimeUnixNano()), float64(sumCount)))
		}

	}

	return logs
}

func gauge2Logs(name string, gauga *v1.Gauge, labels *helper.MetricLabels) (logs []*protocol.Log) {
	for _, dataPoint := range gauga.GetDataPoints() {

		newLabels := labels.Clone()
		attrsToLabels(newLabels, dataPoint.GetAttributes())

		for _, exemplar := range dataPoint.GetExemplars() {
			logs = append(logs, exemplarMetricToLogs(name, exemplar, newLabels.Clone()))
		}

		logs = append(logs, newMetricLogFromRaw(name, newLabels.Clone(), int64(toTimestamp(dataPoint.TimeUnixNano)), value2Float(dataPoint)))
	}
	return logs
}

func exemplarMetricToLogs(name string, exemplar *v1.Exemplar, labels *helper.MetricLabels) *protocol.Log {
	metricName := name + metricNameSuffixExemplars

	if hex.EncodeToString(exemplar.GetTraceId()) != "" {
		labels.Append("traceId", hex.EncodeToString(exemplar.GetTraceId()))
	}

	if hex.EncodeToString(exemplar.GetSpanId()) != "" {
		labels.Append("spanId", hex.EncodeToString(exemplar.GetSpanId()))
	}

	filterAttributeMap := pcommon.NewMap()
	copyAttributeToMap(exemplar.GetFilteredAttributes(), &filterAttributeMap)

	for key, value := range filterAttributeMap.AsRaw() {
		labels.Append(key, fmt.Sprintf("%v", value))
	}

	return helper.NewMetricLog(metricName, int64(exemplar.TimeUnixNano), exemplarValue2Float(exemplar), labels)
}

func copyAttributeToMap(attributes []*v1Common.KeyValue, attributeMap *pcommon.Map) {
	for _, attribute := range attributes {
		attributeMap.PutStr(attribute.GetKey(), anyValueToString(attribute.GetValue()))
	}
}

func exemplarValue2Float(exemplar *v1.Exemplar) float64 {
	switch exemplar.GetValue().(type) {
	case *v1.Exemplar_AsDouble:
		return exemplar.GetAsDouble()
	case *v1.Exemplar_AsInt:
		return float64(exemplar.GetAsInt())
	default:
		return 0
	}
}

func value2Float(dataPoint *v1.NumberDataPoint) float64 {
	switch dataPoint.GetValue().(type) {
	case *v1.NumberDataPoint_AsDouble:
		return dataPoint.GetAsDouble()
	case *v1.NumberDataPoint_AsInt:
		return float64(dataPoint.GetAsInt())
	default:
		return 0
	}
}

func toTimestamp(timUnixNano uint64) pcommon.Timestamp {
	return pcommon.Timestamp(timUnixNano)
}
