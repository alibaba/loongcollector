// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      grpc://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package opentelemetry

import (
	"bytes"
	"context"
	"fmt"
	"net/http"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pluginmanager"
	"github.com/alibaba/ilogtail/plugins/test"
)

type ContextTest struct {
	pluginmanager.ContextImp

	Logs []string
}

func (p *ContextTest) LogWarnf(alarmType string, format string, params ...interface{}) {
	p.Logs = append(p.Logs, alarmType+":"+fmt.Sprintf(format, params...))
}

func (p *ContextTest) LogErrorf(alarmType string, format string, params ...interface{}) {
	p.Logs = append(p.Logs, alarmType+":"+fmt.Sprintf(format, params...))
}

func (p *ContextTest) LogWarn(alarmType string, kvPairs ...interface{}) {
	fmt.Println(alarmType, kvPairs)
}

type op func(*Server)

func setCompression(cp string) op {
	return func(s *Server) {
		if s.Protocals.GRPC != nil {
			s.Protocals.GRPC.Compression = cp
		}
	}
}

func setDecompression(cp string) op {
	return func(s *Server) {
		if s.Protocals.GRPC != nil {
			s.Protocals.GRPC.Decompression = cp
		}
	}
}

func newInput(enableGRPC, enableHTTP bool, grpcEndpoint, httpEndpoint string, ops ...op) (*Server, error) {
	ctx := &ContextTest{}
	ctx.ContextImp.InitContext("a", "b", "c")
	s := &Server{
		Protocals: Protocals{},
	}
	if enableGRPC {
		s.Protocals.GRPC = &helper.GRPCServerSettings{
			Endpoint: grpcEndpoint,
		}
	}

	if enableHTTP {
		s.Protocals.HTTP = &HTTPServerSettings{
			Endpoint: httpEndpoint,
		}
	}

	for _, op := range ops {
		op(s)
	}
	_, err := s.Init(&ctx.ContextImp)
	return s, err
}

func TestOtlpGRPC_Logs(t *testing.T) {
	endpointGrpc := test.GetAvailableLocalAddress(t)
	input, err := newInput(true, false, endpointGrpc, "")
	assert.NoError(t, err)

	queueSize := 10
	pipelineCxt := helper.NewObservePipelineConext(queueSize)
	input.StartService(pipelineCxt)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	},
	)

	cc, err := grpc.Dial(endpointGrpc, grpc.WithTransportCredentials(insecure.NewCredentials()))
	assert.NoError(t, err)
	defer func() {
		assert.NoError(t, cc.Close())
	}()

	for i := 0; i < queueSize; i++ {
		err = exportLogs(cc, GenerateLogs(i+1))
		assert.NoError(t, err)
	}

	count := 0

	for groupEvent := range pipelineCxt.Collector().Observe() {
		assert.Equal(t, "resource-attr-val-1", groupEvent.Group.Metadata.Get("resource-attr"))
		for _, v := range groupEvent.Events {
			assert.Equal(t, models.EventTypeLogging, v.GetType())
		}
		count++
		if count == queueSize {
			break
		}
	}
}

func TestOtlpGRPC_Trace(t *testing.T) {
	endpointGrpc := test.GetAvailableLocalAddress(t)
	input, err := newInput(true, false, endpointGrpc, "")
	assert.NoError(t, err)

	queueSize := 10
	pipelineCxt := helper.NewObservePipelineConext(queueSize)
	input.StartService(pipelineCxt)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	},
	)

	cc, err := grpc.Dial(endpointGrpc, grpc.WithTransportCredentials(insecure.NewCredentials()))
	assert.NoError(t, err)
	defer func() {
		assert.NoError(t, cc.Close())
	}()

	for i := 0; i < queueSize; i++ {
		err = exportTraces(cc, GenerateTraces(i+1))
		assert.NoError(t, err)
	}

	count := 0

	for groupEvent := range pipelineCxt.Collector().Observe() {
		assert.Equal(t, "resource-attr-val-1", groupEvent.Group.Metadata.Get("resource-attr"))
		assert.Equal(t, count+1, len(groupEvent.Events))
		assert.Equal(t, count+1, len(groupEvent.Events))
		for _, v := range groupEvent.Events {
			assert.Equal(t, models.EventTypeSpan, v.GetType())
		}
		count++
		if count == queueSize {
			break
		}
	}
}

