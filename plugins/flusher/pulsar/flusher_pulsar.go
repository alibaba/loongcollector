// Copyright 2023 iLogtail Authors
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

package pulsar

import (
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/apache/pulsar-client-go/pulsar"

	"github.com/alibaba/ilogtail/pkg/fmtstr"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/pkg/util"
)

type FlusherPulsar struct {
	// URL for the Pulsar service
	URL string
	// SendTimeout send timeout
	SendTimeout time.Duration
	// OperationTimeout sets producer-create, subscribe and unsubscribe operations timeout (default: 30 seconds)
	OperationTimeout time.Duration
	// ConnectionTimeout timeout for the establishment of a TCP connection (default: 5 seconds)
	ConnectionTimeout time.Duration
	// MaxConnectionsPerBroker max number of connections to a single broker that will kept in the pool. (default: 1 connection)
	MaxConnectionsPerBroker int
	// Topic The name of the pulsar topic
	Topic string
	// Name  The producer name
	Name string
	// Convert  ilogtail data convert config
	Convert convertConfig
	// Configure whether the Pulsar client accept untrusted TLS certificate from broker (default: false)
	// EnableTLS
	EnableTLS bool
	// Set the path to the trusted TLS certificate file
	TLSTrustCertsFilePath string
	// Authentication support tls
	Authentication Authentication

	// MaxReconnectToBroker specifies the maximum retry number of reconnectToBroker. (default: ultimate)
	MaxReconnectToBroker *uint
	// DisableBlockIfQueueFull controls whether Send and SendAsync block if producer's message queue is full
	DisableBlockIfQueueFull bool
	// CompressionType  Codec used to produce messages,NONE,LZ4,ZLIB,ZSTD
	CompressionType string
	// HashingScheme is used to define the partition on where to publish a particular message
	HashingScheme string
	// BatchingMaxPublishDelay the batch push delay
	BatchingMaxPublishDelay time.Duration
	// BatchingMaxMessages maximum number of messages in a batch
	BatchingMaxMessages uint
	// MaxCacheProducers Specify the max cache(lru) producers of dynamic topic
	MaxCacheProducers int

	PartitionKeys []string
	ClientID      string

	context   pipeline.Context
	converter *converter.Converter
	// obtain from Topic
	topicKeys       []string
	pulsarClient    pulsar.Client
	producers       *Producers
	producerOptions pulsar.ProducerOptions
	hashKeyMap      map[string]interface{}
	selectFields    []string
}
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

func (f *FlusherPulsar) Init(context pipeline.Context) error {
	f.context = context
	// Validate config of flusher
	if err := f.Validate(); err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init pulsar flusher fail, error", err)
		return err
	}
	// Set default value while not set
	if f.Convert.Encoding == "" {
		f.converter.Encoding = converter.EncodingJSON
	}
	if f.Convert.Protocol == "" {
		f.Convert.Protocol = converter.ProtocolCustomSingle
	}
	// Init converter
	convert, err := f.getConverter()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init pulsar flusher converter fail, error", err)
		return err
	}
	f.converter = convert

	// Obtain topic keys from dynamic topic expression
	topicKeys, err := fmtstr.CompileKeys(f.Topic)
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init pulsar flusher fail, error", err)
		return err
	}
	f.topicKeys = topicKeys
	// Init pulsar client
	options := f.initClientOptions()
	client, err := pulsar.NewClient(options)
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init pulsar flusher fail, error", err)
		return err
	}
	f.pulsarClient = client

	// Init pulsar producers
	f.producers = NewProducers(f.context.GetRuntimeContext(), f.MaxCacheProducers)

	// Init Producer options
	producerOptions, err := f.initProducerOptions()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init pulsar flusher producer options fail, error", err)
		return err
	}
	f.producerOptions = producerOptions
	// Init partition keys
	if f.PartitionKeys != nil {
		f.hashKeyMap = make(map[string]interface{})
		for _, key := range f.PartitionKeys {
			f.hashKeyMap[key] = struct{}{}
		}
	}
	// Merge topicKeys and HashKeys,Only one convert after merge
	f.selectFields = util.UniqueStrings(f.topicKeys, f.PartitionKeys)
	return nil
}

