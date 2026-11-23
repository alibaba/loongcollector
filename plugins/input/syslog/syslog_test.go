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

package inputsyslog

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/pluginmanager"

	"fmt"
	"math/rand"
	"net"
	"os"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestGetAddressPartsHostFormat(t *testing.T) {
	cases := []struct {
		address    string
		wantScheme string
		wantHost   string
	}{
		{"tcp://127.0.0.1:9000", "tcp", "127.0.0.1:9000"},
		{"udp://127.0.0.1", "udp", "127.0.0.1:6514"}, // default port
		{"tcp6://[::1]:9000", "tcp6", "[::1]:9000"},  // IPv6 must stay bracketed
		{"tcp6://[::1]", "tcp6", "[::1]:6514"},       // IPv6 + default port
	}
	for _, c := range cases {
		scheme, host, err := getAddressParts(c.address)
		require.NoError(t, err, "address=%q", c.address)
		assert.Equal(t, c.wantScheme, scheme, "address=%q", c.address)
		assert.Equal(t, c.wantHost, host, "address=%q", c.address)

		// The produced host must round-trip through net.SplitHostPort, which is what
		// parseTargetPort relies on to derive the rsyslog target.
		_, _, splitErr := net.SplitHostPort(host)
		require.NoError(t, splitErr, "address=%q host=%q", c.address, host)
	}
}