func TestOtlpGRPC_Metrics(t *testing.T) {
	endpointGrpc := test.GetAvailableLocalAddress(t)
	input, err := newInput(true, false, endpointGrpc, "")
	assert.NoError(t, err)

	queueSize := 10
	pipelineCxt := helper.NewObservePipelineConext(queueSize)
	input.StartService(pipelineCxt)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	},
	)

	cc, err := grpc.Dial(endpointGrpc, grpc.WithTransportCredentials(insecure.NewCredentials()))
	assert.NoError(t, err)
	defer func() {
		assert.NoError(t, cc.Close())
	}()

	for i := 0; i < queueSize; i++ {
		err = exportMetrics(cc, GenerateMetrics(i+1))
		assert.NoError(t, err)
	}

	count := 0

	for groupEvent := range pipelineCxt.Collector().Observe() {
		assert.Equal(t, "resource-attr-val-1", groupEvent.Group.Metadata.Get("resource-attr"))
		for _, v := range groupEvent.Events {
			assert.Equal(t, models.EventTypeMetric, v.GetType())
		}
		count++
		if count == queueSize {
			break
		}
	}
}

func TestOtlpHTTP_Metrics(t *testing.T) {
	endpointHTTP := test.GetAvailableLocalAddress(t)
	input, err := newInput(false, true, "", endpointHTTP)
	assert.NoError(t, err)

	queueSize := 10
	pipelineCxt := helper.NewObservePipelineConext(queueSize)
	input.StartService(pipelineCxt)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	})

	client := &http.Client{}
	url := fmt.Sprintf("http://%s/v1/metrics", endpointHTTP)

	for i := 0; i < queueSize; i++ {
		req := pmetricotlp.NewExportRequestFromMetrics(GenerateMetrics(i + 1))
		err = httpExport(client, req, url, i%2 == 0)
		assert.NoError(t, err)
	}

	count := 0

	for groupEvent := range pipelineCxt.Collector().Observe() {
		assert.Equal(t, "resource-attr-val-1", groupEvent.Group.Metadata.Get("resource-attr"))
		for _, v := range groupEvent.Events {
			assert.Equal(t, models.EventTypeMetric, v.GetType())
		}
		count++
		if count == queueSize {
			break
		}
	}
}

func TestOtlpHTTP_Trace(t *testing.T) {
	endpointHTTP := test.GetAvailableLocalAddress(t)
	input, err := newInput(false, true, "", endpointHTTP)
	assert.NoError(t, err)

	queueSize := 10
	pipelineCxt := helper.NewObservePipelineConext(queueSize)
	input.StartService(pipelineCxt)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	})

	client := &http.Client{}
	url := fmt.Sprintf("http://%s/v1/traces", endpointHTTP)

	for i := 0; i < queueSize; i++ {
		req := ptraceotlp.NewExportRequestFromTraces(GenerateTraces(i + 1))
		err = httpExport(client, req, url, i%2 == 0)
		assert.NoError(t, err)
	}

	count := 0
	for groupEvent := range pipelineCxt.Collector().Observe() {
		assert.Equal(t, "resource-attr-val-1", groupEvent.Group.Metadata.Get("resource-attr"))
		assert.Equal(t, count+1, len(groupEvent.Events))
		assert.Equal(t, count+1, len(groupEvent.Events))
		for _, v := range groupEvent.Events {
			assert.Equal(t, models.EventTypeSpan, v.GetType())
		}
		count++
		if count == queueSize {
			break
		}
	}
}

