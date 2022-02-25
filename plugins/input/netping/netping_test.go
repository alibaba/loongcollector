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
	"net"
	"testing"

	"github.com/agiledragon/gomonkey/v2"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestInitEmpty(t *testing.T) {

	ctx := mock.NewEmptyContext("project", "store", "config")
	config1 := `{
		"interval_seconds" : 5,
		"icmp" : [
		  {"src" : "1.1.1.1", "target" : "www.baidu.com", "count" : 3},
		  {"src" : "2.2.2.2", "target" : "www.baidu.com", "count" : 3}
		],
		"tcp" : [
		  {"src" : "1.1.1.1",  "target" : "www.baidu.com", "port" : 80, "count" : 3}
		]
	  }`

	netPing := &NetPing{}
	json.Unmarshal([]byte(config1), netPing)
	assert.Equal(t, 2, len(netPing.ICMPConfigs))
	assert.Equal(t, 1, len(netPing.TCPConfigs))

	funcPatch := gomonkey.ApplyFunc(getOutboudIP, func() (net.IP, error) {
		return net.IPv4(0, 0, 0, 0), nil
	})
	defer funcPatch.Reset()
	// 0 match
	netPing.Init(ctx)
	assert.Equal(t, 0, len(netPing.ICMPConfigs))
	assert.Equal(t, 0, len(netPing.TCPConfigs))

	c := &test.MockMetricCollector{}
	netPing.Collect(c)
}

func TestInitAndCollect(t *testing.T) {

	ctx := mock.NewEmptyContext("project", "store", "config")
	config1 := `{
		"interval_seconds" : 5,
		"icmp" : [
		  {"src" : "1.1.1.1", "target" : "www.baidu.com", "count" : 3},
		  {"src" : "2.2.2.2", "target" : "www.baidu.com", "count" : 3}
		],
		"tcp" : [
		  {"src" : "1.1.1.1",  "target" : "www.baidu.com", "port" : 80, "count" : 3}
		]
	  }`

	netPing := &NetPing{}
	json.Unmarshal([]byte(config1), netPing)
	assert.Equal(t, 2, len(netPing.ICMPConfigs))
	assert.Equal(t, 1, len(netPing.TCPConfigs))

	// 1 match
	funcPatch := gomonkey.ApplyFunc(getOutboudIP, func() (net.IP, error) {
		return net.IPv4(1, 1, 1, 1), nil
	})
	defer funcPatch.Reset()

	netPing.Init(ctx)
	assert.Equal(t, 1, len(netPing.ICMPConfigs))
	assert.Equal(t, 1, len(netPing.TCPConfigs))

	c := &test.MockMetricCollector{}
	netPing.IcmpPrivileged = false
	netPing.Collect(c)

	assert.Equal(t, 12, len(c.Logs))

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
	netPing.IcmpPrivileged = false

	ch := make(chan *Result, 100)
	config1 := ICMPConfig{
		Target: "www.baidu.com",
		Count:  3,
	}

	netPing.doICMPing(config1, ch)
	res1 := <-ch
	fmt.Println(res1)

	assert.Equal(t, "src#$#|dst#$#www.baidu.com", res1.Label)
	assert.Equal(t, true, res1.Valid)
	assert.Equal(t, 3, res1.Total)
	assert.Equal(t, 3, res1.Success)
	assert.Equal(t, 0, res1.Failed)

	// fail 1
	config2 := ICMPConfig{
		Target: "www.baidubaidubaidubaidubaidubaidubaidubaidubaidu.com",
		Count:  3,
	}

	netPing.doICMPing(config2, ch)
	res2 := <-ch
	fmt.Println(res2)

	assert.Equal(t, true, res2.Valid)
	assert.Equal(t, 3, res2.Total)
	assert.Equal(t, 0, res2.Success)
	assert.Equal(t, 3, res2.Failed)
}

func TestDoTCPing(t *testing.T) {

	cxt := mock.NewEmptyContext("project", "store", "config")
	netPing := ilogtail.MetricInputs["metric_input_netping"]().(*NetPing)
	_, err := netPing.Init(cxt)
	assert.NoError(t, err, "cannot init the mock process plugin: %v", err)

	ch := make(chan *Result, 100)
	config1 := TCPConfig{
		Target: "www.baidu.com",
		Port:   80,
		Count:  3,
	}

	go netPing.doTCPing(config1, ch)

	res1 := <-ch
	fmt.Println(res1)
	assert.Equal(t, true, res1.Valid)
	assert.Equal(t, 3, res1.Total)
	assert.Equal(t, 3, res1.Success)
	assert.Equal(t, 0, res1.Failed)

	config2 := TCPConfig{
		Target: "www.baidubaidubaidubaidubaidubaidubaidubaidubaidu.com",
		Port:   80,
		Count:  3,
	}

	go netPing.doTCPing(config2, ch)
	res2 := <-ch
	fmt.Println(res2)

	assert.Equal(t, true, res2.Valid)
	assert.Equal(t, 3, res2.Total)
	assert.Equal(t, 0, res2.Success)
	assert.Equal(t, 3, res2.Failed)
	assert.Equal(t, float64(0), res2.MaxRTTMs)
	assert.Equal(t, float64(0), res2.MinRTTMs)
	assert.Equal(t, float64(0), res2.AvgRTTMs)

	a := []int{0, 1}

	fmt.Print(a[0:2])
}
