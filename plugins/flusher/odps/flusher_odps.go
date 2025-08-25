// Copyright 2025 iLogtail Authors
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

package odps

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/aliyun/aliyun-odps-go-sdk/odps"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/account"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/tunnel"
)

const extraLevelDefault = 1

type FlusherOdps struct {
	AccessKeyID     string
	AccessKeySecret string
	SecurityToken   string
	Endpoint        string
	ProjectName     string
	SchemaName      string
	TableName       string
	PartitionConfig string
	TimeRange       int
	sender          TunnelSender
	context         pipeline.Context
}

func NewFlusherOdps() *FlusherOdps {
	return &FlusherOdps{
		AccessKeyID:     "",
		AccessKeySecret: "",
		SecurityToken:   "",
		Endpoint:        "",
		ProjectName:     "",
		TableName:       "",
		PartitionConfig: "",
		TimeRange:       15,
	}
}

func (o *FlusherOdps) Init(context pipeline.Context) error {
	o.context = context

	tunnelIns, err := o.CreateTunnel()
	if err != nil {
		logger.Errorf(o.context.GetRuntimeContext(), "ODPS_FLUSHER_ALAR", "Create (%s/%s/%s) tunnel failed, error:%v",
			o.ProjectName, o.SchemaName, o.TableName, err)
		return err
	}

	o.sender = NewTunnelSender(context, o.ProjectName, o.SchemaName, o.TableName, o.PartitionConfig, o.TimeRange, extraLevelDefault, tunnelIns)
	if err := o.sender.Init(); err != nil {
		logger.Errorf(o.context.GetRuntimeContext(), "ODPS_FLUSHER_ALAR", "Init (%s/%s/%s) tunnel sender failed, error:%v",
			o.ProjectName, o.SchemaName, o.TableName, err)
		return err
	}

	logger.Infof(o.context.GetRuntimeContext(), "Init odps (%s/%s/%s) flusher success, partition:%s/%d",
		o.ProjectName, o.SchemaName, o.TableName, o.PartitionConfig, o.TimeRange)
	return nil
}

func (o *FlusherOdps) CreateTunnel() (*tunnel.Tunnel, error) {
	var aliAccount account.Account
	if len(o.SecurityToken) > 0 {
		aliAccount = account.NewStsAccount(o.AccessKeyID, o.AccessKeySecret, o.SecurityToken)
	} else {
		aliAccount = account.NewAliyunAccount(o.AccessKeyID, o.AccessKeySecret)
	}

	odpsIns := odps.NewOdps(aliAccount, o.Endpoint)
	odpsIns.SetUserAgent(fmt.Sprintf("loongcollector/%s-%s", config.BaseVersion, config.LoongcollectorGlobalConfig.HostIP))
	odpsIns.SetDefaultProjectName(o.ProjectName)
	project := odpsIns.DefaultProject()
	tunnelEndpoint, err := project.GetTunnelEndpoint()
	if err != nil {
		return nil, err
	}
	logger.Infof(o.context.GetRuntimeContext(), "Get (%s/%s) tunnel endpoint success, endpoint:%s", o.ProjectName, o.TableName, tunnelEndpoint)
	return tunnel.NewTunnel(odpsIns, tunnelEndpoint), nil
}

func (o *FlusherOdps) Description() string {
	return "odps flusher"
}

func (o *FlusherOdps) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		for _, log := range logGroup.Logs {
			if err := o.sender.Send(logGroup, log); err != nil {
				logger.Errorf(o.context.GetRuntimeContext(), "ODPS_FLUSHER_ALAR", "Flush odps(%s/%s/%s) failed, error:%v", o.ProjectName, o.SchemaName, o.TableName, err)
				return err
			}
		}
	}
	return o.sender.Flush()
}

func (o *FlusherOdps) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return o.sender != nil
}

func (o *FlusherOdps) SetUrgent(flag bool) {
	// do nothing
}

func (o *FlusherOdps) Stop() error {
	return o.sender.Flush()
}

func init() {
	pipeline.Flushers["flusher_aliyun_odps"] = func() pipeline.Flusher {
		f := NewFlusherOdps()
		return f
	}
}