func httpExport[P interface {
	MarshalProto() ([]byte, error)
	MarshalJSON() ([]byte, error)
}](cli *http.Client, eReq P, url string, usePB bool) error {
	var eReqBytes []byte
	var err error
	if usePB {
		eReqBytes, err = eReq.MarshalProto()
	} else {
		eReqBytes, err = eReq.MarshalJSON()
	}

	if err != nil {
		return err
	}
	buf := bytes.NewBuffer(eReqBytes)
	req, err := http.NewRequest(http.MethodPost, url, buf)
	if err != nil {
		return err
	}

	if usePB {
		req.Header.Set("Content-Type", pbContentType)
	} else {
		req.Header.Set("Content-Type", jsonContentType)
	}

	resp, err := cli.Do(req)
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("response_status_not_ok")
	}
	return err
}

func exportLogs(cc *grpc.ClientConn, ld plog.Logs) error {
	acc := plogotlp.NewGRPCClient(cc)
	req := plogotlp.NewExportRequestFromLogs(ld)
	_, err := acc.Export(context.Background(), req)
	return err
}

func exportTraces(cc *grpc.ClientConn, td ptrace.Traces) error {
	acc := ptraceotlp.NewGRPCClient(cc)
	req := ptraceotlp.NewExportRequestFromTraces(td)
	_, err := acc.Export(context.Background(), req)

	return err
}

func exportMetrics(cc *grpc.ClientConn, md pmetric.Metrics) error {
	acc := pmetricotlp.NewGRPCClient(cc)
	req := pmetricotlp.NewExportRequestFromMetrics(md)
	_, err := acc.Export(context.Background(), req)
	return err
}

func GenerateLogs(logCount int) plog.Logs {
	ld := plog.NewLogs()
	initResource(ld.ResourceLogs().AppendEmpty().Resource())
	slog := ld.ResourceLogs().At(0).ScopeLogs().AppendEmpty().LogRecords()
	for i := 0; i < logCount; i++ {
		newLogs := slog.AppendEmpty()
		newLogs.Body().SetStr(fmt.Sprintf("log body %d", i))
	}
	return ld
}

func GenerateTraces(spanCount int) ptrace.Traces {
	td := ptrace.NewTraces()
	initResource(td.ResourceSpans().AppendEmpty().Resource())
	ss := td.ResourceSpans().At(0).ScopeSpans().AppendEmpty().Spans()
	ss.EnsureCapacity(spanCount)
	for i := 0; i < spanCount; i++ {
		switch i % 2 {
		case 0:
			fillSpanOne(ss.AppendEmpty())
		case 1:
			fillSpanTwo(ss.AppendEmpty())
		}
	}
	return td
}

var (
	spanStartTimestamp = pcommon.NewTimestampFromTime(time.Date(2020, 2, 11, 20, 26, 12, 321, time.UTC))
	spanEventTimestamp = pcommon.NewTimestampFromTime(time.Date(2020, 2, 11, 20, 26, 13, 123, time.UTC))
	spanEndTimestamp   = pcommon.NewTimestampFromTime(time.Date(2020, 2, 11, 20, 26, 13, 789, time.UTC))
)

var (
	metricStartTimestamp    = pcommon.NewTimestampFromTime(time.Date(2020, 2, 11, 20, 26, 12, 321, time.UTC))
	metricExemplarTimestamp = pcommon.NewTimestampFromTime(time.Date(2020, 2, 11, 20, 26, 13, 123, time.UTC))
	metricTimestamp         = pcommon.NewTimestampFromTime(time.Date(2020, 2, 11, 20, 26, 13, 789, time.UTC))
)

func initResource(r pcommon.Resource) {
	r.Attributes().PutStr("resource-attr", "resource-attr-val-1")
}

