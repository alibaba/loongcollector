// Copyright 2024 iLogtail Authors
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

package prometheus

import (
	"errors"
	"strconv"
	"testing"

	"github.com/VictoriaMetrics/VictoriaMetrics/lib/prompb"
	pb "github.com/VictoriaMetrics/VictoriaMetrics/lib/prompbmarshal"
	"github.com/prometheus/common/model"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/models"
)

// 场景：性能测试，确定性能基线（UT粒度）
// 因子：所有 Event type 均为 models.EventTypeMetric
// 因子：所有 PipelineEvent interface 的实现均为 *models.Metric
// 预期：EncodeV2 和 EncodeBatchV2 性能相当（实现上 EncodeBatchV2 会循环调用 EncodeV2）
// Benchmark结果（每次在具体数值上可能会有差异，但数量级相同）：
// goos: darwin
// goarch: arm64
// pkg: github.com/alibaba/ilogtail/pkg/protocol/encoder/prometheus
// BenchmarkV2Encode
// BenchmarkV2Encode/EncodeV2
// BenchmarkV2Encode/EncodeV2-12         	     685	   1655657 ns/op
// BenchmarkV2Encode/BatchEncodeV2
// BenchmarkV2Encode/BatchEncodeV2-12    	     716	   1639491 ns/op
// PASS
func BenchmarkV2Encode(b *testing.B) {
	// given
	p := NewPromEncoder(19)
	groupEventsSlice := genNormalPipelineGroupEventsSlice(100)
	want := append([]*models.PipelineGroupEvents(nil), groupEventsSlice...)

	b.Run("EncodeV2", func(b *testing.B) {
		b.ResetTimer()

		for i := 0; i < b.N; i++ {
			for _, groupEvents := range groupEventsSlice {
				p.EncodeV2(groupEvents)
			}
		}
	})
	assert.Equal(b, want, groupEventsSlice)

	b.Run("BatchEncodeV2", func(b *testing.B) {
		b.ResetTimer()

		for i := 0; i < b.N; i++ {
			p.EncodeBatchV2(groupEventsSlice)
		}
	})
	assert.Equal(b, want, groupEventsSlice)
}

// 场景：V2 Encode接口功能测试
// 说明：EncodeBatchV2 内部会调用 EncodeV2，所以也同时测试了 EncodeV2 的正常逻辑的功能
// 因子：所有 Event type 均为 models.EventTypeMetric
// 因子：所有 PipelineEvent interface 的实现均为「正常的*models.Metric」（而不是new(models.Metric)），
// 具体区别在于正常的*models.Metric，其中的Tags、Value等都不是nil（如果为nil，会触发序列化的异常逻辑）
// 预期：V2 Encode逻辑正常（正常流程都能正确处理），返回的error类型为nil，[][]byte不为nil
func TestV2Encode_ShouldReturnNoError_GivenNormalDataOfPipelineGroupEvents(t *testing.T) {
	// given
	groupEventsSlice1 := genNormalPipelineGroupEventsSlice(100)
	groupEventsSlice2 := genNormalPipelineGroupEventsSlice(100)
	p := NewPromEncoder(19)

	// when
	// then
	data1, err1 := p.EncodeBatchV2(groupEventsSlice1)
	assert.NoError(t, err1)
	data2, err2 := p.EncodeBatchV2(groupEventsSlice2)
	assert.NoError(t, err2)

	assert.Equal(t, len(data2), len(data1))
}