func (f *FlusherPulsar) Validate() error {
	if f.URL == "" {
		var err = errors.New("url is nil")
		return err
	}
	if f.Topic == "" {
		var err = errors.New("topic is nil")
		return err
	}
	if f.EnableTLS {
		if len(f.TLSTrustCertsFilePath) == 0 {
			return errors.New("no tls_trust_certs_file_path configured")
		}
	}
	if f.Authentication.OAuth2 != nil {
		if f.Authentication.OAuth2.Enabled {
			if len(f.Authentication.OAuth2.IssuerURL) == 0 {
				return errors.New("OAuth2 issuer_url is nil")
			}
			if len(f.Authentication.OAuth2.PrivateKey) == 0 {
				return errors.New("OAuth2 private_key is nil")
			}
		}
	}
	return nil
}

func (f *FlusherPulsar) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	topic := f.Topic
	for _, logGroup := range logGroupList {
		logger.Debug(f.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)
		logs, values, err := f.converter.ToByteStreamWithSelectedFields(logGroup, f.selectFields)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush pulsar convert log fail, error", err)
		}
		for index, log := range logs.([][]byte) {
			valueMap := values[index]
			if len(f.topicKeys) > 0 {
				formattedTopic, err := fmtstr.FormatTopic(valueMap, f.Topic)
				if err != nil {
					logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush pulsar format topic fail, error", err)
				} else {
					topic = *formattedTopic
				}
			}
			producer, err := f.producers.GetProducer(topic, f.pulsarClient, f.producerOptions)
			if err != nil {
				logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "load pulsar producer fail,topic", topic, "err", err)
				return err
			}

			message := &pulsar.ProducerMessage{
				Payload: log,
			}
			if f.PartitionKeys != nil {
				message.Key = f.hashPartitionKey(valueMap, logstoreName)
			}
			producer.SendAsync(f.context.GetRuntimeContext(), message, func(msgId pulsar.MessageID, prodMsg *pulsar.ProducerMessage, err error) {
				if err != nil {
					logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "send message to pulsar fail,error", err)
				} else {
					logger.Debug(f.context.GetRuntimeContext(), "Pulsar success send events: messageID: %s ", msgId)
				}
			})
		}
	}
	return nil
}

func (f *FlusherPulsar) Description() string {
	return "Pulsar flusher for logtail"
}

func (*FlusherPulsar) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (f *FlusherPulsar) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return f.pulsarClient != nil
}
func (f *FlusherPulsar) Stop() error {
	err := f.producers.Close()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_STOP_ALARM", "stop pulsar flusher fail, error", err)
	}
	f.pulsarClient.Close()
	return err
}

func (f *FlusherPulsar) getConverter() (*converter.Converter, error) {
	logger.Debug(f.context.GetRuntimeContext(), "[ilogtail data convert config] Protocol", f.Convert.Protocol,
		"Encoding", f.Convert.Encoding, "TagFieldsRename", f.Convert.TagFieldsRename, "ProtocolFieldsRename", f.Convert.ProtocolFieldsRename)
	return converter.NewConverter(f.Convert.Protocol, f.Convert.Encoding, f.Convert.TagFieldsRename, f.Convert.ProtocolFieldsRename, f.context.GetPipelineScopeConfig())
}

func (f *FlusherPulsar) initClientOptions() pulsar.ClientOptions {
	options := pulsar.ClientOptions{
		URL:                     f.URL,
		OperationTimeout:        f.OperationTimeout,
		ConnectionTimeout:       f.ConnectionTimeout,
		MaxConnectionsPerBroker: f.MaxConnectionsPerBroker,
	}
	options.TLSAllowInsecureConnection = f.EnableTLS
	if len(f.TLSTrustCertsFilePath) > 0 {
		options.TLSTrustCertsFilePath = f.TLSTrustCertsFilePath
	}
	// configure Authentication
	options.Authentication = f.Authentication.Auth()
	return options
}