func TestStartAndStop(t *testing.T) {
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("test_project", "test_logstore", "test_configname")

	syslog := newSyslog()
	collector := helper.NewObservePipelineContext(1024)
	_, _ = syslog.Init(ctx)
	go func() {
		err := syslog.StartService(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Second)

	const connCount = 10
	var conns []net.Conn
	for i := 0; i < connCount; i++ {
		conn := connect(t, syslog)
		log := getLog(int(rand.Int31n(24)*8+rand.Int31n(8)), "hostname", "program", "message", nil)
		_, err := conn.Write([]byte(log))
		assert.NoError(t, err)
		conns = append(conns, conn)
	}
	time.Sleep(time.Millisecond * 2)
	syslog.connectionsMu.Lock()
	require.Equal(t, connCount, len(syslog.connections))
	require.Equal(t, len(conns), len(syslog.connections))
	syslog.connectionsMu.Unlock()

	time.Sleep(time.Millisecond * 10)
	_ = syslog.Stop()
	require.Equal(t, 0, len(syslog.connections))
}

func connect(t *testing.T, syslog *Syslog) net.Conn {
	scheme, path, err := getAddressParts(syslog.Address)
	require.NoError(t, err)

	var host string
	switch {
	case scheme == "unixgram":
		host = path
	case syslog.tcpListener != nil:
		host = fmt.Sprintf("127.0.0.1:%d", syslog.tcpListener.Addr().(*net.TCPAddr).Port)
	case syslog.udpListener != nil:
		host = fmt.Sprintf("127.0.0.1:%d", syslog.udpListener.LocalAddr().(*net.UDPAddr).Port)
	}

	var conn net.Conn
	switch {
	case syslog.isStream:
		conn, err = net.Dial(scheme, host)
	case scheme == "unixgram":
		addr, e := net.ResolveUnixAddr(scheme, host)
		require.NoError(t, e)
		conn, err = net.DialUnix(scheme, nil, addr)
	default:
		addr, e := net.ResolveUDPAddr(scheme, host)
		require.NoError(t, e)
		conn, err = net.DialUDP(scheme, nil, addr)
	}
	require.NoError(t, err)
	return conn
}

func getLog(priority int, hostname string, program string, message string, t *time.Time) string {
	if t == nil {
		t = &time.Time{}
		*t = time.Now()
	}
	return fmt.Sprintf("<%v>%v %v %v: %v\n",
		priority, t.Format("Jan 02 15:04:05"), hostname, program, message)
}

//nolint:gosec
func mockRun(t *testing.T, syslog *Syslog, collector pipeline.PipelineContext) {
	rand.Seed(time.Now().UnixNano())

	reconnectCount := 0
	totalLogCount := 0
	var rawLogs []string

	conn := connect(t, syslog)
	for i := 0; i < 10; i++ {
		// Reconnect randomly (5%).
		if 0 == rand.Int31n(20) {
			reconnectCount++
			conn.Close()
			conn = connect(t, syslog)
			time.Sleep(time.Duration(1) * time.Second)
		}

		logCount := int(1 + rand.Int31n(10))
		batchLogs := ""
		for logIdx := 0; logIdx < logCount; logIdx++ {
			log := getLog(int(rand.Int31n(24)*8+rand.Int31n(8)), "hostname", "program", "message", nil)
			rawLogs = append(rawLogs, log)
			batchLogs += log
		}
		n, err := conn.Write([]byte(batchLogs))
		require.NoError(t, err)
		require.Equal(t, len(batchLogs), n)

		totalLogCount += logCount
	}

	time.Sleep(time.Duration(1) * time.Second)

	t1 := time.Now()
	assert.NoError(t, syslog.Stop())
	dur := time.Since(t1)
	require.True(t, dur/time.Microsecond < 3000, "dur: %v", dur)
	assert.NoError(t, conn.Close())

	groups := collector.Collector().ToArray()
	require.Len(t, groups, len(rawLogs))
	for idx, rawLog := range rawLogs {
		require.Len(t, groups[idx].Events, 1)
		event := groups[idx].Events[0].(*models.Log)
		fields := event.GetIndices()
		priority, err := strconv.Atoi(pipeline.GetStringValue(fields.Get("_priority_")))
		require.NoError(t, err)
		timestamp := time.Unix(0, int64(event.GetTimestamp()))
		log := getLog(priority, pipeline.GetStringValue(fields.Get("_hostname_")),
			pipeline.GetStringValue(fields.Get("_program_")), pipeline.GetStringValue(fields.Get("_content_")), &timestamp)
		if t.Name() != "TestMockUnixGram" {
			require.Equal(t, "127.0.0.1", fields.Get("_client_ip_"))
		}
		require.Equal(t, rawLog, log, "log index: %v", idx)
	}

	t.Logf("Total log count: %v, reconnect count: %v\n", totalLogCount, reconnectCount)

}

func TestMockTcpRun(t *testing.T) {
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("test_project", "test_logstore", "test_configname")
	collector := helper.NewObservePipelineContext(1024)

	syslog := newSyslog()
	syslog.ParseProtocol = "rfc3164"
	syslog.Address = "tcp://127.0.0.1:0"
	_, _ = syslog.Init(ctx)
	go func() {
		err := syslog.StartService(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)

	mockRun(t, syslog, collector)
}

func TestMockUdpRun(t *testing.T) {
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("test_project", "test_logstore", "test_configname")
	collector := helper.NewObservePipelineContext(1024)

	syslog := newSyslog()
	syslog.ParseProtocol = "rfc3164"
	syslog.Address = "udp://127.0.0.1:0"
	_, _ = syslog.Init(ctx)
	go func() {
		err := syslog.StartService(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)

	mockRun(t, syslog, collector)
}

func TestMockUnixGram(t *testing.T) {
	unixFilePath := "/tmp/mock_dev_log"
	if _, err := os.Stat(unixFilePath); !os.IsNotExist(err) {
		err := os.Remove(unixFilePath)
		require.Nil(t, err)
	}
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("test_project", "test_logstore", "test_configname")
	collector := helper.NewObservePipelineContext(1024)

	syslog := newSyslog()
	syslog.ParseProtocol = "rfc3164"
	syslog.Address = "unixgram://" + unixFilePath
	_, _ = syslog.Init(ctx)
	go func() {
		err := syslog.StartService(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)

	mockRun(t, syslog, collector)
}

func TestSyslogParseKeepsRawMessageFields(t *testing.T) {
	syslog := newSyslog()
	syslog.parser = syslogParsers[""](&parserConfig{})
	collector := helper.NewObservePipelineContext(2).Collector()

	input := []byte("first\nsecond\n")
	syslog.parse(input, clientIPField("192.0.2.1:514"), collector)
	// The receive buffer may be reused before the pipeline processes the messages.
	clear(input)

	groups := collector.ToArray()
	require.Len(t, groups, 2)
	for i, content := range []string{"first", "second"} {
		require.Len(t, groups[i].Events, 1)
		log := groups[i].Events[0].(*models.Log)
		require.Equal(t, map[string]any{
			"_program_":       "",
			"_priority_":      "-1",
			"_facility_":      "-1",
			"_severity_":      "-1",
			"_unixtimestamp_": strconv.FormatInt(int64(log.GetTimestamp()), 10),
			"_hostname_":      util.GetHostName(),
			"_client_ip_":     "192.0.2.1",
			"_ip_":            util.GetIPAddress(),
			"_content_":       content,
		}, log.GetIndices().Iterator())
	}

	first := groups[0].Events[0].(*models.Log)
	first.GetIndices().Iterator()["_content_"] = "changed"
	first.GetTags().Add("event", "tag")
	groups[0].Group.GetTags().Add("group", "tag")
	groups[0].Group.GetMetadata().Add("group", "metadata")

	second := groups[1].Events[0].(*models.Log)
	require.Equal(t, "changed", first.GetIndices().Get("_content_"))
	require.Equal(t, "tag", first.GetTags().Get("event"))
	require.Equal(t, "tag", groups[0].Group.GetTags().Get("group"))
	require.Equal(t, "metadata", groups[0].Group.GetMetadata().Get("group"))
	require.Equal(t, "second", second.GetIndices().Get("_content_"))
	require.Empty(t, second.GetTags().Iterator())
	require.Empty(t, groups[1].Group.GetTags().Iterator())
	require.Empty(t, groups[1].Group.GetMetadata().Iterator())
}

func TestSyslogTCPReadsLineAtMessageSizeLimit(t *testing.T) {
	config := &pluginmanager.ContextImp{}
	config.InitContext("test_project", "test_logstore", "test_configname")
	ctx := helper.NewObservePipelineContext(2)
	syslog := newSyslog()
	_, err := syslog.Init(config)
	require.NoError(t, err)
	require.NoError(t, syslog.StartService(ctx))
	t.Cleanup(func() { require.NoError(t, syslog.Stop()) })

	conn := connect(t, syslog)
	t.Cleanup(func() { require.NoError(t, conn.Close()) })
	require.NoError(t, conn.SetWriteDeadline(time.Now().Add(5*time.Second)))
	content := strings.Repeat("x", syslog.MaxMessageSize-len("\r\n"))
	payload := content + "\r\nnext\n"
	n, err := conn.Write([]byte(payload))
	require.NoError(t, err)
	require.Equal(t, len(payload), n)

	timer := time.NewTimer(5 * time.Second)
	defer timer.Stop()
	for _, want := range []string{content, "next"} {
		select {
		case group := <-ctx.Collector().Observe():
			require.Len(t, group.Events, 1)
			log := group.Events[0].(*models.Log)
			require.Equal(t, want, log.GetIndices().Get("_content_"))
		case <-timer.C:
			t.Fatal("timed out waiting for TCP message")
		}
	}
}
