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

package netping

import (
	"encoding/json"
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestInitEmpty(t *testing.T) {

	ctx := mock.NewEmptyContext("project", "store", "config")
	config1 := `{
		"interval_seconds" : 5,
		"icmp" : [
		  {"src" : "1.1.1.1", "target" : "8.8.8.8", "count" : 3},
		  {"src" : "2.2.2.2", "target" : "8.8.8.8", "count" : 3}
		],
		"tcp" : [
		  {"src" : "1.1.1.1",  "target" : "8.8.8.8", "port" : 80, "count" : 3}
		]
	  }`

	netPing := &NetPing{}
	json.Unmarshal([]byte(config1), netPing)
	assert.Equal(t, 2, len(netPing.ICMPConfigs))
	assert.Equal(t, 1, len(netPing.TCPConfigs))

	// 0 match
	netPing.Init(ctx)
	assert.Equal(t, 0, len(netPing.ICMPConfigs))
	assert.Equal(t, 0, len(netPing.TCPConfigs))

	c := &test.MockMetricCollector{}
	netPing.Collect(c)
}

func TestInitAndCollect(t *testing.T) {

	ctx := mock.NewEmptyContext("project", "store", "config")
	ip := util.GetIPAddress()
	config1 := fmt.Sprintf(`{
		"interval_seconds" : 5,
		"icmp" : [
		  {"src" : "%s", "target" : "8.8.8.8", "count" : 3},
		  {"src" : "2.2.2.2", "target" : "8.8.8.8", "count" : 3}
		],
		"tcp" : [
		  {"src" : "%s",  "target" : "www.baidu.com", "port" : 80, "count" : 3}
		]
	  }`, ip, ip)

	netPing := &NetPing{}
	json.Unmarshal([]byte(config1), netPing)
	assert.Equal(t, 2, len(netPing.ICMPConfigs))
	assert.Equal(t, 1, len(netPing.TCPConfigs))

	netPing.Init(ctx)
	assert.Equal(t, 1, len(netPing.ICMPConfigs))
	assert.Equal(t, 1, len(netPing.TCPConfigs))

	c := &test.MockMetricCollector{}
	netPing.icmpPrivileged = false
	netPing.Collect(c)

	hasTcping := false
	hasPing := false

	for _, log := range c.Logs {
		fmt.Println(log)
		for _, content := range log.Contents {
			if content.Key == "__name__" && content.Value == "tcping_total" {
				hasTcping = true
			} else if content.Key == "__name__" && content.Value == "ping_total" {
				hasPing = true
			}
		}
	}

	assert.Equal(t, true, hasTcping)
	assert.Equal(t, true, hasPing)
}

func TestDoICMPing(t *testing.T) {
	cxt := mock.NewEmptyContext("project", "store", "config")
	netPing := ilogtail.MetricInputs["metric_input_netping"]().(*NetPing)
	_, err := netPing.Init(cxt)
	assert.NoError(t, err, "cannot init the mock process plugin: %v", err)

	// sudo sysctl -w net.ipv4.ping_group_range="0 2147483647"
	netPing.icmpPrivileged = false

	config1 := ICMPConfig{
		Target: "8.8.8.8",
		Count:  3,
	}

	netPing.doICMPing(config1)
	res1 := <-netPing.resultChannel
	fmt.Println(res1)

	assert.Equal(t, "src#$#|dst#$#8.8.8.8", res1.Label)
	assert.Equal(t, true, res1.Valid)
	assert.Equal(t, 3, res1.Total)
	assert.Equal(t, 3, res1.Success+res1.Failed)

	// fail 1
	config2 := ICMPConfig{
		Target: "www.baidubaidubaidubaidubaidubaidubaidubaidubaidu.com",
		Count:  3,
	}

	netPing.doICMPing(config2)
	res2 := <-netPing.resultChannel
	fmt.Println(res2)

	assert.Equal(t, true, res2.Valid)
	assert.Equal(t, 3, res2.Total)
	assert.Equal(t, 3, res2.Success+res2.Failed)
}

func TestDoTCPing(t *testing.T) {

	cxt := mock.NewEmptyContext("project", "store", "config")
	netPing := ilogtail.MetricInputs["metric_input_netping"]().(*NetPing)
	_, err := netPing.Init(cxt)
	assert.NoError(t, err, "cannot init the mock process plugin: %v", err)

	config1 := TCPConfig{
		Target: "www.baidu.com",
		Port:   80,
		Count:  3,
	}

	go netPing.doTCPing(config1)

	res1 := <-netPing.resultChannel
	fmt.Println(res1)
	assert.Equal(t, true, res1.Valid)
	assert.Equal(t, 3, res1.Total)
	assert.Equal(t, 3, res1.Success+res1.Failed)

	config2 := TCPConfig{
		Target: "www.baidubaidubaidubaidubaidubaidubaidubaidubaidu.com",
		Port:   80,
		Count:  3,
	}

	go netPing.doTCPing(config2)
	res2 := <-netPing.resultChannel
	fmt.Println(res2)

	assert.Equal(t, true, res2.Valid)
	assert.Equal(t, 3, res2.Total)
	assert.Equal(t, 3, res2.Success+res2.Failed)
	assert.Equal(t, float64(0), res2.MaxRTTMs)
	assert.Equal(t, float64(0), res2.MinRTTMs)
	assert.Equal(t, float64(0), res2.AvgRTTMs)

	a := []int{0, 1}

	fmt.Print(a[0:2])
}

func TestProcessTimeoutAndInterval(t *testing.T) {
	ctx := mock.NewEmptyContext("project", "store", "config")

	// case 1
	config := `{
		"interval_seconds" : 60
	  }`

	netPing := &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, 60, netPing.IntervalSeconds)
	assert.Equal(t, DefaultTimeoutSeconds, netPing.TimeoutSeconds)

	// case 2
	config = `{
		"interval_seconds" : 0
	  }`

	netPing = &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, DefaultIntervalSeconds, netPing.IntervalSeconds)
	assert.Equal(t, DefaultTimeoutSeconds, netPing.TimeoutSeconds)

	// case 3
	config = `{
		"interval_seconds" : 86401
	  }`

	netPing = &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, DefaultIntervalSeconds, netPing.IntervalSeconds)
	assert.Equal(t, DefaultTimeoutSeconds, netPing.TimeoutSeconds)

	// case 4
	config = `{
		"timeout_seconds" : 30
	  }`

	netPing = &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, DefaultIntervalSeconds, netPing.IntervalSeconds)
	assert.Equal(t, 30, netPing.TimeoutSeconds)

	// case 5
	config = `{
			"timeout_seconds" : 0
		  }`

	netPing = &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, DefaultIntervalSeconds, netPing.IntervalSeconds)
	assert.Equal(t, DefaultTimeoutSeconds, netPing.TimeoutSeconds)

	// case 6
	config = `{
			"timeout_seconds" : -1
		  }`

	netPing = &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, DefaultIntervalSeconds, netPing.IntervalSeconds)
	assert.Equal(t, DefaultTimeoutSeconds, netPing.TimeoutSeconds)

	// case 7
	config = `{
			"timeout_seconds" : 86401
		  }`

	netPing = &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, DefaultIntervalSeconds, netPing.IntervalSeconds)
	assert.Equal(t, DefaultTimeoutSeconds, netPing.TimeoutSeconds)

	// case 8
	config = `{
			"timeout_seconds" : 300,
			"interval_seconds" : 100
		  }`

	netPing = &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, 100, netPing.IntervalSeconds)
	assert.Equal(t, DefaultTimeoutSeconds, netPing.TimeoutSeconds)

	// case 9
	config = `{
			"timeout_seconds" : 86401,
			"interval_seconds" : 86401
		  }`

	netPing = &NetPing{}
	json.Unmarshal([]byte(config), netPing)
	netPing.Init(ctx)
	assert.Equal(t, DefaultIntervalSeconds, netPing.IntervalSeconds)
	assert.Equal(t, DefaultTimeoutSeconds, netPing.TimeoutSeconds)
}
