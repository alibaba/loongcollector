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

package kafkav2

import (
	cryptorand "crypto/rand"
	"errors"
	"fmt"
	"math"
	"math/big"
	"strings"
	"time"

	"github.com/IBM/sarama"

	"github.com/alibaba/ilogtail/pkg/fmtstr"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	PartitionerTypeRandom     = "random"
	PartitionerTypeRoundRobin = "roundrobin"
	PartitionerTypeRoundHash  = "hash"
)

type FlusherKafka struct {
	context   pipeline.Context
	converter *converter.Converter
	// The list of kafka brokers
	Brokers []string
	// The name of the kafka topic
	Topic string
	// Kafka protocol version
	Version Version
	// The number of seconds to wait for responses from the Kafka brokers before timing out.
	// The default is 30 (seconds).
	Timeout time.Duration

	// Authentication using SASL/PLAIN
	Authentication Authentication
	// Kafka output broker event partitioning strategy.
	// Must be one of random, roundrobin, or hash. By default, the random partitioner is used
	PartitionerType string
	// Kafka metadata update settings.
	Metadata metaConfig
	// The keep-alive period for an active network connection.
	// If 0s, keep-alives are disabled. The default is 0 seconds.
	KeepAlive time.Duration
	// The maximum number of messages the producer will send in a single
	MaxMessageBytes *int
	// RequiredAcks Number of acknowledgements required to assume that a message has been sent.
	//   0 -> NoResponse.  doesn't send any response
	//   1 -> WaitForLocal. waits for only the local commit to succeed before responding ( default )
	//   -1 -> WaitForAll. waits for all in-sync replicas to commit before responding.
	RequiredACKs *int
	// The maximum duration a broker will wait for number of required ACKs.
	// The default is 10s.
	BrokerTimeout time.Duration
	// Compression Codec used to produce messages
	// The options are: 'none', 'gzip', 'snappy', 'lz4', and 'zstd'
	Compression string
	// Sets the compression level used by gzip. Setting this value to 0 disables compression.
	// The compression level must be in the range of 1 (best speed) to 9 (best compression)
	// The default value is 4.
	CompressionLevel int
	// How many outstanding requests a connection is allowed to have before
	// sending on it blocks (default 5).
	MaxOpenRequests int

	// The maximum number of events to bulk in a single Kafka request. The default is 2048.
	BulkMaxSize int

	// Duration to wait before sending bulk Kafka request. 0 is no delay. The default is 0.
	BulkFlushFrequency time.Duration

	MaxRetries int
	// A header is a key-value pair, and multiple headers can be included with the same key.
	// Only string values are supported
	Headers []header

	Backoff backoffConfig
	// Per Kafka broker number of messages buffered in output pipeline. The default is 256
	ChanBufferSize int
	// ilogtail data convert config
	Convert convertConfig

	HashKeys []string
	HashOnce bool
	ClientID string

	// obtain from Topic
	topicKeys     []string
	isTerminal    chan bool
	producer      sarama.AsyncProducer
	hashKeyMap    map[string]struct{}
	hashKey       sarama.StringEncoder
	flusher       FlusherFunc
	selectFields  []string
	recordHeaders []sarama.RecordHeader
}

type backoffConfig struct {
	// The number of seconds to wait before trying to republish to Kafka after a network error.
	// After a successful publishing, the backoff timer is reset. The default is 1s.
	Init time.Duration
	// The maximum number of seconds to wait before attempting to republish to Kafka after a network error.
	// The default is 60s.
	Max time.Duration
}

type header struct {
	Key   string
	Value string
}

type metaConfig struct {
	Retry metaRetryConfig
	// Metadata refresh interval. Defaults to 10 minutes.
	RefreshFrequency time.Duration
	// Strategy to use when fetching metadata,
	// when this option is true, the client will maintain a full set of metadata for all the available topics
	// if this option is set to false it will only refresh the metadata for the configured topics. The default is false.
	Full bool
}

