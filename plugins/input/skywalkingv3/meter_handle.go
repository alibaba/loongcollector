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

package skywalkingv3

import (
	"io"
	"math"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	agent "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
)

type MeterHandler struct {
	context   pipeline.Context
	collector pipeline.PipelineCollector
}

func (m *MeterHandler) Collect(srv agent.MeterReportService_CollectServer) error {
	defer panicRecover()
	// skywalking only send time/service/serviceInstance in first packet
	ts := int64(0)
	service := ""
	serviceInstance := ""
	for {
		meterData, err := srv.Recv()
		if err != nil {
			if err == io.EOF {
				return srv.SendAndClose(&v3.Commands{})
			}
			logger.Warning(m.context.GetRuntimeContext(), selfmonitor.SkywalkingMeterGRPCError, "error", err)
			return err
		}
		// logger.Info("service", service, "serviceInstance", serviceInstance, "meter", meterData.String())
		if meterData.Timestamp > 0 {
			ts = meterData.Timestamp
		} else {
			ts = time.Now().UnixNano() / 1e6
		}
		if len(meterData.Service) > 0 {
			service = meterData.Service
		}
		if len(meterData.ServiceInstance) > 0 {
			serviceInstance = meterData.ServiceInstance
		}
		// when logtail restart, will receive partial stream, drop them
		if service == "" || serviceInstance == "" {
			continue
		}
		handleMeterData(m.context, m.collector, meterData, service, serviceInstance, ts)

	}
}

func convertHistogramData(histogramData *agent.MeterHistogram) *helper.HistogramData {
	hd := &helper.HistogramData{}
	var totalCount int64
	var sum float64
	for index, v := range histogramData.Values {
		sum += float64(v.Count) * v.Bucket
		if index == 0 {
			totalCount = v.Count
			continue
		}
		hd.Buckets = append(hd.Buckets, helper.DefBucket{
			Le:    v.Bucket,
			Count: totalCount,
		})
		totalCount += v.Count
	}
	hd.Buckets = append(hd.Buckets, helper.DefBucket{
		Le:    math.Inf(0),
		Count: totalCount,
	})
	hd.Count = totalCount
	hd.Sum = sum
	return hd
}

func handleMeterData(context pipeline.Context, collector pipeline.PipelineCollector, meterData *agent.MeterData, service string, serviceInstance string, ts int64) {
	singleValue := meterData.GetSingleValue()
	logger.Debug(context.GetRuntimeContext(), "service", meterData.Service, "serviceInstance", meterData.ServiceInstance)
	if singleValue != nil {
		value := singleValue.Value
		name := singleValue.Name
		var labels helper.MetricLabels
		for _, l := range singleValue.Labels {
			labels.Append(l.Name, l.Value)
		}
		labels.Append("service", service)
		labels.Append("serviceInstance", serviceInstance)

		collector.Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), helper.NewInputMetric(name, ts, value, &labels))
	}
	histogramData := meterData.GetHistogram()

	if histogramData != nil {
		var labels helper.MetricLabels
		labels.Append("service", service)
		labels.Append("serviceInstance", serviceInstance)
		for _, l := range histogramData.Labels {
			labels.Append(l.Name, l.Value)
		}

		histogram := convertHistogramData(histogramData)
		name := histogramData.Name
		collector.Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), helper.NewInputMetric(name+"_count", ts, float64(histogram.Count), &labels))
		collector.Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), helper.NewInputMetric(name+"_sum", ts, histogram.Sum, &labels))
		for _, bucket := range histogram.Buckets {
			labels.Replace("le", strconv.FormatFloat(bucket.Le, 'g', -1, 64))
			collector.Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), helper.NewInputMetric(name+"_bucket", ts, float64(bucket.Count), &labels))
		}
	}
}
