// Copyright 2022 iLogtail Authors
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

package udpserver

import (
	"bytes"
	"errors"
	"fmt"
	"net"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type UDPServer struct {
	Decoder       string
	Format        string
	Address       string
	MaxBufferSize int

	context pipeline.Context
	decoder extensions.Decoder
	addr    *net.UDPAddr
	conn    *net.UDPConn
}

func (u *UDPServer) Init(context pipeline.Context) (int, error) {
	u.context = context
	u.Format = strings.ToLower(strings.TrimSpace(u.Format))
	options := &struct {
		Format string
	}{
		Format: u.Format,
	}
	ext, err := context.GetExtension(u.Decoder, options)
	if err != nil {
		return 0, err
	}
	decoder, ok := ext.(extensions.Decoder)
	if !ok {
		return 0, fmt.Errorf("extension %s with type %T not implement extensions.Decoder", u.Decoder, ext)
	}
	u.decoder = decoder

	host, portStr, err := net.SplitHostPort(u.Address)
	if err != nil {
		logger.Warning(u.context.GetRuntimeContext(), selfmonitor.UDPServerAlarm, "illegal udp listening addr", u.Address, "err", err)
		return 0, err
	}
	if host == "" {
		host = "0.0.0.0"
	}

	ip, err := net.ResolveIPAddr("ip", host)
	if err != nil {
		logger.Warning(u.context.GetRuntimeContext(), selfmonitor.UDPServerAlarm, "unable resolve addr", u.Address, "err", err)
		return 0, err
	}

	port, err := strconv.Atoi(portStr)
	if err != nil || port < 0 || port > 65535 {
		logger.Warning(u.context.GetRuntimeContext(), selfmonitor.UDPServerAlarm, "illegal port", portStr, "err", err)
	}

	u.addr = &net.UDPAddr{
		IP:   ip.IP,
		Port: port,
		Zone: ip.Zone,
	}
	return 0, nil
}

func (u *UDPServer) Description() string {
	return "this is an udp listening server"
}

func (u *UDPServer) StartService(ctx pipeline.PipelineContext) error {
	err := u.doStart(ctx.Collector().CollectList)
	logger.Infof(u.context.GetRuntimeContext(), "start udp server, status", err == nil)
	return err
}

func (u *UDPServer) doStart(dispatch func(...*models.PipelineGroupEvents)) error {
	conn, err := net.ListenUDP("udp", u.addr)
	if err != nil {
		logger.Warning(u.context.GetRuntimeContext(), selfmonitor.UDPServerAlarm, "start udp server err", err)
		return err
	}
	u.conn = conn
	go func() {
		buf := make([]byte, u.MaxBufferSize)
		defer func() {
			logger.Debug(u.context.GetRuntimeContext(), "release udp read goroutine")
		}()
		for {
			n, _, err := conn.ReadFromUDP(buf)
			if err != nil {
				// https://github.com/golang/go/issues/4373
				// ignore net: errClosing error as it will occur during shutdown
				if errors.Is(err, net.ErrClosed) {
					return
				}
				logger.Warning(u.context.GetRuntimeContext(), selfmonitor.UDPServerAlarm, "read record err", err)
				return
			}
			groups, err := u.decodeDatagram(buf[:n])
			if err != nil {
				logger.Warning(u.context.GetRuntimeContext(), selfmonitor.UDPServerAlarm, "decode record err,some logs would be dropped", err)
			} else {
				dispatch(groups...)
			}
		}
	}()
	return nil
}

func (u *UDPServer) decodeDatagram(packet []byte) ([]*models.PipelineGroupEvents, error) {
	var request *http.Request
	switch u.Format {
	case common.ProtocolPrometheus, common.ProtocolInflux, common.ProtocolInfluxdb, common.ProtocolPyroscope:
		request = &http.Request{URL: &url.URL{}}
	case common.ProtocolOTLPLogV1, common.ProtocolOTLPMetricV1, common.ProtocolOTLPTraceV1:
		request = &http.Request{URL: &url.URL{}, Header: http.Header{"Content-Type": {"application/x-protobuf"}}}
	}

	// The next datagram reuses the read buffer, but decoders may retain the payload.
	groups, err := u.decoder.Decode(bytes.Clone(packet), request)
	if err != nil || u.Format != common.ProtocolRaw {
		return groups, err
	}
	for _, group := range groups {
		for i, event := range group.Events {
			if data, ok := event.(models.ByteArray); ok {
				log := helper.NewInputLog(nil, nil, time.Now().Truncate(time.Second))
				log.SetBody([]byte(data))
				group.Events[i] = log
			}
		}
	}
	return groups, nil
}

func (u *UDPServer) Stop() error {
	if u.conn != nil {
		_ = u.conn.Close()
		u.conn = nil
	}
	logger.Infof(u.context.GetRuntimeContext(), "stop udp server, success")
	return nil
}

func init() {
	pipeline.ServiceInputs["service_udp_server"] = func() pipeline.ServiceInput {
		return &UDPServer{
			MaxBufferSize: 65535,
			Decoder:       "ext_default_decoder",
		}
	}
}