// 场景：V2 Encode接口功能测试（异常数据，非全nil或0值）
// 说明：尽管 EncodeBatchV2 内部会调用 EncodeV2，但异常情况可能是 EncodeBatchV2 侧的防御，
// 所以还需要测试异常情况下 EncodeV2 的功能
// 因子：并非所有 Event type 均为 models.EventTypeMetric（e.g. 还可能是 models.EventTypeLogging 等）
// 因子：PipelineEvent interface 的实现，部分是「正常的*models.Metric」，部分为 nil，部分为new(models.Metric)，
// 部分为其它（e.g. *models.Log 等）
// 预期：Encode逻辑正常（异常流程也能正确处理），返回的error类型不为nil，[][]byte为nil
func TestV2Encode_ShouldReturnError_GivenAbNormalDataOfPipelineGroupEvents(t *testing.T) {
	// given
	groupEventsSlice1 := genPipelineGroupEventsSliceIncludingAbnormalData(100)
	groupEventsSlice2 := genPipelineGroupEventsSliceIncludingAbnormalData(100)
	assert.Equal(t, len(groupEventsSlice1), len(groupEventsSlice2))
	p := NewPromEncoder(19)

	// when
	// then
	t.Run("Test EncodeV2 with abnormal data input", func(t *testing.T) {
		for i, groupEvents := range groupEventsSlice1 {
			data1, err1 := p.EncodeV2(groupEvents)
			data2, err2 := p.EncodeV2(groupEventsSlice2[i])
			if err1 != nil {
				assert.Error(t, err2)
				assert.Equal(t, err1, err2)
			} else {
				assert.NoError(t, err2)
				assert.Equal(t, len(data2), len(data1))
			}
		}
	})

	t.Run("Test EncodeBatchV2 with abnormal data input", func(t *testing.T) {
		data1, err1 := p.EncodeBatchV2(groupEventsSlice1)
		assert.NoError(t, err1)
		data2, err2 := p.EncodeBatchV2(groupEventsSlice2)
		assert.NoError(t, err2)

		assert.Equal(t, len(data2), len(data1))
	})
}

// 场景：V2 Encode接口功能测试（异常数据，全nil或0值）
// 说明：尽管 EncodeBatchV2 内部会调用 EncodeV2，但异常情况可能是 EncodeBatchV2 侧的防御，
// 所以还需要测试异常情况下 EncodeV2 的功能
// 因子：所有 *models.PipelineGroupEvents 及 []*models.PipelineGroupEvents 底层为 nil 或者 长度为0的切片
// 预期：Encode逻辑正常（异常流程也能正确处理），返回的error类型不为nil，[][]byte为nil
func TestV2Encode_ShouldReturnError_GivenNilOrZeroDataOfPipelineGroupEvents(t *testing.T) {
	// given
	p := NewPromEncoder(19)
	nilOrZeroGroupEventsSlices := []*models.PipelineGroupEvents{
		nil,
		{}, // same as {Events: nil},
		{Events: make([]models.PipelineEvent, 0)},
	}
	nilOrZeroGroupEventsSlicesEx := [][]*models.PipelineGroupEvents{
		nil,
		{}, // same as {nil}
		{{Events: nil}},
		nilOrZeroGroupEventsSlices,
	}

	// when
	// then
	t.Run("Test EncodeV2 with nil or zero data input", func(t *testing.T) {
		for _, input := range nilOrZeroGroupEventsSlices {
			data, err := p.EncodeV2(input)
			assert.Error(t, err)
			assert.Nil(t, data)
		}
	})

	t.Run("Test EncodeBatchV2 with nil or zero data input", func(t *testing.T) {
		for _, input := range nilOrZeroGroupEventsSlicesEx {
			data, err := p.EncodeBatchV2(input)
			assert.Error(t, err)
			assert.Nil(t, data)
		}
	})
}

// 场景：V2 Encode接口功能测试
// 说明：EncoderBatchV2 内部会调用 EncoderV2，所以也同时测试了 EncoderV2 的功能
// 因子：所有 Event type 均为 models.EventTypeMetric
// 因子：所有 PipelineEvent interface 的实现均为 *models.Metric
// 因子：每个 metric event 生成的 *models.Metric.Tags 中的 tag 仅一个
// （确保 Encode 时 range map不用考虑 range go原生map每次顺序随机，从而导致2次Encode相同数据后得到的结果不同）
// PS：如果不这么做，就要对map做转化，先变成 range 保序的 slice，再 Encode；
// 但测试的功能点是Encode，所以采用上述方法绕过range go原生map每次顺序随机的特点，完成功能测试
// 预期：Encode逻辑正常（正常流程都能正确处理），返回的error类型为nil，[][]byte不为nil，且两次Encode后返回的数据相同
func TestEncoderBatchV2_ShouldReturnNoErrorAndEqualData_GivenNormalDataOfDataOfPipelineGroupEventsWithSingleTag(t *testing.T) {
	// given
	groupEventsSlice1 := genPipelineGroupEventsSliceSingleTag(100)
	groupEventsSlice2 := genPipelineGroupEventsSliceSingleTag(100)
	p := NewPromEncoder(19)

	// when
	// then
	data1, err1 := p.EncodeBatchV2(groupEventsSlice1)
	assert.NoError(t, err1)
	data2, err2 := p.EncodeBatchV2(groupEventsSlice2)
	assert.NoError(t, err2)

	assert.Equal(t, data2, data1)
}

