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

package s3

import (
	"bytes"
	"encoding/base64"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/google/uuid"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/credentials"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/s3/s3manager"
)

type convertConfig struct {
	// Rename one or more fields from tags.
	TagFieldsRename map[string]string
	// Rename one or more fields, The protocol field options can only be: contents, tags, time
	ProtocolFieldsRename map[string]string
	// Convert protocol, default value: custom_single
	Protocol string
	// Convert encoding, default value:json
	// The options are: 'json'
	Encoding string
}

type FlusherS3 struct {
	Region        string        // s3 bucket的region
	Bucket        string        // bucket name
	Profile       string        // 指定 AWS 配置文件以提供凭据的选项，用于指定权限，默认为`default`。
	S3KeyFormat   string        // S3 键的格式字符串。此选项支持${UUID}、strftime 时间格式器。在格式字符串中添加 ${UUID} 以插入一个随机字符串。默认值： "/loongcollector-logs/%Y-%m-%d/%H-%M-%S-${UUID}.log"
	TotalFileSize int64         // S3 中写入的文件大小，单位为byte。最小大小： 1024（1K） ，最大大小： 1073741824（1G）。默认值： 104857600（100M）。
	UploadTimeout int64         // 超时时间，单位为秒。超过该时间，LoongCollector 将在 S3 中上传并创建一个新的文件。设置为 3600 表示每小时上传一个新文件。最小值：10，默认值：3600
	Convert       convertConfig // LoongCollector 数据转换协议

	context   pipeline.Context
	uploader  *s3manager.Uploader
	mutex     sync.Mutex
	converter *converter.Converter
	buffer    *bytes.Buffer
	startTime time.Time
}

func (f *FlusherS3) Init(context pipeline.Context) error {
	f.context = context
	// Check TotalFileSize
	if f.TotalFileSize < 1024 {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init flusher_s3 fail, error", "TotalFileSize must be at least 1024 bytes")
		f.TotalFileSize = 1024
	}
	if f.TotalFileSize > 1073741824 {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init flusher_s3 fail, error", "TotalFileSize must be no more than 1073741824 bytes")
		f.TotalFileSize = 1073741824
	}
	// Check UploadTimeout
	if f.UploadTimeout < 10 {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init flusher_s3 fail, error", "UploadTimeout must be at least 10 seconds")
		f.UploadTimeout = 10
	}
	// Init session
	sess := session.Must(session.NewSession(&aws.Config{
		Region:      aws.String(f.Region),
		Credentials: credentials.NewSharedCredentials("", f.Profile),
	}))
	f.uploader = s3manager.NewUploader(sess)
	// Init converter
	if f.Convert.Encoding == "" {
		f.Convert.Encoding = converter.EncodingJSON
	}
	if f.Convert.Protocol == "" {
		f.Convert.Protocol = converter.ProtocolCustomSingle
	}
	convert, err := f.getConverter()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init flusher_s3 converter fail, error", err.Error())
		return err
	}
	f.converter = convert
	// Init buffer
	f.buffer = new(bytes.Buffer)
	// Init start time
	f.startTime = time.Now()
	return nil
}

func (f *FlusherS3) Description() string {
	return "s3 flusher for LoongCollector"
}

func (f *FlusherS3) SetUrgent(flag bool) {
}

func (f *FlusherS3) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return f.uploader != nil
}

func (f *FlusherS3) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		serializedLogs, _ := f.converter.ToByteStream(logGroup)
		for _, log := range serializedLogs.([][]byte) {
			f.writeBuffer(log)
			if f.shouldUpload() {
				f.uploadBuffer()
			}
		}
	}
	return nil
}

func (f *FlusherS3) Export(in []*models.PipelineGroupEvents, context pipeline.PipelineContext) error {
	for _, pipelineEventGroup := range in {
		serializedLogs, _, _ := f.converter.ToByteStreamWithSelectedFieldsV2(pipelineEventGroup, nil)
		for _, log := range serializedLogs.([][]byte) {
			f.writeBuffer(log)
			if f.shouldUpload() {
				f.uploadBuffer()
			}
		}
	}
	return nil
}

