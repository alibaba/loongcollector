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
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/plugins/test"
)

func TestSharedUDPServerRoutesStatsDLogs(t *testing.T) {
	statsd, err := decoder.GetDecoder(common.ProtocolStatsd)
	require.NoError(t, err)
	first := helper.NewObservePipelineContext(2).Collector()
	second := helper.NewObservePipelineContext(2).Collector()
	server := &SharedUDPServer{
		dispatchKey: "instance",
		lastLog:     time.Now(),
		udp:         &UDPServer{Format: common.ProtocolStatsd, decoder: statsd},
		collectors:  make(map[string]pipeline.PipelineCollector),
	}
	server.RegisterCollectors("a", first)
	server.RegisterCollectors("b", second)

	groups, err := server.udp.decodeDatagram([]byte("gauge:1|g|#instance:a,instance_peer:other\ngauge:2|g|#instance:b,env:test\ngauge:3|g|#instance_peer:ignored"))
	require.NoError(t, err)
	groups[0].Group.Metadata.Add("source", "packet")
	server.dispatcher(groups...)

	res := first.ToArray()
	require.Len(t, res, 1)
	require.Equal(t, "packet", res[0].Group.GetMetadata().Get("source"))
	require.Len(t, res[0].Events, 1)
	log := res[0].Events[0].(*models.Log)
	require.Empty(t, log.GetTags().Iterator())
	require.Equal(t, "instance_peer#$#other", log.GetIndices().Get("__labels__"))
	require.Equal(t, "1", log.GetIndices().Get("__value__"))

	res = second.ToArray()
	require.Len(t, res, 1)
	require.Equal(t, "packet", res[0].Group.GetMetadata().Get("source"))
	require.Len(t, res[0].Events, 1)
	log = res[0].Events[0].(*models.Log)
	require.Empty(t, log.GetTags().Iterator())
	require.Equal(t, "env#$#test", log.GetIndices().Get("__labels__"))
	require.Equal(t, "2", log.GetIndices().Get("__value__"))
}

func TestUDPDatagramPreservesRawLogAndSLSTags(t *testing.T) {
	raw, err := decoder.GetDecoder(common.ProtocolRaw)
	require.NoError(t, err)
	packet := []byte("udp contents")
	u := &UDPServer{Format: common.ProtocolRaw, decoder: raw}
	groups, err := u.decodeDatagram(packet)
	require.NoError(t, err)
	clear(packet)
	require.Len(t, groups, 1)
	require.Len(t, groups[0].Events, 1)
	log := groups[0].Events[0].(*models.Log)
	require.Equal(t, "udp contents", string(log.GetBody()))
	require.WithinDuration(t, time.Now(), time.Unix(0, int64(log.GetTimestamp())), 2*time.Second)
	require.Zero(t, log.GetTimestamp()%uint64(time.Second))

	sls, err := decoder.GetDecoder(common.ProtocolSLS)
	require.NoError(t, err)
	nanos := uint32(123456789)
	encoded := test.AppendSLSWireLog(nil, 1700000000, &nanos, [2]string{"content", "message"})
	encoded = protowire.AppendTag(encoded, 3, protowire.BytesType)
	encoded = protowire.AppendString(encoded, "udp-topic")
	encoded = protowire.AppendTag(encoded, 4, protowire.BytesType)
	encoded = protowire.AppendString(encoded, "udp-origin")
	encoded = test.AppendSLSWireTag(encoded, "env", "test")
	u = &UDPServer{Format: common.ProtocolSLS, decoder: sls}
	groups, err = u.decodeDatagram(encoded)
	require.NoError(t, err)
	require.Len(t, groups, 1)
	require.Equal(t, "udp-origin", groups[0].Group.GetMetadata().Get("source"))
	require.Equal(t, "udp-topic", groups[0].Group.GetTags().Get(pipeline.TopicTagKey))
	require.Equal(t, "test", groups[0].Group.GetTags().Get("env"))
	require.Equal(t, uint64(1700000000123456789), groups[0].Events[0].GetTimestamp())
}

func TestUDPStatsDPreservesLegacyLogFields(t *testing.T) {
	opts := &config.LoongcollectorGlobalConfig
	wasEnabled := opts.EnableSlsMetricsFormat
	t.Cleanup(func() { opts.EnableSlsMetricsFormat = wasEnabled })
	statsd, err := decoder.GetDecoder(common.ProtocolStatsd)
	require.NoError(t, err)
	u := &UDPServer{Format: common.ProtocolStatsd, decoder: statsd}

	for _, tc := range []struct {
		enabled bool
		name    string
		labels  string
	}{
		{false, "app.requests", "a#$#first|x.y#$#other"},
		{true, "app_requests", "a#$#first|x_y#$#other"},
	} {
		opts.EnableSlsMetricsFormat = tc.enabled
		groups, err := u.decodeDatagram([]byte("app.requests:3.5|g|#x.y:other,a:first"))
		require.NoError(t, err)
		require.Len(t, groups, 1)
		require.Len(t, groups[0].Events, 1)
		log := groups[0].Events[0].(*models.Log)
		require.Equal(t, tc.name, log.GetIndices().Get("__name__"))
		require.Equal(t, tc.labels, log.GetIndices().Get("__labels__"))
		require.Equal(t, "3.5", log.GetIndices().Get("__value__"))
		require.Equal(t, strconv.FormatUint(log.GetTimestamp(), 10), log.GetIndices().Get("__time_nano__"))
		require.Empty(t, log.GetTags().Iterator())
	}
}