type metaRetryConfig struct {
	// The total number of times to retry a metadata request when the
	// cluster is in the middle of a leader election or at startup (default 3).
	Max int
	// How long to wait for leader election to occur before retrying
	// default 250ms
	Backoff time.Duration
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

type FlusherFunc func(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error

// NewFlusherKafka Kafka flusher default config
func NewFlusherKafka() *FlusherKafka {
	return &FlusherKafka{
		Brokers:            nil,
		ClientID:           "LogtailPlugin",
		PartitionerType:    PartitionerTypeRandom,
		Timeout:            30 * time.Second,
		BulkMaxSize:        2048,
		BulkFlushFrequency: 0,
		Metadata: metaConfig{
			Retry: metaRetryConfig{
				Max:     3,
				Backoff: 250 * time.Millisecond,
			},
			RefreshFrequency: 10 * time.Minute,
			Full:             false,
		},
		KeepAlive:        0,
		MaxMessageBytes:  nil, // use library default
		RequiredACKs:     nil, // use library default
		MaxOpenRequests:  5,
		BrokerTimeout:    10 * time.Second,
		Compression:      "none",
		CompressionLevel: 4,
		Version:          "1.0.0",
		MaxRetries:       3,
		Headers:          nil,
		Backoff: backoffConfig{
			Init: 1 * time.Second,
			Max:  60 * time.Second,
		},
		ChanBufferSize: 256,
		Authentication: Authentication{
			PlainText: &PlainTextConfig{
				Username: "",
				Password: "",
			},
		},
		Convert: convertConfig{
			Protocol: converter.ProtocolCustomSingle,
			Encoding: converter.EncodingJSON,
		},
	}
}
func (k *FlusherKafka) Init(context pipeline.Context) error {
	k.context = context
	if k.Brokers == nil || len(k.Brokers) == 0 {
		var err = errors.New("brokers ip is nil")
		logger.Error(k.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init kafka flusher fail, error", err)
		return err
	}

	// Set default value while not set
	if k.Convert.Encoding == "" {
		k.converter.Encoding = converter.EncodingJSON
	}

	if k.Convert.Protocol == "" {
		k.Convert.Protocol = converter.ProtocolCustomSingle
	}

	convert, err := k.getConverter()
	if err != nil {
		logger.Error(k.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init kafka flusher converter fail, error", err)
		return err
	}
	k.converter = convert

	// Obtain topic keys from dynamic topic expression
	topicKeys, err := fmtstr.CompileKeys(k.Topic)
	if err != nil {
		logger.Error(k.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init kafka flusher fail, error", err)
		return err
	}
	k.topicKeys = topicKeys

	// Init headers
	k.recordHeaders = k.makeHeaders()

	saramaConfig, err := newSaramaConfig(k)
	if err != nil {
		logger.Error(k.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init kafka flusher fail, error", err)
		return err
	}

	producer, err := sarama.NewAsyncProducer(k.Brokers, saramaConfig)
	if err != nil {
		logger.Error(k.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init kafka flusher fail, error", err)
		return err
	}
	// Merge topicKeys and HashKeys,Only one convert after merge
	k.selectFields = util.UniqueStrings(k.topicKeys, k.HashKeys)

	SIGTERM := make(chan bool)
	go func(p sarama.AsyncProducer, SIGTERM chan bool) {
		errors := p.Errors()
		success := p.Successes()
		for {
			select {
			case err := <-errors:
				if err != nil {
					logger.Error(k.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush kafka write data fail, error", err)
				}
			case <-success:
				// Do Nothing
			case <-SIGTERM:
				return
			}
		}
	}(producer, SIGTERM)

	k.producer = producer
	k.isTerminal = SIGTERM
	return nil
}

func (k *FlusherKafka) Description() string {
	return "Kafka flusher for logtail"
}

func (k *FlusherKafka) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	return k.flusher(projectName, logstoreName, configName, logGroupList)
}

func (k *FlusherKafka) NormalFlush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	topic := k.Topic
	for _, logGroup := range logGroupList {
		logger.Debug(k.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)
		logs, values, err := k.converter.ToByteStreamWithSelectedFields(logGroup, k.topicKeys)
		if err != nil {
			logger.Error(k.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush kafka convert log fail, error", err)
		}
		for index, log := range logs.([][]byte) {
			if len(k.topicKeys) > 0 {
				valueMap := values[index]
				formattedTopic, err := fmtstr.FormatTopic(valueMap, k.Topic)
				if err != nil {
					logger.Error(k.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush kafka format topic fail, error", err)
				} else {
					topic = *formattedTopic
				}
			}
			m := &sarama.ProducerMessage{
				Topic:   topic,
				Value:   sarama.ByteEncoder(log),
				Headers: k.recordHeaders,
			}
			k.producer.Input() <- m
		}
	}
	return nil
}

func (k *FlusherKafka) HashFlush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	topic := k.Topic
	for _, logGroup := range logGroupList {
		logger.Debug(k.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)
		logs, values, err := k.converter.ToByteStreamWithSelectedFields(logGroup, k.selectFields)
		if err != nil {
			logger.Error(k.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush kafka convert log fail, error", err)
		}
		for index, log := range logs.([][]byte) {
			selectedValueMap := values[index]
			if len(k.topicKeys) > 0 {
				formattedTopic, err := fmtstr.FormatTopic(selectedValueMap, k.Topic)
				if err != nil {
					logger.Error(k.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush kafka format topic fail, error", err)
				} else {
					topic = *formattedTopic
				}
			}
			m := &sarama.ProducerMessage{
				Topic:   topic,
				Value:   sarama.ByteEncoder(log),
				Headers: k.recordHeaders,
			}
			// set key when partition type is hash
			if k.HashOnce {
				if len(k.hashKey) == 0 {
					k.hashKey = k.hashPartitionKey(selectedValueMap, logstoreName)
				}
				m.Key = k.hashKey
			} else {
				m.Key = k.hashPartitionKey(selectedValueMap, logstoreName)
			}
			k.producer.Input() <- m
		}
	}
	return nil
}

func (k *FlusherKafka) hashPartitionKey(valueMap map[string]string, defaultKey string) sarama.StringEncoder {
	var hashData []string
	for key, value := range valueMap {
		if _, ok := k.hashKeyMap[key]; ok {
			hashData = append(hashData, value)
		}
	}
	if len(hashData) == 0 {
		hashData = append(hashData, defaultKey)
	}
	logger.Debug(k.context.GetRuntimeContext(), "partition key", hashData, " hashKeyMap", k.hashKeyMap)
	return sarama.StringEncoder(strings.Join(hashData, "###"))
}

func (*FlusherKafka) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (k *FlusherKafka) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return k.producer != nil
}

// Stop ...
func (k *FlusherKafka) Stop() error {
	err := k.producer.Close()
	close(k.isTerminal)
	return err
}

func newSaramaConfig(config *FlusherKafka) (*sarama.Config, error) {
	partitioner, err := makePartitioner(config)
	if err != nil {
		return nil, err
	}

	k := sarama.NewConfig()

	// configure network level properties
	k.Net.MaxOpenRequests = config.MaxOpenRequests
	timeout := config.Timeout
	k.Net.DialTimeout = timeout
	k.Net.ReadTimeout = timeout
	k.Net.WriteTimeout = timeout
	k.Net.KeepAlive = config.KeepAlive
	k.Producer.Timeout = config.BrokerTimeout
	k.Producer.CompressionLevel = config.CompressionLevel

	// configure Authentication
	err = config.Authentication.ConfigureAuthentication(k)
	if err != nil {
		return nil, err
	}
	// configure kafka version
	version, ok := config.Version.Get()
	if !ok {
		return nil, fmt.Errorf("unknown/unsupported kafka version: %v", config.Version)
	}
	k.Version = version

	// configure metadata update properties
	k.Metadata.Retry.Max = config.Metadata.Retry.Max
	k.Metadata.Retry.Backoff = config.Metadata.Retry.Backoff
	k.Metadata.RefreshFrequency = config.Metadata.RefreshFrequency
	k.Metadata.Full = config.Metadata.Full

	// configure producer API properties
	if config.MaxMessageBytes != nil {
		k.Producer.MaxMessageBytes = *config.MaxMessageBytes
	}
	if config.RequiredACKs != nil {
		k.Producer.RequiredAcks = sarama.RequiredAcks(*config.RequiredACKs)
	}

	compressionMode, err := saramaProducerCompressionCodec(strings.ToLower(config.Compression))
	if err != nil {
		return nil, err
	}
	k.Producer.Compression = compressionMode

	k.Producer.Return.Successes = true // enable return channel for signaling
	k.Producer.Return.Errors = true

	retryMax := config.MaxRetries
	if retryMax < 0 {
		retryMax = 1000
	}
	k.Producer.Retry.Max = retryMax
	k.Producer.Retry.BackoffFunc = makeBackoffFunc(config.Backoff)

	// configure per broker go channel buffering
	k.ChannelBufferSize = config.ChanBufferSize

	// configure bulk size
	k.Producer.Flush.MaxMessages = config.BulkMaxSize
	if config.BulkFlushFrequency > 0 {
		k.Producer.Flush.Frequency = config.BulkFlushFrequency
	}

	// configure client ID
	k.ClientID = config.ClientID

	k.Producer.Partitioner = partitioner

	if err := k.Validate(); err != nil {
		logger.Error(config.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "Invalid kafka configuration, error", err)
		return nil, err
	}
	return k, nil
}

// makeBackoffFunc returns a stateless implementation of exponential-backoff-with-jitter. It is conceptually
// equivalent to the stateful implementation used by other outputs, EqualJitterBackoff.
func makeBackoffFunc(cfg backoffConfig) func(retries, maxRetries int) time.Duration {
	maxBackoffRetries := int(math.Ceil(math.Log2(float64(cfg.Max) / float64(cfg.Init))))
	return func(retries, _ int) time.Duration {
		// compute 'base' duration for exponential backoff
		dur := cfg.Max
		if retries < maxBackoffRetries {
			dur = time.Duration(uint64(cfg.Init) * uint64(1<<retries))
		}
		// apply about equaly distributed jitter in second half of the interval, such that the wait
		// time falls into the interval [dur/2, dur]
		limit := int64(dur / 2)
		jitter, _ := cryptorand.Int(cryptorand.Reader, big.NewInt(limit+1))
		return time.Duration(limit + jitter.Int64())
	}
}

func (k *FlusherKafka) Validate() error {
	if len(k.Brokers) == 0 {
		return errors.New("no hosts configured")
	}

	// check topic
	if k.Topic == "" {
		return errors.New("topic can't be empty")
	}

	if _, err := saramaProducerCompressionCodec(strings.ToLower(k.Compression)); err != nil {
		return err
	}

	if err := k.Version.Validate(); err != nil {
		return err
	}

	if k.Compression == "gzip" {
		lvl := k.CompressionLevel
		if lvl != sarama.CompressionLevelDefault && !(0 <= lvl && lvl <= 9) {
			return fmt.Errorf("compression_level must be between 0 and 9")
		}
	}
	return nil
}

func makePartitioner(k *FlusherKafka) (partitioner sarama.PartitionerConstructor, err error) {
	switch k.PartitionerType {
	case PartitionerTypeRoundRobin:
		partitioner = sarama.NewRoundRobinPartitioner
	case PartitionerTypeRoundHash:
		partitioner = sarama.NewHashPartitioner
		k.hashKeyMap = make(map[string]struct{})
		k.hashKey = ""
		for _, key := range k.HashKeys {
			k.hashKeyMap[key] = struct{}{}
		}
		k.flusher = k.HashFlush
	case PartitionerTypeRandom:
		partitioner = sarama.NewRandomPartitioner
	default:
		return nil, fmt.Errorf("invalid PartitionerType,configured value %v", k.PartitionerType)
	}
	return partitioner, nil
}

func saramaProducerCompressionCodec(compression string) (sarama.CompressionCodec, error) {
	switch compression {
	case "none":
		return sarama.CompressionNone, nil
	case "gzip":
		return sarama.CompressionGZIP, nil
	case "snappy":
		return sarama.CompressionSnappy, nil
	case "lz4":
		return sarama.CompressionLZ4, nil
	case "zstd":
		return sarama.CompressionZSTD, nil
	default:
		return sarama.CompressionNone, fmt.Errorf("producer.compression should be one of 'none', 'gzip', 'snappy', 'lz4', or 'zstd'. configured value %v", compression)
	}
}

func (k *FlusherKafka) makeHeaders() []sarama.RecordHeader {
	if len(k.Headers) != 0 {
		recordHeaders := make([]sarama.RecordHeader, 0, len(k.Headers))
		for _, h := range k.Headers {
			if h.Key == "" {
				continue
			}
			recordHeader := sarama.RecordHeader{
				Key:   []byte(h.Key),
				Value: []byte(h.Value),
			}
			recordHeaders = append(recordHeaders, recordHeader)
		}
		return recordHeaders
	}
	return nil
}

func (k *FlusherKafka) getConverter() (*converter.Converter, error) {
	logger.Debug(k.context.GetRuntimeContext(), "[ilogtail data convert config] Protocol", k.Convert.Protocol, "Encoding", k.Convert.Encoding, "TagFieldsRename", k.Convert.TagFieldsRename, "ProtocolFieldsRename", k.Convert.ProtocolFieldsRename)
	return converter.NewConverter(k.Convert.Protocol, k.Convert.Encoding, k.Convert.TagFieldsRename, k.Convert.ProtocolFieldsRename)
}

func init() {
	pipeline.Flushers["flusher_kafka_v2"] = func() pipeline.Flusher {
		f := NewFlusherKafka()
		f.flusher = f.NormalFlush
		return f
	}
}