func (f *FlusherS3) Stop() error {
	f.uploadBuffer()
	return nil
}

func (f *FlusherS3) writeBuffer(data []byte) {
	f.mutex.Lock()
	defer f.mutex.Unlock()

	f.buffer.Write(data)
}

func (f *FlusherS3) uploadBuffer() {
	f.mutex.Lock()
	defer f.mutex.Unlock()

	if f.buffer.Len() == 0 {
		return
	}

	key := f.generateS3Key()
	data := make([]byte, len(f.buffer.Bytes()))
	copy(data, f.buffer.Bytes())
	f.buffer.Reset()
	f.startTime = time.Now()

	// 起协程发送buffer的副本，及时释放锁，提高效率
	go f.upload(key, data)
}

func (f *FlusherS3) upload(key string, data []byte) {
	count := 0
	for count < 5 {
		select {
		case <-f.context.GetRuntimeContext().Done():
			logger.Warningf(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "upload data canceled to S3: %s/%s", f.Bucket, key)
			return
		default:
			_, err := f.uploader.Upload(&s3manager.UploadInput{
				Bucket: aws.String(f.Bucket),
				Key:    aws.String(key),
				Body:   bytes.NewReader(data),
			})

			if err != nil {
				count++
				logger.Errorf(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "failed to upload data to S3: %s/%s, err: %v, try %d/5", f.Bucket, key, err, count)
				time.Sleep(1 * time.Second)
			} else {
				logger.Infof(f.context.GetRuntimeContext(), "Data uploaded to S3: %s/%s success, size: %d", f.Bucket, key, len(data))
				return
			}
		}
	}
	logger.Errorf(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "failed to upload data to S3: %s/%s, discard data, size: %d", f.Bucket, key, len(data))
}

func (f *FlusherS3) shouldUpload() bool {
	return f.buffer.Len() >= int(f.TotalFileSize) || int64(time.Since(f.startTime).Seconds()) > f.UploadTimeout
}

func (f *FlusherS3) generateS3Key() string {
	key := f.S3KeyFormat
	u := uuid.New()
	base64UUID := base64.URLEncoding.EncodeToString(u[:])
	key = strings.ReplaceAll(key, "${UUID}", base64UUID[:8])

	currentTime := time.Now()
	replacements := map[string]string{
		"%Y": fmt.Sprintf("%04d", currentTime.Year()),
		"%m": fmt.Sprintf("%02d", currentTime.Month()),
		"%d": fmt.Sprintf("%02d", currentTime.Day()),
		"%H": fmt.Sprintf("%02d", currentTime.Hour()),
		"%M": fmt.Sprintf("%02d", currentTime.Minute()),
		"%S": fmt.Sprintf("%02d", currentTime.Second()),
		// Add more replacements as needed
	}

	for placeholder, value := range replacements {
		key = strings.ReplaceAll(key, placeholder, value)
	}
	return key
}

func (f *FlusherS3) getConverter() (*converter.Converter, error) {
	logger.Debug(f.context.GetRuntimeContext(), "[ilogtail data convert config] Protocol", f.Convert.Protocol,
		"Encoding", f.Convert.Encoding, "TagFieldsRename", f.Convert.TagFieldsRename, "ProtocolFieldsRename", f.Convert.ProtocolFieldsRename)
	return converter.NewConverter(f.Convert.Protocol, f.Convert.Encoding, f.Convert.TagFieldsRename, f.Convert.ProtocolFieldsRename, f.context.GetPipelineScopeConfig())
}

// Register the plugin to the Flushers array.
func init() {
	pipeline.Flushers["flusher_s3"] = func() pipeline.Flusher {
		return &FlusherS3{
			S3KeyFormat:   "/loongcollector-logs/%Y-%m-%d/%H-%M-%S-${UUID}.log",
			TotalFileSize: 104857600,
			UploadTimeout: 3600,
			Profile:       "default",
		}
	}
}
