// Copyright 2026 iLogtail Authors
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
	"encoding/json"
	"errors"
	"fmt"
	"time"

	paho "github.com/eclipse/paho.mqtt.golang"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const defaultConnectTimeout = 30 * time.Second

type publisher interface {
	Publish(topic string, qos byte, retained bool, payload interface{}) paho.Token
	IsConnected() bool
	Disconnect(quiesce uint)
}

// FlusherMQTT publishes one JSON object per log to an MQTT topic.
type FlusherMQTT struct {
	Server         string
	Topic          string
	ClientID       string
	Username       string
	Password       string
	QoS            byte
	Retained       bool
	ConnectTimeout time.Duration

	context pipeline.Context
	client  publisher
}

var _ pipeline.FlusherV1 = (*FlusherMQTT)(nil)

func NewMQTTFlusher() *FlusherMQTT {
	return &FlusherMQTT{ConnectTimeout: defaultConnectTimeout}
}

func (f *FlusherMQTT) Description() string {
	return "mqtt flusher for logtail"
}

func (f *FlusherMQTT) Init(context pipeline.Context) error {
	f.context = context
	if f.Server == "" {
		return errors.New("mqtt server is required")
	}
	if f.Topic == "" {
		return errors.New("mqtt topic is required")
	}
	if f.QoS > 2 {
		return fmt.Errorf("mqtt qos must be 0, 1, or 2, got %d", f.QoS)
	}
	if f.ConnectTimeout <= 0 {
		f.ConnectTimeout = defaultConnectTimeout
	}

	options := paho.NewClientOptions().AddBroker(f.Server).SetClientID(f.ClientID)
	options.SetUsername(f.Username)
	options.SetPassword(f.Password)
	client := paho.NewClient(options)
	token := client.Connect()
	if !token.WaitTimeout(f.ConnectTimeout) {
		return fmt.Errorf("connect to mqtt server %q timed out", f.Server)
	}
	if err := token.Error(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherInitAlarm, "mqtt connect failed", err)
		return fmt.Errorf("connect to mqtt server %q: %w", f.Server, err)
	}
	f.client = client
	return nil
}

func (f *FlusherMQTT) IsReady(string, string, int64) bool {
	return f.client != nil && f.client.IsConnected()
}

func (f *FlusherMQTT) SetUrgent(bool) {}

func (f *FlusherMQTT) Stop() error {
	if f.client != nil {
		f.client.Disconnect(1000)
		f.client = nil
	}
	return nil
}

func (f *FlusherMQTT) Flush(_ string, _ string, _ string, groups []*protocol.LogGroup) error {
	if !f.IsReady("", "", 0) {
		return errors.New("mqtt client is not connected")
	}
	for _, group := range groups {
		for _, log := range group.Logs {
			payload, err := marshalLog(log)
			if err != nil {
				return err
			}
			token := f.client.Publish(f.Topic, f.QoS, f.Retained, payload)
			if !token.WaitTimeout(f.ConnectTimeout) {
				return fmt.Errorf("publish to mqtt topic %q timed out", f.Topic)
			}
			if err := token.Error(); err != nil {
				return fmt.Errorf("publish to mqtt topic %q: %w", f.Topic, err)
			}
		}
	}
	return nil
}

func marshalLog(log *protocol.Log) ([]byte, error) {
	fields := make(map[string]string, len(log.Contents)+1)
	for _, content := range log.Contents {
		fields[content.Key] = content.Value
	}
	fields["__time__"] = fmt.Sprint(log.Time)
	return json.Marshal(fields)
}

func init() {
	pipeline.AddFlusherCreator("flusher_mqtt", func() pipeline.Flusher {
		return NewMQTTFlusher()
	})
}
