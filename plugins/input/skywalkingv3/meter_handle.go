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
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	agent "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
)

type MeterHandler struct {
	context   pipeline.Context
	collector pipeline.Collector
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
			logger.Error(m.context.GetRuntimeContext(), "SKYWALKING_METER_GRPC_ERROR", "error", err)
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

func handleMeterData(context pipeline.Context, collector pipeline.Collector, meterData *agent.MeterData, service string, serviceInstance string, ts int64) {
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

		metricLog := helper.NewMetricLog(name, ts, value, &labels)
		// logger.Info("meter", meterData)
		collector.AddRawLog(metricLog)
	}
	histogramData := meterData.GetHistogram()

	if histogramData != nil {
		var labels helper.MetricLabels
		labels.Append("service", service)
		labels.Append("serviceInstance", serviceInstance)
		for _, l := range histogramData.Labels {
			labels.Append(l.Name, l.Value)
		}

		hd := convertHistogramData(histogramData)
		logs := hd.ToMetricLogs(histogramData.Name, ts, &labels)
		for _, logIns := range logs {
			collector.AddRawLog(logIns)
		}
	}
}
