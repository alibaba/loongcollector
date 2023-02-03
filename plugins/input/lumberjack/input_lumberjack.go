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

// Lumberjack server test tool.
//
// Create lumberjack server endpoint ACKing all received batches only. The
// server supports all lumberjack protocol versions, which must be explicitly enabled
// from command line. For printing list of known command line flags run:
//
//	tst-lj -h
package lumberjack

import (
	"context"
	"crypto/tls"
	"errors"
	"sync"
	"time"

	lumberlog "github.com/elastic/go-lumber/log"
	"github.com/elastic/go-lumber/server"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

var errDecode = errors.New("decode error")

var rawJSONDecoder = func(input []byte, out interface{}) error {
	event, ok := out.(*interface{})
	if !ok {
		return errDecode
	}
	jsonStr := string(input)
	*event = jsonStr
	return nil
}

type defaultLogger struct{}

func (defaultLogger) Printf(format string, args ...interface{}) {
	logger.Infof(context.Background(), format, args...)
}

func (defaultLogger) Println(args ...interface{}) {
	logger.Info(context.Background(), args...)
}

func (defaultLogger) Print(args ...interface{}) {
	logger.Info(context.Background(), args...)
}

type ServiceLumber struct {
	BindAddress string
	V1          bool
	V2          bool
	// Path to CA file
	SSLCA string
	// Path to host cert file
	SSLCert string
	// Path to cert key file
	SSLKey string
	// Use SSL but skip chain & host verification
	InsecureSkipVerify bool

	server    server.Server
	shutdown  chan struct{}
	waitGroup sync.WaitGroup
	context   pipeline.Context
}

func (p *ServiceLumber) Init(context pipeline.Context) (int, error) {
	p.context = context
	return 0, nil
}

func (p *ServiceLumber) Description() string {
	return "lumberjack input plugin for logtail"
}

// Gather takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (p *ServiceLumber) Collect(pipeline.Collector) error {
	return nil
}

// Start starts the ServiceInput's service, whatever that may be
func (p *ServiceLumber) Start(c pipeline.Collector) error {
	p.shutdown = make(chan struct{})
	p.waitGroup.Add(1)
	defer p.waitGroup.Done()
	var err error
	for {
		logger.Info(p.context.GetRuntimeContext(), "start listen lumber, address", p.BindAddress)

		if p.SSLCert == "" && p.SSLKey == "" && p.SSLCA == "" && !p.InsecureSkipVerify {
			p.server, err = server.ListenAndServe(p.BindAddress,
				server.V1(p.V1),
				server.V2(p.V2),
				server.JSONDecoder(rawJSONDecoder),
			)
		} else {
			var tlsConfig *tls.Config
			tlsConfig, err = util.GetTLSConfig(p.SSLCert, p.SSLKey, p.SSLCA, p.InsecureSkipVerify)
			if err != nil {
				logger.Error(p.context.GetRuntimeContext(), "LUMBER_LISTEN_ALARM", "init tls error", err)
			}
			p.server, err = server.ListenAndServe(p.BindAddress,
				server.V1(p.V1),
				server.V2(p.V2),
				server.JSONDecoder(rawJSONDecoder),
				server.TLS(tlsConfig),
			)
		}

		if err != nil {
			logger.Error(p.context.GetRuntimeContext(), "LUMBER_LISTEN_ALARM", "listen init error", err, "sleep 10 seconds and retry")
			if util.RandomSleep(time.Second*10, 0.1, p.shutdown) {
				return nil
			}
			continue
		}

		recvChan := p.server.ReceiveChan()

		keys := []string{"content"}
		values := []string{""}
	ForBlock:
		for {
			select {
			case batch := <-recvChan:
				if batch == nil {
					err = p.server.Close()
					logger.Error(p.context.GetRuntimeContext(), "LUMBER_CONNECTION_ALARM", "lumber server error", "chan closed", "close server, err", err)
					break ForBlock
				}
				for _, event := range batch.Events {
					if p.V2 {
						strValue, _ := event.(string)
						values[0] = strValue
						c.AddDataArray(nil, keys, values)
						logger.Debug(p.context.GetRuntimeContext(), "received event", strValue)
					} else {
						strMap, ok := event.(map[string]string)
						if ok && strMap != nil {
							c.AddData(nil, strMap)
						} else {
							logger.Debug(p.context.GetRuntimeContext(), "received invalid event")
						}
					}

				}
				batch.ACK()
			case <-p.shutdown:
				return p.server.Close()
			}
		}

	}
}

// Stop stops the services and closes any necessary channels and connections
func (p *ServiceLumber) Stop() error {
	close(p.shutdown)
	p.waitGroup.Wait()
	return nil
}

func init() {
	pipeline.ServiceInputs["service_lumberjack"] = func() pipeline.ServiceInput {
		return &ServiceLumber{
			BindAddress: "127.0.0.1:5044",
			V2:          true,
			V1:          false,
		}
	}
	lumberlog.Logger = defaultLogger{}
}
