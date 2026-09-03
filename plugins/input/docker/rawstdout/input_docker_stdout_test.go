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

package rawstdout

import (
	"io"
	"regexp"
	"testing"
	"time"

	"github.com/moby/moby/api/types/container"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestLogDriverSupported(t *testing.T) {
	tests := []struct {
		name       string
		hostConfig *container.HostConfig
		expected   bool
	}{
		{name: "nil host config", hostConfig: nil, expected: true},
		{name: "json file", hostConfig: hostConfigWithLogDriver("json-file"), expected: true},
		{name: "journald", hostConfig: hostConfigWithLogDriver("journald"), expected: true},
		{name: "unsupported", hostConfig: hostConfigWithLogDriver("syslog"), expected: false},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			assert.Equal(t, test.expected, logDriverSupported(container.InspectResponse{HostConfig: test.hostConfig}))
		})
	}
}

func TestStdoutCheckPointReturnsIsolatedCopy(t *testing.T) {
	checkpoint := &StdoutCheckPoint{
		checkpointMap: map[string]string{
			"container-a": "2026-09-02T01:02:03.000000000Z",
		},
	}

	copied := checkpoint.GetAllCheckPoint()
	copied["container-a"] = "changed"
	copied["container-b"] = "added"

	assert.Equal(t, "2026-09-02T01:02:03.000000000Z", checkpoint.GetCheckPoint("container-a"))
	assert.Empty(t, checkpoint.GetCheckPoint("container-b"))
}

func TestNewContainerPumpSingleLine(t *testing.T) {
	const input = "" +
		"2026-09-02T01:02:03.000000000Z first line\n" +
		"2026-09-02T01:02:04.000000000Z second line\n"

	for _, source := range []string{"stdout", "stderr"} {
		t.Run(source, func(t *testing.T) {
			logs := runContainerPump(t, source, input, nil)

			require.Len(t, logs, 2)
			assertPumpLog(t, logs[0], "2026-09-02T01:02:03.000000000Z", source, "first line")
			assertPumpLog(t, logs[1], "2026-09-02T01:02:04.000000000Z", source, "second line")
		})
	}
}

func TestNewContainerPumpMultiline(t *testing.T) {
	const timestamp = "2026-09-02T01:02:03.000000000Z"
	const input = "" +
		timestamp + " START first line\n" +
		timestamp + " continuation\n" +
		timestamp + " START second line\n"

	for _, source := range []string{"stdout", "stderr"} {
		t.Run(source, func(t *testing.T) {
			logs := runContainerPump(t, source, input, regexp.MustCompile(`^START.*$`))

			require.Len(t, logs, 2)
			assertPumpLog(t, logs[0], timestamp, source, "START first line\ncontinuation")
			assertPumpLog(t, logs[1], timestamp, source, "START second line")
		})
	}
}

func runContainerPump(t *testing.T, source, input string, beginLineRegex *regexp.Regexp) []*protocol.Log {
	t.Helper()
	collector := &helper.LocalCollector{}
	syner := &stdoutSyner{
		info: &containercenter.DockerInfoDetail{
			ContainerInfo: container.InspectResponse{
				ID:     "container-id",
				Name:   "container-name",
				Config: &container.Config{},
			},
		},
		context:              mock.NewEmptyContext("project", "logstore", "config"),
		beginLineReg:         beginLineRegex,
		beginLineTimeout:     time.Second,
		beginLineCheckLength: 1024,
		maxLogSize:           512 * 1024,
	}
	reader, writer := io.Pipe()
	if source == "stdout" {
		syner.newContainerPump(collector, reader, nil)
	} else {
		syner.newContainerPump(collector, nil, reader)
	}

	_, err := io.WriteString(writer, input)
	require.NoError(t, err)
	require.NoError(t, writer.Close())
	syner.wg.Wait()
	return collector.Logs
}

func assertPumpLog(t *testing.T, log *protocol.Log, timestamp, source, content string) {
	t.Helper()
	fields := make(map[string]string, len(log.Contents))
	for _, field := range log.Contents {
		fields[field.GetKey()] = field.GetValue()
	}
	assert.Equal(t, timestamp, fields["_time_"])
	assert.Equal(t, source, fields["_source_"])
	assert.Equal(t, content, fields["content"])
}

func hostConfigWithLogDriver(driver string) *container.HostConfig {
	return &container.HostConfig{
		LogConfig: container.LogConfig{Type: driver},
	}
}