func fillSpanOne(span ptrace.Span) {
	span.SetName("operationA")
	span.SetStartTimestamp(spanStartTimestamp)
	span.SetEndTimestamp(spanEndTimestamp)
	span.SetDroppedAttributesCount(1)
	span.SetTraceID([16]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10})
	span.SetSpanID([8]byte{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18})
	evs := span.Events()
	ev0 := evs.AppendEmpty()
	ev0.SetTimestamp(spanEventTimestamp)
	ev0.SetName("event-with-attr")
	ev0.Attributes().PutStr("span-event-attr", "span-event-attr-val")
	ev0.SetDroppedAttributesCount(2)
	ev1 := evs.AppendEmpty()
	ev1.SetTimestamp(spanEventTimestamp)
	ev1.SetName("event")
	ev1.SetDroppedAttributesCount(2)
	span.SetDroppedEventsCount(1)
	status := span.Status()
	status.SetCode(ptrace.StatusCodeError)
	status.SetMessage("status-canceled")
}

func fillSpanTwo(span ptrace.Span) {
	span.SetName("operationB")
	span.SetStartTimestamp(spanStartTimestamp)
	span.SetEndTimestamp(spanEndTimestamp)
	link0 := span.Links().AppendEmpty()
	link0.Attributes().PutStr("span-link-attr", "span-link-attr-val")
	link0.SetDroppedAttributesCount(4)
	link1 := span.Links().AppendEmpty()
	link1.SetDroppedAttributesCount(4)
	span.SetDroppedLinksCount(3)
}

const (
	TestGaugeDoubleMetricName          = "gauge-double"
	TestGaugeIntMetricName             = "gauge-int"
	TestSumDoubleMetricName            = "sum-double"
	TestSumIntMetricName               = "sum-int"
	TestHistogramMetricName            = "histogram"
	TestExponentialHistogramMetricName = "exponential-histogram"
	TestSummaryMetricName              = "summary"
)

func generateMetricsOneEmptyInstrumentationScope() pmetric.Metrics {
	md := pmetric.NewMetrics()
	initResource(md.ResourceMetrics().AppendEmpty().Resource())
	md.ResourceMetrics().At(0).ScopeMetrics().AppendEmpty()
	return md
}

func GenerateMetrics(count int) pmetric.Metrics {
	md := generateMetricsOneEmptyInstrumentationScope()
	ms := md.ResourceMetrics().At(0).ScopeMetrics().At(0).Metrics()
	ms.EnsureCapacity(count)
	for i := 0; i < count; i++ {
		switch i % 7 {
		case 0:
			initGaugeIntMetric(ms.AppendEmpty())
		case 1:
			initGaugeDoubleMetric(ms.AppendEmpty())
		case 2:
			initSumIntMetric(ms.AppendEmpty())
		case 3:
			initSumDoubleMetric(ms.AppendEmpty())
		case 4:
			initHistogramMetric(ms.AppendEmpty())
		case 5:
			initExponentialHistogramMetric(ms.AppendEmpty())
		case 6:
			initSummaryMetric(ms.AppendEmpty())
		}
	}
	return md
}

func initGaugeIntMetric(im pmetric.Metric) {
	initMetric(im, TestGaugeIntMetricName, pmetric.MetricTypeGauge)

	idps := im.Gauge().DataPoints()
	idp0 := idps.AppendEmpty()
	initMetricAttributes1(idp0.Attributes())
	idp0.SetStartTimestamp(metricStartTimestamp)
	idp0.SetTimestamp(metricTimestamp)
	idp0.SetIntValue(123)
	idp1 := idps.AppendEmpty()
	initMetricAttributes2(idp1.Attributes())
	idp1.SetStartTimestamp(metricStartTimestamp)
	idp1.SetTimestamp(metricTimestamp)
	idp1.SetIntValue(456)
}

func initGaugeDoubleMetric(im pmetric.Metric) {
	initMetric(im, TestGaugeDoubleMetricName, pmetric.MetricTypeGauge)

	idps := im.Gauge().DataPoints()
	idp0 := idps.AppendEmpty()
	initMetricAttributes12(idp0.Attributes())
	idp0.SetStartTimestamp(metricStartTimestamp)
	idp0.SetTimestamp(metricTimestamp)
	idp0.SetDoubleValue(1.23)
	idp1 := idps.AppendEmpty()
	initMetricAttributes13(idp1.Attributes())
	idp1.SetStartTimestamp(metricStartTimestamp)
	idp1.SetTimestamp(metricTimestamp)
	idp1.SetDoubleValue(4.56)
}

