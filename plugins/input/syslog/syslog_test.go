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
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pluginmanager"

	"fmt"
	"math/rand"
	"net"
	"os"
	"strconv"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestStartAndStop(t *testing.T) {
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("test_project", "test_logstore", "test_configname")

	syslog := newSyslog()
	collector := &mockCollector{}
	_, _ = syslog.Init(ctx)
	go func() {
		err := syslog.Start(collector)
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

type mockLog struct {
	tags    map[string]string
	fields  map[string]string
	t       time.Time
	isValid bool
}

type mockCollector struct {
	rawLogs []string
	logs    []*mockLog
	lock    sync.Mutex
}

func (c *mockCollector) AddData(
	tags map[string]string, fields map[string]string, t ...time.Time) {
	c.AddDataWithContext(tags, fields, nil, t...)
}

func (c *mockCollector) AddDataArray(
	tags map[string]string, columns []string, values []string, t ...time.Time) {
	c.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (c *mockCollector) AddRawLog(log *protocol.Log) {
	c.AddRawLogWithContext(log, nil)
}

func (c *mockCollector) AddDataWithContext(tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	c.lock.Lock()
	defer c.lock.Unlock()
	c.logs = append(c.logs, &mockLog{tags, fields, t[0], true})
}

func (c *mockCollector) AddDataArrayWithContext(
	tags map[string]string, columns []string, values []string, ctx map[string]interface{}, t ...time.Time) {
}

func (c *mockCollector) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {

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
func mockRun(t *testing.T, syslog *Syslog, collector *mockCollector) {
	rand.Seed(time.Now().UnixNano())

	reconnectCount := 0
	totalLogCount := 0

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
			collector.rawLogs = append(collector.rawLogs, log)
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

	require.Equal(t, len(collector.rawLogs), len(collector.logs))
	for idx, rawLog := range collector.rawLogs {
		slog := collector.logs[idx]
		priority, _ := strconv.Atoi(slog.fields["_priority_"])
		log := getLog(priority,
			slog.fields["_hostname_"], slog.fields["_program_"], slog.fields["_content_"], &slog.t)
		if t.Name() != "TestMockUnixGram" {
			require.Equal(t, "127.0.0.1", slog.fields["_client_ip_"])
		}
		require.Equal(t, rawLog, log, "log index: %v, slog: %v, raw log: %v", idx, slog, rawLog)
	}

	t.Logf("Total log count: %v, reconnect count: %v\n", totalLogCount, reconnectCount)

}

func TestMockTcpRun(t *testing.T) {
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("test_project", "test_logstore", "test_configname")
	collector := &mockCollector{}

	syslog := newSyslog()
	syslog.ParseProtocol = "rfc3164"
	syslog.Address = "tcp://127.0.0.1:0"
	_, _ = syslog.Init(ctx)
	go func() {
		err := syslog.Start(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)

	mockRun(t, syslog, collector)
}

func TestMockUdpRun(t *testing.T) {
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("test_project", "test_logstore", "test_configname")
	collector := &mockCollector{}

	syslog := newSyslog()
	syslog.ParseProtocol = "rfc3164"
	syslog.Address = "udp://127.0.0.1:0"
	_, _ = syslog.Init(ctx)
	go func() {
		err := syslog.Start(collector)
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
	collector := &mockCollector{}

	syslog := newSyslog()
	syslog.ParseProtocol = "rfc3164"
	syslog.Address = "unixgram://" + unixFilePath
	_, _ = syslog.Init(ctx)
	go func() {
		err := syslog.Start(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)

	mockRun(t, syslog, collector)
}
