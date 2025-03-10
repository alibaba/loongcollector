package odps

import (
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/aliyun/aliyun-odps-go-sdk/odps"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/account"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/tunnel"
)

const OdpsAlarm string = "ODPS_FLUSHER_ALARM"

type OdpsFlusher struct {
	AccessKeyId     string
	AccessKeySecret string
	SecurityToken   string
	BearerToken     string
	Endpoint        string
	ProjectName     string
	SchemaName      string
	TableName       string
	ExtraLevel      int
	TimeRange       int
	PartitionConfig string
	context         pipeline.Context
	sender          TunnelSender
}

func NewOdpsFlusher() *OdpsFlusher {
	return &OdpsFlusher{
		AccessKeyId:     "",
		AccessKeySecret: "",
		SecurityToken:   "",
		BearerToken:     "",
		Endpoint:        "",
		ProjectName:     "",
		TableName:       "",
		ExtraLevel:      1,
		TimeRange:       15,
		PartitionConfig: "",
	}
}

func (o *OdpsFlusher) Init(context pipeline.Context) error {
	o.context = context

	tunnelIns, err := o.CreateTunnel()
	if err != nil {
		logger.Errorf(o.context.GetRuntimeContext(), OdpsAlarm, "Create (%s/%s/%s) tunnel failed, error:%v",
			o.ProjectName, o.SchemaName, o.TableName, err)
		return err
	}

	o.sender = NewTunnelSender(context, o.ProjectName, o.SchemaName, o.TableName, o.PartitionConfig, o.TimeRange, o.ExtraLevel, tunnelIns)
	if err := o.sender.Init(); err != nil {
		logger.Errorf(o.context.GetRuntimeContext(), OdpsAlarm, "Init (%s/%s/%s) tunnel sender failed, error:%v",
			o.ProjectName, o.SchemaName, o.TableName, err)
		return err
	}

	logger.Infof(o.context.GetRuntimeContext(), "Init odps (%s/%s/%s) flusher success, partition:%s/%d",
		o.ProjectName, o.SchemaName, o.TableName, o.PartitionConfig, o.TimeRange)
	return nil
}

func (o *OdpsFlusher) CreateTunnel() (*tunnel.Tunnel, error) {
	var aliAccount account.Account
	if len(o.BearerToken) > 0 {
		logger.Infof(o.context.GetRuntimeContext(), "Use bearer token to create odps flusher: %s", o.BearerToken)
		aliAccount = account.NewBearerTokenAccount(o.BearerToken)
	} else {
		logger.Infof(o.context.GetRuntimeContext(), "Use AK to create odps flusher: %s", o.AccessKeyId)
		if len(o.SecurityToken) > 0 {
			aliAccount = account.NewStsAccount(o.AccessKeyId, o.AccessKeySecret, o.SecurityToken)
		} else {
			aliAccount = account.NewAliyunAccount(o.AccessKeyId, o.AccessKeySecret)
		}
	}

	odpsIns := odps.NewOdps(aliAccount, o.Endpoint)
	odpsIns.SetDefaultProjectName(o.ProjectName)
	project := odpsIns.DefaultProject()
	tunnelEndpoint, err := project.GetTunnelEndpoint()
	if err != nil {
		return nil, err
	}
	logger.Infof(o.context.GetRuntimeContext(), "Get (%s/%s) tunnel endpoint success, endpoint:%s", o.ProjectName, o.TableName, tunnelEndpoint)
	return tunnel.NewTunnel(odpsIns, tunnelEndpoint), nil
}

func (o *OdpsFlusher) Description() string {
	return "odps flusher"
}

func (o *OdpsFlusher) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		for _, log := range logGroup.Logs {
			if err := o.sender.Send(logGroup, log); err != nil {
				logger.Errorf(o.context.GetRuntimeContext(), OdpsAlarm, "Flush odps(%s/%s/%s) failed, error:%v", o.ProjectName, o.SchemaName, o.TableName, err)
				return err
			}
		}
	}
	return o.sender.Flush()
}

func (o *OdpsFlusher) IsReady(ProjectName string, logstoreName string, logstoreKey int64) bool {
	return o.sender != nil
}

func (o *OdpsFlusher) SetUrgent(flag bool) {
	// do nothing
}

func (o *OdpsFlusher) Stop() error {
	return o.sender.Flush()
}

func init() {
	pipeline.Flushers["flusher_odps"] = func() pipeline.Flusher {
		f := NewOdpsFlusher()
		return f
	}
}
