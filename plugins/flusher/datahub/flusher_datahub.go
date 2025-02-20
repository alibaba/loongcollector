package datahub

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
)

const (
	VERSION = "0.0.1"
)

type DatahubFlusher struct {
	AccessKeyId     string
	AccessKeySecret string
	SecurityToken   string
	DwarfToken      string
	DwarfSignature  string
	Endpoint        string
	ProjectName     string
	TopicName       string
	CompressType    datahub.CompressorType
	ExtraLevel      int
	encryptor       *Encryptor
	recordBuilder   RecordBuilder
	producer        Producer
	context         pipeline.Context
}

func NewDatahubFlusher() *DatahubFlusher {
	return &DatahubFlusher{
		AccessKeyId:     "",
		AccessKeySecret: "",
		SecurityToken:   "",
		DwarfToken:      "",
		DwarfSignature:  "",
		Endpoint:        "",
		ProjectName:     "",
		TopicName:       "",
		CompressType:    datahub.ZSTD,
		ExtraLevel:      1,
		encryptor:       NewDefaultEncryptor(),
	}
}

func (d *DatahubFlusher) Init(context pipeline.Context) error {
	d.context = context
	logger.Infof(d.context.GetRuntimeContext(), "Init datahub flusher:%v", *d)

	decodeKey, err := d.encryptor.DecodeKey(d.AccessKeySecret)
	if err != nil {
		logger.Errorf(d.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "Decode datahub(%s/%s) access key secret failed, error:%v", d.ProjectName, d.TopicName, err)
		return err
	}

	var account datahub.Account
	if len(d.DwarfToken) > 0 && len(d.DwarfSignature) > 0 {
		account = datahub.NewDwarfCredential(d.AccessKeyId, decodeKey, d.SecurityToken, d.DwarfToken, d.DwarfSignature)
	} else if len(d.SecurityToken) > 0 {
		account = datahub.NewStsCredential(d.AccessKeyId, decodeKey, d.SecurityToken)
	} else {
		account = datahub.NewAliyunAccount(d.AccessKeyId, decodeKey)
	}

	config := &datahub.Config{
		UserAgent:            fmt.Sprintf("dwarf-ilogtail/%s-%s", VERSION, getLocalIp()),
		CompressorType:       d.CompressType,
		EnableBinary:         true,
		EnableSchemaRegistry: false,
		HttpClient:           datahub.DefaultHttpClient(),
	}

	client := datahub.NewClientWithConfig(d.Endpoint, config, account)

	d.recordBuilder = NewRecordBuilder(d.ProjectName, d.TopicName, d.ExtraLevel, client, d.context)
	err = d.recordBuilder.Init()
	if err != nil {
		logger.Errorf(d.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "Init datahub(%s/%s) record builder failed, error:%v", d.ProjectName, d.TopicName, err)
		return err
	} else {
		logger.Infof(d.context.GetRuntimeContext(), "Init datahub(%s/%s) record builder success", d.ProjectName, d.TopicName)
	}

	d.producer = NewProducer(d.ProjectName, d.TopicName, client, d.context)
	err = d.producer.Init()
	if err != nil {
		logger.Errorf(d.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "Init datahub(%s/%s) producer failed, error:%v", d.ProjectName, d.TopicName, err)
		return err
	} else {
		logger.Infof(d.context.GetRuntimeContext(), "Init datahub(%s/%s) producer success", d.ProjectName, d.TopicName)
	}
	return nil
}

func (d *DatahubFlusher) Description() string {
	return "datahub flusher"
}

func (d *DatahubFlusher) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		records := make([]datahub.IRecord, len(logGroup.Logs))
		for id, log := range logGroup.Logs {
			record, err := d.recordBuilder.Log2Record(logGroup, log)
			if err != nil {
				return err
			}
			records[id] = record
		}
		err := d.producer.Send(records)
		if err != nil {
			return err
		}
	}
	return nil
}

func (d *DatahubFlusher) IsReady(ProjectName string, logstoreName string, logstoreKey int64) bool {
	return d.producer != nil && d.recordBuilder != nil
}

func (d *DatahubFlusher) SetUrgent(flag bool) {
}

func (d *DatahubFlusher) Stop() error {
	return nil
}

func init() {
	pipeline.Flushers["flusher_datahub"] = func() pipeline.Flusher {
		f := NewDatahubFlusher()
		return f
	}
}