func initSumIntMetric(im pmetric.Metric) {
	initMetric(im, TestSumIntMetricName, pmetric.MetricTypeSum)

	idps := im.Sum().DataPoints()
	idp0 := idps.AppendEmpty()
	initMetricAttributes1(idp0.Attributes())
	idp0.SetStartTimestamp(metricStartTimestamp)
	idp0.SetTimestamp(metricTimestamp)
	idp0.SetIntValue(123)
	idp1 := idps.AppendEmpty()
	initMetricAttributes2(idp1.Attributes())
	idp1.SetStartTimestamp(metricStartTimestamp)
	idp1.SetTimestamp(metricTimestamp)
	idp1.SetIntValue(456)
}

func initSumDoubleMetric(dm pmetric.Metric) {
	initMetric(dm, TestSumDoubleMetricName, pmetric.MetricTypeSum)

	ddps := dm.Sum().DataPoints()
	ddp0 := ddps.AppendEmpty()
	initMetricAttributes12(ddp0.Attributes())
	ddp0.SetStartTimestamp(metricStartTimestamp)
	ddp0.SetTimestamp(metricTimestamp)
	ddp0.SetDoubleValue(1.23)

	ddp1 := ddps.AppendEmpty()
	initMetricAttributes13(ddp1.Attributes())
	ddp1.SetStartTimestamp(metricStartTimestamp)
	ddp1.SetTimestamp(metricTimestamp)
	ddp1.SetDoubleValue(4.56)
}

func initHistogramMetric(hm pmetric.Metric) {
	initMetric(hm, TestHistogramMetricName, pmetric.MetricTypeHistogram)

	hdps := hm.Histogram().DataPoints()
	hdp0 := hdps.AppendEmpty()
	initMetricAttributes13(hdp0.Attributes())
	hdp0.SetStartTimestamp(metricStartTimestamp)
	hdp0.SetTimestamp(metricTimestamp)
	hdp0.SetCount(1)
	hdp0.SetSum(15)

	hdp1 := hdps.AppendEmpty()
	initMetricAttributes2(hdp1.Attributes())
	hdp1.SetStartTimestamp(metricStartTimestamp)
	hdp1.SetTimestamp(metricTimestamp)
	hdp1.SetCount(1)
	hdp1.SetSum(15)
	hdp1.SetMin(15)
	hdp1.SetMax(15)
	hdp1.BucketCounts().FromRaw([]uint64{0, 1})
	exemplar := hdp1.Exemplars().AppendEmpty()
	exemplar.SetTimestamp(metricExemplarTimestamp)
	exemplar.SetDoubleValue(15)
	initMetricExemplarAttributes(exemplar.FilteredAttributes())
	hdp1.ExplicitBounds().FromRaw([]float64{1})
}