func (f *FlusherPulsar) initProducerOptions() (pulsar.ProducerOptions, error) {
	producerOptions := pulsar.ProducerOptions{
		Topic:                f.Topic,
		MaxReconnectToBroker: f.MaxReconnectToBroker,
	}
	if len(f.Name) > 0 {
		producerOptions.Name = f.Name
	}

	hashScheme, err := f.convertHashScheme(f.HashingScheme)
	if err != nil {
		return pulsar.ProducerOptions{}, err
	}
	producerOptions.HashingScheme = hashScheme

	compressType, err := f.convertCompressionType(f.CompressionType)
	if err != nil {
		return pulsar.ProducerOptions{}, err
	}
	producerOptions.CompressionType = compressType

	if f.SendTimeout > 0 {
		producerOptions.SendTimeout = f.SendTimeout
	}

	if f.BatchingMaxPublishDelay > 0 {
		producerOptions.BatchingMaxPublishDelay = f.BatchingMaxPublishDelay
	}
	if f.BatchingMaxMessages > 0 {
		producerOptions.BatchingMaxMessages = f.BatchingMaxMessages
	}
	if f.DisableBlockIfQueueFull {
		producerOptions.DisableBlockIfQueueFull = f.DisableBlockIfQueueFull
	}
	return producerOptions, nil
}

func (f *FlusherPulsar) hashPartitionKey(valueMap map[string]string, defaultKey string) string {
	var hashData []string
	var notMatchKeys []string
	for key := range f.hashKeyMap {
		if value, ok := valueMap[key]; ok {
			hashData = append(hashData, value)
		} else {
			notMatchKeys = append(notMatchKeys, key)
		}
	}
	if len(notMatchKeys) > 0 {
		logger.Warning(f.context.GetRuntimeContext(), "Some fields in PartitionKeys cannot be matched in the log content, keys", notMatchKeys)
	}
	if len(hashData) == 0 {
		hashData = append(hashData, defaultKey)
	}
	logger.Debug(f.context.GetRuntimeContext(), "partition key", hashData, " hashKeyMap", f.hashKeyMap)
	return strings.Join(hashData, "###")
}

func (f *FlusherPulsar) convertCompressionType(compressionType string) (pulsar.CompressionType, error) {
	switch compressionType {
	case "NONE":
		return pulsar.NoCompression, nil
	case "LZ4":
		return pulsar.LZ4, nil
	case "ZLIB":
		return pulsar.ZLib, nil
	case "ZSTD":
		return pulsar.ZSTD, nil
	default:
		return pulsar.NoCompression, fmt.Errorf("CompressionType should be one of 'NONE', 'LZ4', 'ZLIB', or 'ZSTD'. configured value %v", compressionType)
	}
}

func (f *FlusherPulsar) convertHashScheme(hashScheme string) (pulsar.HashingScheme, error) {
	switch hashScheme {
	case "JavaStringHash":
		return pulsar.JavaStringHash, nil
	case "Murmur3_32Hash ":
		return pulsar.Murmur3_32Hash, nil
	default:
		return pulsar.JavaStringHash, fmt.Errorf("HashingScheme  should be one of 'JavaStringHash', 'Murmur3_32Hash'. configured value %v", hashScheme)
	}
}

func init() {
	pipeline.Flushers["flusher_pulsar"] = func() pipeline.Flusher {
		return &FlusherPulsar{
			URL:               "",
			ClientID:          "iLogtail",
			MaxCacheProducers: 8,
			Convert: convertConfig{
				Protocol: converter.ProtocolCustomSingle,
				Encoding: converter.EncodingJSON,
			},
			CompressionType:         "NONE",
			HashingScheme:           "JavaStringHash",
			MaxConnectionsPerBroker: 1,
			ConnectionTimeout:       5 * time.Second,
			OperationTimeout:        30 * time.Second,
			MaxReconnectToBroker:    nil,
		}
	}
}
