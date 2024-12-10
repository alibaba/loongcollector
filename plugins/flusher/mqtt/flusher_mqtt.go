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

package mqtt

import (
	"errors"
	"strconv"
	"time"

	MQTT "github.com/eclipse/paho.mqtt.golang"
	jsoniter "github.com/json-iterator/go"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type FlusherMqtt struct {
	context      pipeline.Context
	Server       string
	Topic        string
	ClientID     string
	Qos          int
	Username     string
	Password     string
	SSLCA        string // Path to CA file
	SSLCert      string // Path to host cert file
	SSLKey       string // Path to cert key file
	RetryMin     int
	RetryRatio   float64
	RetryMax     int
	CleanSession bool
	Version      int // 3 - MQTT 3.1 or 4 - MQTT 3.1.1
	TopicField   string

	isTerminal chan struct{}
	client     MQTT.Client
}

func (m *FlusherMqtt) Init(context pipeline.Context) error {
	m.context = context
	if len(m.Server) == 0 {
		m.Server = "tcp://127.0.0.1:1883"
	}
	if len(m.ClientID) == 0 {
		m.ClientID = util.GetIPAddress()
	}

	connOpts := MQTT.NewClientOptions().AddBroker(m.Server).SetCleanSession(m.CleanSession)
	if len(m.SSLCA) > 0 || len(m.SSLCert) > 0 || len(m.SSLKey) > 0 {
		tlsConfig, err := util.GetTLSConfig(m.SSLCert, m.SSLKey, m.SSLCA, true)
		if err != nil {
			return err
		}
		if tlsConfig != nil {
			connOpts.SetTLSConfig(tlsConfig)
		}
	}
	if len(m.Username) != 0 {
		connOpts.SetUsername(m.Username)
	}
	if len(m.Password) != 0 {
		connOpts.SetPassword(m.Password)
	}
	connOpts.SetAutoReconnect(true)
	connOpts.SetClientID(m.ClientID)
	logger.Info(m.context.GetRuntimeContext(), "begin connect", m.Server, "client", connOpts.ClientID, "detail", *connOpts)
	connOpts.SetDefaultPublishHandler(func(client MQTT.Client, message MQTT.Message) {

	})
	connOpts.SetProtocolVersion(uint(m.Version))
	connOpts.OnConnect = func(c MQTT.Client) {
	}
	connOpts.OnConnectionLost = func(c MQTT.Client, err error) {
		logger.Error(m.context.GetRuntimeContext(), "MQTT_CONNECTION_LOST_ALARM", "connection lost", m.Server, "error", err)
	}
	client := MQTT.NewClient(connOpts)

	waitTime := time.Duration(m.RetryMin) * time.Second
	m.isTerminal = make(chan struct{})

	var err error
	connSuccess := false
	for {
		if token := client.Connect(); token.Wait() && token.Error() != nil {
			logger.Warning(m.context.GetRuntimeContext(), "MQTT_CONNECT_ALARM", "connect", m.Server, "error", token.Error())
			if util.RandomSleep(waitTime, 0.1, m.isTerminal) {
				err = errors.New("MQTT connect failed")
				break
			}
			waitTime = time.Duration(m.RetryRatio * float64(waitTime))
			if waitTime > time.Second*time.Duration(m.RetryMax) {
				waitTime = time.Second * time.Duration(m.RetryMax)
			}
		} else {
			logger.Info(m.context.GetRuntimeContext(), "connected to", m.Server)
			connSuccess = true
			break
		}
	}
	if !connSuccess {
		return err
	}

	m.client = client
	return nil
}

func (m *FlusherMqtt) Description() string {
	return "mqtt flusher for logtail"
}

func (m *FlusherMqtt) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		logger.Debug(m.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)

		for _, log := range logGroup.Logs {
			if len(log.Contents) == 0 {
				continue
			}
			topic, buf := m.FormatArray2Map(log)
			if m.Topic != "" {
				topic = m.Topic
			}
			logger.Debug(m.context.GetRuntimeContext(), string(buf))
			m.client.Publish(topic, byte(m.Qos), false, string(buf))
		}
	}

	return nil
}

func (m *FlusherMqtt) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (m *FlusherMqtt) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return m.client != nil
}

// Stop ...
func (m *FlusherMqtt) Stop() error {
	m.client.Disconnect(1)
	close(m.isTerminal)
	return nil
}

func (m *FlusherMqtt) FormatArray2Map(log *protocol.Log) (string, []byte) {
	writer := jsoniter.NewStream(jsoniter.ConfigDefault, nil, 128)
	writer.WriteObjectStart()
	topic := ""
	for _, c := range log.Contents {
		if c.Key == m.TopicField {
			topic = c.Value
			continue
		}
		writer.WriteObjectField(c.Key)
		writer.WriteString(c.Value)
		_, _ = writer.Write([]byte{','})
	}
	writer.WriteObjectField("collectTime")
	writer.WriteString(strconv.Itoa(int(log.Time)))
	writer.WriteObjectEnd()

	return topic, writer.Buffer()
}

func init() {
	pipeline.Flushers["flusher_mqtt"] = func() pipeline.Flusher {
		f := &FlusherMqtt{
			RetryMin:   1,
			RetryRatio: 2.0,
			RetryMax:   300,
			TopicField: "topic",
		}
		return f
	}
}
