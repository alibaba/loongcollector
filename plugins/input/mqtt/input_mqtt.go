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
	"crypto/tls"
	"fmt"
	"strconv"
	"sync"
	"time"

	MQTT "github.com/eclipse/paho.mqtt.golang"
	"github.com/pkg/errors"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

// ServiceMQTT ...
type ServiceMQTT struct {
	shutdown  chan struct{}
	waitGroup sync.WaitGroup
	context   pipeline.Context
	collector pipeline.Collector
	keys      []string
	id        int

	Server          string
	Topics          []string
	QoS             int
	ClientID        string
	Username        string
	Password        string
	SSLCA           string // Path to CA file
	SSLCert         string // Path to host cert file
	SSLKey          string // Path to cert key file
	RetryMin        int
	RetryRatio      float64
	RetryMax        int
	CleanSession    bool
	OrderMatters    bool
	ClientIDAutoInc bool
	KeepAlive       int
	Version         int // 3 - MQTT 3.1 or 4 - MQTT 3.1.1
}

type DebugLogger struct {
}

func (DebugLogger) Println(v ...interface{}) {
	fmt.Println(v...)
}
func (DebugLogger) Printf(format string, v ...interface{}) { fmt.Printf(format, v...) }

func (p *ServiceMQTT) Init(context pipeline.Context) (int, error) {
	p.context = context
	p.id = int(time.Now().Unix() % 100000)
	if len(p.Topics) == 0 {
		p.Topics = append(p.Topics, "#")
	}
	if len(p.Server) == 0 {
		p.Server = "tcp://127.0.0.1:1883"
	}
	if len(p.ClientID) == 0 {
		p.ClientID = util.GetHostName() + "_" + strconv.Itoa(time.Now().Second())
	}
	p.keys = append(p.keys, "server")
	p.keys = append(p.keys, "topic")
	p.keys = append(p.keys, "duplicated")
	p.keys = append(p.keys, "retained")
	p.keys = append(p.keys, "message_id")
	p.keys = append(p.keys, "content")
	return 0, nil
}

func (p *ServiceMQTT) Description() string {
	return "mqtt service input plugin for logtail"
}

// Collect takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (p *ServiceMQTT) Collect(pipeline.Collector) error {
	return nil
}

func (p *ServiceMQTT) onMessageReceived(client MQTT.Client, message MQTT.Message) {
	logger.Debug(p.context.GetRuntimeContext(), "Received message on topic", message.Topic())
	values := make([]string, 6)
	values[0] = p.Server
	values[1] = message.Topic()
	if message.Duplicate() {
		values[2] = "true"
	} else {
		values[2] = "false"
	}

	if message.Retained() {
		values[3] = "true"
	} else {
		values[3] = "false"
	}

	values[4] = strconv.Itoa(int(message.MessageID()))
	values[5] = string(message.Payload())

	p.collector.AddDataArray(nil, p.keys, values)
}