// 场景：V2 Encode接口功能测试
// 说明：EncoderBatchV2 内部会调用 EncoderV2，所以也同时测试了 EncoderV2 的功能
// 因子：所有 Event type 均为 models.EventTypeMetric
// 因子：所有 PipelineEvent interface 的实现均为 *models.Metric
// 因子：每个 metric event 生成的 *models.Metric.Tags 中的 tag 仅一个
// （确保 Encode 时 range map不用考虑 range go原生map每次顺序随机，从而导致2次Encode相同数据后得到的结果不同）
// PS：如果不这么做，就要对map做转化，先变成 range 保序的 slice，再 Encode；
// 但测试的功能点是Encode，所以采用上述方法绕过range go原生map每次顺序随机的特点，完成功能测试
// 因子：对Encode后的数据进行Decode
// 因子：「构造的用例数据」的长度（len([]*models.PipelineGroupEvents)）未超过 series limit
// 预期：「构造的用例数据」和「对用例数据先Encode再Decode后的数据」相等
func TestEncoderBatchV2_ShouldDecodeSuccess_GivenNormalDataOfDataOfPipelineGroupEventsWithSingleTagNotExceedingSeriesLimit(t *testing.T) {
	// given
	seriesLimit := 19
	n := seriesLimit
	wantGroupEventsSlice := genPipelineGroupEventsSliceSingleTag(n)
	p := NewPromEncoder(seriesLimit)
	data, err := p.EncodeBatchV2(wantGroupEventsSlice)
	assert.NoError(t, err)

	// when
	// then
	gotGroupEventsSlice, err := DecodeBatchV2(data)
	assert.NoError(t, err)
	assert.Equal(t, wantGroupEventsSlice, gotGroupEventsSlice)
}

// 场景：V2 Encode接口功能测试
// 说明：EncoderBatchV2 内部会调用 EncoderV2，所以也同时测试了 EncoderV2 的功能
// 因子：所有 Event type 均为 models.EventTypeMetric
// 因子：所有 PipelineEvent interface 的实现均为 *models.Metric
// 因子：每个 metric event 生成的 *models.Metric.Tags 中的 tag 仅一个
// （确保 Encode 时 range map不用考虑 range go原生map每次顺序随机，从而导致2次Encode相同数据后得到的结果不同）
// PS：如果不这么做，就要对map做转化，先变成 range 保序的 slice，再 Encode；
// 但测试的功能点是Encode，所以采用上述方法绕过range go原生map每次顺序随机的特点，完成功能测试
// 因子：对Encode后的数据进行Decode
// 因子：「构造的用例数据」的长度（len([]*models.PipelineGroupEvents)）超过 series limit
// 预期：「构造的用例数据」的长度小于「对用例数据先Encode再Decode后的数据」的长度，且用 expectedLen 计算后的长度相等
// PS：expectedLen 的计算方法，其实是和 genPipelineGroupEventsSlice 生成用例及根据series limit确定encode批次
// 的逻辑相关，和 Encode 本身的逻辑无关
func TestEncoderBatchV2_ShouldDecodeSuccess_GivenNormalDataOfDataOfPipelineGroupEventsWithSingleTagExceedingSeriesLimit(t *testing.T) {
	// given
	seriesLimit := 19
	n := 100
	wantGroupEventsSlice := genPipelineGroupEventsSliceSingleTag(n)
	assert.Equal(t, n, len(wantGroupEventsSlice))
	p := NewPromEncoder(seriesLimit)
	data, err := p.EncodeBatchV2(wantGroupEventsSlice)
	assert.NoError(t, err)
	expectedLen := func(limit, length int) int {
		// make sure limit > 0 && length > 0
		if limit <= 0 || length <= 0 {
			return -1
		}

		mod := length % limit
		mul := length / limit

		res := 0
		for i := 0; i <= mul; i++ {
			res += i * limit
		}
		res += (mul + 1) * mod

		return res
	}

	// when
	gotGroupEventsSlice, err := DecodeBatchV2(data)
	assert.NoError(t, err)

	// then
	assert.Equal(t, expectedLen(seriesLimit, n), len(gotGroupEventsSlice))
}