func initExponentialHistogramMetric(hm pmetric.Metric) {
	initMetric(hm, TestExponentialHistogramMetricName, pmetric.MetricTypeExponentialHistogram)

	hdps := hm.ExponentialHistogram().DataPoints()
	hdp0 := hdps.AppendEmpty()
	initMetricAttributes13(hdp0.Attributes())
	hdp0.SetStartTimestamp(metricStartTimestamp)
	hdp0.SetTimestamp(metricTimestamp)
	hdp0.SetCount(5)
	hdp0.SetSum(0.15)
	hdp0.SetZeroCount(1)
	hdp0.SetScale(1)

	// positive index 1 and 2 are values sqrt(2), 2 at scale 1
	hdp0.Positive().SetOffset(1)
	hdp0.Positive().BucketCounts().FromRaw([]uint64{1, 1})
	// negative index -1 and 0 are values -1/sqrt(2), -1 at scale 1
	hdp0.Negative().SetOffset(-1)
	hdp0.Negative().BucketCounts().FromRaw([]uint64{1, 1})

	// The above will print:
	// Bucket (2.000000, 2.828427], Count: 1
	// Bucket (1.414214, 2.000000], Count: 1
	// Bucket [0, 0], Count: 1
	// Bucket [-1.000000, -0.707107), Count: 1
	// Bucket [-1.414214, -1.000000), Count: 1

	hdp1 := hdps.AppendEmpty()
	initMetricAttributes2(hdp1.Attributes())
	hdp1.SetStartTimestamp(metricStartTimestamp)
	hdp1.SetTimestamp(metricTimestamp)
	hdp1.SetCount(3)
	hdp1.SetSum(1.25)
	hdp1.SetMin(0)
	hdp1.SetMax(1)
	hdp1.SetZeroCount(1)
	hdp1.SetScale(-1)

	// index -1 and 0 are values 0.25, 1 at scale -1
	hdp1.Positive().SetOffset(-1)
	hdp1.Positive().BucketCounts().FromRaw([]uint64{1, 1})

	// The above will print:
	// Bucket [0, 0], Count: 1
	// Bucket (0.250000, 1.000000], Count: 1
	// Bucket (1.000000, 4.000000], Count: 1

	exemplar := hdp1.Exemplars().AppendEmpty()
	exemplar.SetTimestamp(metricExemplarTimestamp)
	exemplar.SetDoubleValue(15)
	initMetricExemplarAttributes(exemplar.FilteredAttributes())
}

func initSummaryMetric(sm pmetric.Metric) {
	initMetric(sm, TestSummaryMetricName, pmetric.MetricTypeSummary)

	sdps := sm.Summary().DataPoints()
	sdp0 := sdps.AppendEmpty()
	initMetricAttributes13(sdp0.Attributes())
	sdp0.SetStartTimestamp(metricStartTimestamp)
	sdp0.SetTimestamp(metricTimestamp)
	sdp0.SetCount(1)
	sdp0.SetSum(15)

	sdp1 := sdps.AppendEmpty()
	initMetricAttributes2(sdp1.Attributes())
	sdp1.SetStartTimestamp(metricStartTimestamp)
	sdp1.SetTimestamp(metricTimestamp)
	sdp1.SetCount(1)
	sdp1.SetSum(15)

	quantile := sdp1.QuantileValues().AppendEmpty()
	quantile.SetQuantile(0.01)
	quantile.SetValue(15)
}

func initMetric(m pmetric.Metric, name string, ty pmetric.MetricType) {
	m.SetName(name)
	m.SetDescription("")
	m.SetUnit("1")
	switch ty {
	case pmetric.MetricTypeGauge:
		m.SetEmptyGauge()
	case pmetric.MetricTypeSum:
		sum := m.SetEmptySum()
		sum.SetIsMonotonic(true)
		sum.SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
	case pmetric.MetricTypeHistogram:
		histo := m.SetEmptyHistogram()
		histo.SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
	case pmetric.MetricTypeExponentialHistogram:
		histo := m.SetEmptyExponentialHistogram()
		histo.SetAggregationTemporality(pmetric.AggregationTemporalityDelta)
	case pmetric.MetricTypeSummary:
		m.SetEmptySummary()
	}
}

func initMetricExemplarAttributes(dest pcommon.Map) {
	dest.PutStr("exemplar-attachment", "exemplar-attachment-value")
}

func initMetricAttributes1(dest pcommon.Map) {
	dest.PutStr("label-1", "label-value-1")
}

func initMetricAttributes2(dest pcommon.Map) {
	dest.PutStr("label-2", "label-value-2")
}

func initMetricAttributes12(dest pcommon.Map) {
	initMetricAttributes1(dest)
	initMetricAttributes2(dest)
}

func initMetricAttributes13(dest pcommon.Map) {
	initMetricAttributes1(dest)
	dest.PutStr("label-3", "label-value-3")
}