func (p *ServiceMQTT) createClient(tlsConfig *tls.Config, connLostChannel chan struct{}) (MQTT.Client, error) {
	connOpts := MQTT.NewClientOptions().AddBroker(p.Server).SetCleanSession(p.CleanSession)
	if tlsConfig != nil {
		connOpts.SetTLSConfig(tlsConfig)
	}
	if len(p.Username) != 0 {
		connOpts.SetUsername(p.Username)
	}
	if len(p.Password) != 0 {
		connOpts.SetPassword(p.Password)
	}

	if p.ClientIDAutoInc {
		connOpts.SetAutoReconnect(false)
		connOpts.SetClientID(p.ClientID + strconv.Itoa(p.id))
		p.id++
	} else {
		connOpts.SetAutoReconnect(true)
		connOpts.SetClientID(p.ClientID)
	}

	logger.Info(p.context.GetRuntimeContext(), "begin connect", p.Server, "client", connOpts.ClientID, "detail", *connOpts)

	// @note work around for topic filter's bug
	connOpts.SetDefaultPublishHandler(p.onMessageReceived)
	connOpts.SetProtocolVersion(uint(p.Version))
	connOpts.OnConnect = func(c MQTT.Client) {
	}

	connOpts.OnConnectionLost = func(c MQTT.Client, err error) {
		logger.Error(p.context.GetRuntimeContext(), "MQTT_CONNECTION_LOST_ALARM", "connection lost", p.Server, "error", err)
		if connLostChannel != nil {
			connLostChannel <- struct{}{}
		}
	}

	client := MQTT.NewClient(connOpts)

	waitTime := time.Duration(p.RetryMin) * time.Second
	var err error
	connSuccess := false
	for {
		if token := client.Connect(); token.Wait() && token.Error() != nil {
			logger.Warning(p.context.GetRuntimeContext(), "MQTT_CONNECT_ALARM", "connect", p.Server, "error", token.Error())
			if util.RandomSleep(waitTime, 0.1, p.shutdown) {
				err = errors.New("MQTT connect failed")
				break
			}
			waitTime = time.Duration(p.RetryRatio * float64(waitTime))
			if waitTime > time.Second*time.Duration(p.RetryMax) {
				waitTime = time.Second * time.Duration(p.RetryMax)
			}
		} else {
			logger.Info(p.context.GetRuntimeContext(), "connected to", p.Server)
			connSuccess = true
			multiTopics := make(map[string]byte)

			for _, topic := range p.Topics {
				multiTopics[topic] = byte(p.QoS)
			}
			if token := client.SubscribeMultiple(multiTopics, nil); token.Wait() && token.Error() != nil {
				logger.Error(p.context.GetRuntimeContext(), "MQTT_SUBSCRIBE_ALARM", "subscribe topic", multiTopics, "error", token.Error())
			} else {
				logger.Info(p.context.GetRuntimeContext(), "subscribe success, topic", multiTopics)
			}
			break
		}

	}
	if connSuccess {
		return client, nil
	}
	return nil, err
}

// Start starts the ServiceInput's service, whatever that may be
func (p *ServiceMQTT) Start(c pipeline.Collector) error {
	p.shutdown = make(chan struct{})
	p.collector = c
	p.waitGroup.Add(1)
	defer p.waitGroup.Done()

	var tlsConfig *tls.Config

	var err error
	if len(p.SSLCA) > 0 || len(p.SSLCert) > 0 || len(p.SSLKey) > 0 {
		tlsConfig, err = util.GetTLSConfig(p.SSLCert, p.SSLKey, p.SSLCA, true)
		if err != nil {
			return err
		}
	}

	if p.ClientIDAutoInc {
		waitTime := time.Duration(p.RetryMin) * time.Second
		for {
			closeChan := make(chan struct{}, 1)
			client, _ := p.createClient(tlsConfig, closeChan)
			if client != nil {
				waitTime = time.Duration(p.RetryMin) * time.Second
				select {
				case <-closeChan:
					close(closeChan)
					break
				case <-p.shutdown:
					client.Disconnect(1)
					return nil
				}
			}
			waitTime = time.Duration(p.RetryRatio * float64(waitTime))
			if waitTime > time.Second*time.Duration(p.RetryMax) {
				waitTime = time.Second * time.Duration(p.RetryMax)
			}
			time.Sleep(waitTime)
		}
	} else {
		client, _ := p.createClient(tlsConfig, nil)
		if client != nil {
			<-p.shutdown
			client.Disconnect(1)
		}
	}
	return nil
}

// Stop stops the services and closes any necessary channels and connections
func (p *ServiceMQTT) Stop() error {
	close(p.shutdown)
	p.waitGroup.Wait()
	return nil
}

func init() {
	pipeline.ServiceInputs["service_mqtt"] = func() pipeline.ServiceInput {
		return &ServiceMQTT{
			RetryMin:     1,
			RetryRatio:   2.0,
			RetryMax:     300,
			KeepAlive:    30,
			OrderMatters: true,
			CleanSession: false,
		}
	}
}