func genNormalPipelineGroupEventsSlice(n int) []*models.PipelineGroupEvents {
	return genPipelineGroupEventsSlice(n, genPipelineEvent)
}

func genPipelineGroupEventsSliceIncludingAbnormalData(n int) []*models.PipelineGroupEvents {
	return genPipelineGroupEventsSlice(n, genPipelineEventIncludingAbnormalData)
}

func genPipelineGroupEventsSliceSingleTag(n int) []*models.PipelineGroupEvents {
	return genPipelineGroupEventsSlice(n, genPipelineEventSingleTag)
}

func genPipelineGroupEventsSlice(n int, genPipelineEventFn func(int) []models.PipelineEvent) []*models.PipelineGroupEvents {
	res := make([]*models.PipelineGroupEvents, 0, n)
	for i := 1; i <= n; i++ {
		res = append(res, &models.PipelineGroupEvents{
			Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
			Events: genPipelineEventFn(i),
		})
	}

	return res
}

func genPipelineEvent(n int) []models.PipelineEvent {
	res := make([]models.PipelineEvent, 0, n)
	for i := 1; i <= n; i++ {
		res = append(res, genMetric(i))
	}

	return res
}

func genMetric(n int) *models.Metric {
	i := strconv.Itoa(n)
	tags := models.NewKeyValues[string]()
	tags.AddAll(map[string]string{
		// range map will out of order
		"a" + i: "A" + i,
		"b" + i: "B" + i,
		"c" + i: "C" + i,
		"d" + i: "D" + i,
	})

	return &models.Metric{
		Timestamp: 11111111 * uint64(n),
		Tags:      tags,
		Value:     &models.MetricSingleValue{Value: 1.1 * float64(n)},
	}
}

func genPipelineEventIncludingAbnormalData(n int) []models.PipelineEvent {
	res := make([]models.PipelineEvent, 0, n)
	for i := 1; i <= n; i++ {
		if i&1 == 0 { // i is even number
			// normal data
			res = append(res, genMetric(i))
			continue
		}

		// i is odd number
		// abnormal data
		if i%3 == 0 {
			// abnormal data: nil data
			res = append(res, nil)
			continue
		}

		if i%5 == 0 {
			// abnormal data: zero data
			// PS：
			// 1. 这里只是从边界情况考虑，构造了这种异常值
			// 但实际场景中，不会直接 new(models.Metric) 或者 &models.Metric{} 这样创建 zero data，
			// 一般都是用 models.NewMetric|NewSingleValueMetric|NewMultiValuesMetric 等 构造函数（工厂模式）来创建，
			// 上述构造函数位置：ilogtail/pkg/models/factory.go
			// 2. 此外，也可以给 *models.Metric 的 GetTag 方法增加下 *models.Metric.Tag 为 nil 时的保护
			// （参考其 GetValue 方法的实现），文件位置：ilogtail/pkg/models/metric.go
			res = append(res, new(models.Metric))
			continue
		}

		// abnormal data: other event type not models.EventTypeMetric
		res = append(res, new(models.Log))
	}

	return res
}

func genPipelineEventSingleTag(n int) []models.PipelineEvent {
	res := make([]models.PipelineEvent, 0, n)
	for i := 1; i <= n; i++ {
		res = append(res, genMetricSingleTag(i))
	}

	return res
}

