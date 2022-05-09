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
	"net"

	"google.golang.org/grpc"

	"github.com/alibaba/ilogtail"

	configuration "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/agent/configuration/v3"
	agent "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	profile "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/profile/v3"
	logging "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/logging/v3"
	management "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/management/v3"
)

type Input struct {
	grpcServer       *grpc.Server
	ctx              ilogtail.Context
	MetricIntervalMs int64
	Address          string
	ComponentMapping map[int32]string
}

// Init called for init some system resources, like socket, mutex...
// return interval(ms) and error flag, if interval is 0, use default interval
func (r *Input) Init(ctx ilogtail.Context) (int, error) {
	r.ctx = ctx
	r.grpcServer = grpc.NewServer()
	return 0, nil
}

// Description returns a one-sentence description on the Input
func (r *Input) Description() string {
	return "skywalking agent v3 input for logtail"
}

// Collect takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (r *Input) Collect(ilogtail.Collector) error {
	return nil
}

// Start starts the ServiceInput's service, whatever that may be
func (r *Input) Start(collector ilogtail.Collector) error {
	agent.RegisterJVMMetricReportServiceServer(r.grpcServer, &JVMMetricHandler{r.ctx, collector, r.MetricIntervalMs, -1})
	agent.RegisterCLRMetricReportServiceServer(r.grpcServer, &CLRMetricHandler{r.ctx, collector, r.MetricIntervalMs, -1})
	agent.RegisterMeterReportServiceServer(r.grpcServer, &MeterHandler{r.ctx, collector})
	resourcePropertiesCache := &ResourcePropertiesCache{
		cache:    make(map[string]map[string]string),
		cacheKey: r.ctx.GetConfigName() + "#" + CheckpointKey,
	}
	resourcePropertiesCache.load(r.ctx)
	agent.RegisterTraceSegmentReportServiceServer(r.grpcServer, &TracingHandler{r.ctx, collector, resourcePropertiesCache, InitComponentMapping(r.ComponentMapping)})
	management.RegisterManagementServiceServer(r.grpcServer, &ManagementHandler{r.ctx, collector, resourcePropertiesCache})
	profile.RegisterProfileTaskServer(r.grpcServer, &ProfileHandler{})
	configuration.RegisterConfigurationDiscoveryServiceServer(r.grpcServer, &ConfigurationDiscoveryHandler{})
	logging.RegisterLogReportServiceServer(r.grpcServer, &loggingHandler{r.ctx, collector})
	if r.Address == "" {
		r.Address = "0.0.0.0:11800" // skywalking collector default port
	}
	lis, err := net.Listen("tcp", r.Address)
	if err != nil {
		return err
	}
	return r.grpcServer.Serve(lis)
}

func InitComponentMapping(additionMapping map[int32]string) map[int32]string {
	if additionMapping == nil {
		return componentMapping
	}
	mapping := make(map[int32]string)
	for key, value := range additionMapping {
		mapping[key] = value
	}
	for key, value := range componentMapping {
		mapping[key] = value
	}
	return mapping
}

var componentMapping = map[int32]string{
	38: "RocketMQ",
	39: "RocketMQ",
	40: "Kafka",
	41: "Kafka",
	45: "ActiveMQ",
	46: "ActiveMQ",
	52: "RabbitMQ",
	53: "RabbitMQ",
	73: "Pulsar",
	74: "Pulsar",
}

// Stop stops the services and closes any necessary channels and connections
func (r *Input) Stop() error {
	r.grpcServer.Stop()
	return nil
}
func init() {
	ilogtail.ServiceInputs["service_skywalking_agent_v3"] = func() ilogtail.ServiceInput {
		return &Input{MetricIntervalMs: 10000}
	}
}
