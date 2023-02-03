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
	"context"
	"crypto/rand"
	"fmt"
	"math"
	"math/big"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	common "github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/common"
	v2 "github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent/v2"
)

type JVMMetricReportHandle struct {
	RegistryInformationCache

	context   pipeline.Context
	collector pipeline.Collector
}

func (j *JVMMetricReportHandle) Collect(ctx context.Context, metrics *v2.JVMMetricCollection) (*common.Commands, error) {
	defer panicRecover()
	applicationInstance, ok := j.RegistryInformationCache.findApplicationInstanceRegistryInfo(metrics.GetServiceInstanceId())

	if !ok {
		args := make([]*common.KeyStringValuePair, 0)
		serializeNumber, _ := rand.Int(rand.Reader, big.NewInt(math.MaxInt64))
		args = append(args, &common.KeyStringValuePair{Key: "SerialNumber", Value: fmt.Sprintf("reset-%d", serializeNumber)})

		var commands []*common.Command
		commands = append(commands, &common.Command{Command: "ServiceMetadataReset", Args: args})
		return &common.Commands{Commands: commands}, nil
	}

	for _, metric := range metrics.GetMetrics() {
		logs := toMetricStoreFormat(metric, applicationInstance.application.applicationName, applicationInstance.uuid, applicationInstance.host)
		for _, log := range logs {
			j.collector.AddRawLog(log)
		}
	}

	return &common.Commands{}, nil
}
