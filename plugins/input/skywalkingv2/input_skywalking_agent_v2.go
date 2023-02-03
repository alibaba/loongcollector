// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http:www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package skywalkingv2

import (
	"google.golang.org/grpc"

	"net"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
	v2 "github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent/v2"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv3"
)

type Input struct {
	grpcServer       *grpc.Server
	ctx              pipeline.Context
	Address          string
	ComponentMapping map[int32]string
}

func (r *Input) Init(ctx pipeline.Context) (int, error) {
	r.ctx = ctx
	r.grpcServer = grpc.NewServer()
	return 0, nil
}

func (r *Input) Description() string {
	return "skywalking agent v2 input for logtail"
}

func (r *Input) Collect(pipeline.Collector) error {
	return nil
}

func (r *Input) Start(collector pipeline.Collector) error {
	registryInformationCache := NewRegistryInformationCache()
	agent.RegisterApplicationRegisterServiceServer(r.grpcServer, &ApplicationRegisterHandle{RegistryInformationCache: registryInformationCache})
	agent.RegisterInstanceDiscoveryServiceServer(r.grpcServer, &InstanceDiscoveryServiceHandle{RegistryInformationCache: registryInformationCache})
	agent.RegisterJVMMetricsServiceServer(r.grpcServer, &JVMMetricServiceHandle{
		RegistryInformationCache: registryInformationCache,
		context:                  r.ctx,
		collector:                collector,
	})
	agent.RegisterNetworkAddressRegisterServiceServer(r.grpcServer, &NetworkAddressRegisterHandle{RegistryInformationCache: registryInformationCache})
	agent.RegisterServiceNameDiscoveryServiceServer(r.grpcServer, &ServiceNameDiscoveryHandle{RegistryInformationCache: registryInformationCache})
	agent.RegisterTraceSegmentServiceServer(r.grpcServer, &TraceSegmentHandle{RegistryInformationCache: registryInformationCache,
		context:                      r.ctx,
		collector:                    collector,
		compIDMessagingSystemMapping: skywalkingv3.InitComponentMapping(r.ComponentMapping),
	})
	agent.RegisterRegisterServer(r.grpcServer, &RegisterHandle{RegistryInformationCache: registryInformationCache})
	agent.RegisterServiceInstancePingServer(r.grpcServer, &ServiceInstancePingHandle{RegistryInformationCache: registryInformationCache})
	v2.RegisterTraceSegmentReportServiceServer(r.grpcServer, &TraceSegmentReportHandle{RegistryInformationCache: registryInformationCache,
		context:                      r.ctx,
		collector:                    collector,
		compIDMessagingSystemMapping: skywalkingv3.InitComponentMapping(r.ComponentMapping),
	})

	v2.RegisterJVMMetricReportServiceServer(r.grpcServer, &JVMMetricReportHandle{
		RegistryInformationCache: registryInformationCache,
		context:                  r.ctx,
		collector:                collector,
	})

	if r.Address == "" {
		r.Address = "0.0.0.0:21800"
	}

	lis, err := net.Listen("tcp", r.Address)
	if err != nil {
		return err
	}

	return r.grpcServer.Serve(lis)
}

func (r *Input) Stop() error {
	r.grpcServer.Stop()
	return nil
}

func init() {
	pipeline.ServiceInputs["service_skywalking_agent_v2"] = func() pipeline.ServiceInput {
		return &Input{}
	}
}
