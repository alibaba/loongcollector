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

package test

import (
	"context"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pluginmanager"
	_ "github.com/alibaba/ilogtail/plugins/aggregator"
	_ "github.com/alibaba/ilogtail/plugins/flusher/checker"
	_ "github.com/alibaba/ilogtail/plugins/flusher/statistics"
	_ "github.com/alibaba/ilogtail/plugins/flusher/stdout"
)

func GetTestConfig(configName string) string {
	fileName := "./" + configName + ".json"
	byteStr, err := os.ReadFile(filepath.Clean(fileName))
	if err != nil {
		logger.Warning(context.Background(), selfmonitor.ReadFileAlarm, fileName, "error", err)
	}
	return string(byteStr)
}

func LoadDefaultConfig() *pluginmanager.LogstoreConfig {
	return LoadAndStartMockConfig("project", "logstore", "configName", GetTestConfig("config"))
}

// project, logstore, config, jsonStr
func LoadAndStartMockConfig(project, logstore, configName, jsonStr string) *pluginmanager.LogstoreConfig {
	err := pluginmanager.LoadLogstoreConfig(project, logstore, configName, 666, jsonStr)
	if err != nil {
		panic(err)
	}
	if err := pluginmanager.Start(configName); err != nil {
		panic(err)
	}
	object := pluginmanager.LogtailConfig[configName]
	return object
}

func PluginStart() error {
	return pluginmanager.Start("")
}

func PluginStop() error {
	if err := pluginmanager.StopAllPipelines(true); err != nil {
		return err
	}
	if err := pluginmanager.StopAllPipelines(false); err != nil {
		return err
	}
	return nil
}

func CreateLogs(kvs ...string) *protocol.Log {
	var slsLog protocol.Log
	for i := 0; i < len(kvs)-1; i += 2 {
		cont := &protocol.Log_Content{Key: kvs[i], Value: kvs[i+1]}
		slsLog.Contents = append(slsLog.Contents, cont)
	}
	nowTime := time.Now()
	protocol.SetLogTime(&slsLog, uint32(nowTime.Unix()))
	return &slsLog
}

func CreateLogByFields(fields map[string]string) *protocol.Log {
	var slsLog protocol.Log
	for key, val := range fields {
		cont := &protocol.Log_Content{Key: key, Value: val}
		slsLog.Contents = append(slsLog.Contents, cont)
	}
	nowTime := time.Now()
	protocol.SetLogTime(&slsLog, uint32(nowTime.Unix()))
	return &slsLog
}

type portpair struct {
	first string
	last  string
}

// GetAvailableLocalAddress finds an available local port and returns an endpoint
// describing it. The port is available for opening when this function returns
// provided that there is no race by some other code to grab the same port
// immediately.
func GetAvailableLocalAddress(t testing.TB) string {
	// Retry has been added for windows as net.Listen can return a port that is not actually available. Details can be
	// found in https://github.com/docker/for-win/issues/3171 but to summarize Hyper-V will reserve ranges of ports
	// which do not show up under the "netstat -ano" but can only be found by
	// "netsh interface ipv4 show excludedportrange protocol=tcp".  We'll use []exclusions to hold those ranges and
	// retry if the port returned by GetAvailableLocalAddress falls in one of those them.
	var exclusions []portpair
	portFound := false
	if runtime.GOOS == "windows" {
		exclusions = getExclusionsList(t)
	}

	var endpoint string
	for !portFound {
		endpoint = findAvailableAddress(t)
		_, port, err := net.SplitHostPort(endpoint)
		require.NoError(t, err)
		portFound = true
		if runtime.GOOS == "windows" {
			for _, pair := range exclusions {
				if port >= pair.first && port <= pair.last {
					portFound = false
					break
				}
			}
		}
	}

	return endpoint
}

func findAvailableAddress(t testing.TB) string {
	ln, err := net.Listen("tcp", "localhost:0")
	require.NoError(t, err, "Failed to get a free local port")
	// There is a possible race if something else takes this same port before
	// the test uses it, however, that is unlikely in practice.
	defer func() {
		assert.NoError(t, ln.Close())
	}()
	return ln.Addr().String()
}

// Get excluded ports on Windows from the command: netsh interface ipv4 show excludedportrange protocol=tcp
func getExclusionsList(t testing.TB) []portpair {
	cmdTCP := exec.Command("netsh", "interface", "ipv4", "show", "excludedportrange", "protocol=tcp")
	outputTCP, errTCP := cmdTCP.CombinedOutput()
	require.NoError(t, errTCP)
	exclusions := createExclusionsList(string(outputTCP), t)

	cmdUDP := exec.Command("netsh", "interface", "ipv4", "show", "excludedportrange", "protocol=udp")
	outputUDP, errUDP := cmdUDP.CombinedOutput()
	require.NoError(t, errUDP)
	exclusions = append(exclusions, createExclusionsList(string(outputUDP), t)...)

	return exclusions
}

func createExclusionsList(exclusionsText string, t testing.TB) []portpair {
	var exclusions []portpair

	parts := strings.Split(exclusionsText, "--------")
	require.Equal(t, len(parts), 3)
	portsText := strings.Split(parts[2], "*")
	require.Greater(t, len(portsText), 1) // original text may have a suffix like " - Administered port exclusions."
	lines := strings.Split(portsText[0], "\n")
	for _, line := range lines {
		if strings.TrimSpace(line) != "" {
			entries := strings.Fields(strings.TrimSpace(line))
			require.Equal(t, len(entries), 2)
			pair := portpair{entries[0], entries[1]}
			exclusions = append(exclusions, pair)
		}
	}
	return exclusions
}

// ReadLogVal returns the log content value for the input key, and returns empty string when not found.
func ReadLogVal(log *protocol.Log, key string) string {
	for _, content := range log.Contents {
		if content.Key == key {
			return content.Value
		}
	}
	return ""
}

// PickLogs select some of original logs to new res logs by the specific pickKey and pickVal.
func PickLogs(logs []*protocol.Log, pickKey string, pickVal string) (res []*protocol.Log) {
	for _, log := range logs {
		if ReadLogVal(log, pickKey) == pickVal {
			res = append(res, log)
		}
	}
	return res
}

// PickEvent select one of original events by the specific event name.
func PickEvent(events []models.PipelineEvent, name string) models.PipelineEvent {
	for _, event := range events {
		if event.GetName() == name {
			return event
		}
	}
	return nil

}