func genMetricSingleTag(n int) *models.Metric {
	metricName := "test_metric"
	i := strconv.Itoa(n)
	tags := models.NewTagsWithMap(map[string]string{
		// only single tag
		// keep range in order
		"x" + i: "X" + i,
	})

	dataPoint := pb.Sample{Timestamp: 11111111 * int64(n), Value: 1.1 * float64(n)}

	return models.NewSingleValueMetric(
		metricName, // value of key "__name__" in prometheus
		models.MetricTypeGauge,
		tags,
		model.Time(dataPoint.Timestamp).Time().UnixNano(),
		dataPoint.Value,
	)
}

func DecodeBatchV2(data [][]byte) ([]*models.PipelineGroupEvents, error) {
	if len(data) == 0 {
		return nil, errors.New("no data to decode")
	}

	var res []*models.PipelineGroupEvents

	meta, commonTags := models.NewMetadata(), models.NewTags()
	for _, d := range data {
		groupEvents, err := convertPromRequestToPipelineGroupEvents(d, meta, commonTags)
		if err != nil {
			continue
		}

		res = append(res, groupEvents)
	}

	return res, nil
}

func convertPromRequestToPipelineGroupEvents(data []byte, metaInfo models.Metadata, commonTags models.Tags) (*models.PipelineGroupEvents, error) {
	wr, err := unmarshalBatchTimeseriesData(data)
	if err != nil {
		return nil, err
	}

	groupEvent := &models.PipelineGroupEvents{
		Group: models.NewGroup(metaInfo, commonTags),
	}

	for _, ts := range wr.Timeseries {
		var metricName string
		tags := models.NewTags()
		for _, label := range ts.Labels {
			if label.Name == metricNameKey {
				metricName = label.Value
				continue
			}
			tags.Add(label.Name, label.Value)
		}

		for _, dataPoint := range ts.Samples {
			metric := models.NewSingleValueMetric(
				metricName,
				models.MetricTypeGauge,
				tags,
				// Decode (during input_prometheus stage) makes timestamp
				// with unix milliseconds into unix nanoseconds,
				// e.g. "model.Time(milliseconds).Time().UnixNano()".
				model.Time(dataPoint.Timestamp).Time().UnixNano(),
				dataPoint.Value,
			)
			groupEvent.Events = append(groupEvent.Events, metric)
		}
	}

	return groupEvent, nil
}

func unmarshalBatchTimeseriesData(data []byte) (*pb.WriteRequest, error) {
	wr := new(prompb.WriteRequest)
	if err := wr.Unmarshal(data); err != nil {
		return nil, err
	}

	return convertPrompbToVictoriaMetricspb(wr)
}

func convertPrompbToVictoriaMetricspb(wr *prompb.WriteRequest) (*pb.WriteRequest, error) {
	if wr == nil || len(wr.Timeseries) == 0 {
		return nil, errors.New("nil *prompb.WriteRequest")
	}

	res := &pb.WriteRequest{
		Timeseries: make([]pb.TimeSeries, 0, len(wr.Timeseries)),
	}
	for _, tss := range wr.Timeseries {
		res.Timeseries = append(res.Timeseries, pb.TimeSeries{
			Labels:  convertToVMLabels(tss.Labels),
			Samples: convertToVMSamples(tss.Samples),
		})
	}

	return res, nil
}

func convertToVMLabels(labels []prompb.Label) []pb.Label {
	if len(labels) == 0 {
		return nil
	}

	res := make([]pb.Label, 0, len(labels))
	for _, label := range labels {
		res = append(res, pb.Label{
			Name:  string(label.Name),
			Value: string(label.Value),
		})
	}

	return res
}

func convertToVMSamples(samples []prompb.Sample) []pb.Sample {
	if len(samples) == 0 {
		return nil
	}

	res := make([]pb.Sample, 0, len(samples))
	for _, sample := range samples {
		res = append(res, pb.Sample{
			Value:     sample.Value,
			Timestamp: sample.Timestamp,
		})
	}

	return res
}
