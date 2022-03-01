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
	"fmt"
	"math"
	"net"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	goping "github.com/go-ping/ping"
)

type Result struct {
	Valid    bool // if the result is meaningful for count
	Label    string
	Type     string
	Total    int
	Success  int
	Failed   int
	MinRTTMs float64
	MaxRTTMs float64
	AvgRTTMs float64
}

type ICMPConfig struct {
	Src    string `json:"src"`
	Target string `json:"target"`
	Count  int    `json:"count"`
}

type TCPConfig struct {
	Src    string `json:"src"`
	Target string `json:"target"`
	Port   int    `json:"port"`
	Count  int    `json:"count"`
}

// NetPing struct implements the MetricInput interface.
type NetPing struct {
	timeout         time.Duration
	icmpPrivileged  bool
	hasConfig       bool
	resultChannel   chan *Result
	context         ilogtail.Context
	IntervalSeconds int          `json:"interval_seconds" comment:"the interval of ping/tcping, unit is second,must large than 30"`
	ICMPConfigs     []ICMPConfig `json:"icmp" comment:"the icmping config list, example:  {\"src\" : \"${IP_ADDR}\",  \"target\" : \"${REMOTE_HOST}\", \"count\" : 3}"`
	TCPConfigs      []TCPConfig  `json:"tcp" comment:"the tcping config list, example: {\"src\" : \"${IP_ADDR}\",  \"target\" : \"${REMOTE_HOST}\", \"port\" : ${PORT}, \"count\" : 3}"`
}

const MinIntervalSecods = 30 // min inteval should large than 30s

// refer https://github.com/cloverstd/tcping/blob/master/ping/tcp.go
func evaluteTcping(target string, port int, timeout time.Duration) (time.Duration, error) {
	now := time.Now()
	conn, err := net.DialTimeout("tcp", fmt.Sprintf("%s:%d", target, port), timeout)
	if err != nil {
		return 0, err
	}
	err = conn.Close()
	if err != nil {
		return 0, err
	}

	return time.Since(now), nil
}

func (m *NetPing) Init(context ilogtail.Context) (int, error) {
	m.context = context
	if m.IntervalSeconds <= MinIntervalSecods {
		m.IntervalSeconds = MinIntervalSecods
	}

	localIP := util.GetIPAddress()

	m.hasConfig = false
	// get local icmp target
	localICMPConfigs := make([]ICMPConfig, 0)

	for _, c := range m.ICMPConfigs {
		if c.Src == localIP {
			localICMPConfigs = append(localICMPConfigs, c)
			m.hasConfig = true
		}
	}
	m.ICMPConfigs = localICMPConfigs

	// get tcp target
	localTCPConfigs := make([]TCPConfig, 0)
	for _, c := range m.TCPConfigs {
		if c.Src == localIP {
			localTCPConfigs = append(localTCPConfigs, c)
			m.hasConfig = true
		}
	}
	m.TCPConfigs = localTCPConfigs

	m.icmpPrivileged = true

	m.resultChannel = make(chan *Result, 100)
	m.timeout = time.Duration(m.IntervalSeconds / 2 * int(time.Second))
	return m.IntervalSeconds * 1000, nil
}

func (m *NetPing) Description() string {
	return "a icmp-ping/tcp-ping plugin for logtail"
}

// Collect is called every trigger interval to collect the metrics and send them to the collector.
func (m *NetPing) Collect(collector ilogtail.Collector) error {
	if !m.hasConfig {
		return nil
	}
	nowTs := time.Now()
	counter := 0
	if len(m.ICMPConfigs) > 0 {
		for _, config := range m.ICMPConfigs {
			go m.doICMPing(config)
			counter++
		}
	}

	if len(m.TCPConfigs) > 0 {
		for _, config := range m.TCPConfigs {
			go m.doTCPing(config)
			counter++
		}
	}
	if counter == 0 {
		// nothing to do
		return nil
	}

	for i := 0; i < counter; i++ {
		result := <-m.resultChannel
		helper.AddMetric(collector, fmt.Sprintf("%s_total", result.Type), nowTs, result.Label, float64(result.Total))
		helper.AddMetric(collector, fmt.Sprintf("%s_success", result.Type), nowTs, result.Label, float64(result.Success))
		helper.AddMetric(collector, fmt.Sprintf("%s_failed", result.Type), nowTs, result.Label, float64(result.Failed))
		if result.Success > 0 {
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_min", result.Type), nowTs, result.Label, result.MinRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_max", result.Type), nowTs, result.Label, result.MaxRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_avg", result.Type), nowTs, result.Label, result.AvgRTTMs)
		}
	}

	return nil
}

func (m *NetPing) doICMPing(config ICMPConfig) {
	pinger, err := goping.NewPinger(config.Target)
	if err != nil {
		logger.Error(m.context.GetRuntimeContext(), "FAIL_TO_INIT_PING", err.Error())
		m.resultChannel <- &Result{
			Valid:  true,
			Type:   "ping",
			Total:  config.Count,
			Failed: config.Count,
		}
		return
	}
	pinger.Timeout = m.timeout

	if m.icmpPrivileged {
		pinger.SetPrivileged(true)
	}
	pinger.Count = config.Count
	err = pinger.Run() // Blocks until finished or timeout.
	if err != nil {
		logger.Error(m.context.GetRuntimeContext(), "FAIL_TO_RUN_PING", err.Error())
		m.resultChannel <- &Result{
			Valid:  true,
			Type:   "ping",
			Total:  config.Count,
			Failed: config.Count,
		}
		return
	}
	stats := pinger.Statistics() // get send/receive/duplicate/rtt stats

	var label helper.KeyValues
	label.Append("src", config.Src)
	label.Append("dst", config.Target)

	m.resultChannel <- &Result{
		Valid:    true,
		Label:    label.String(),
		Type:     "ping",
		Total:    pinger.Count,
		Success:  stats.PacketsRecv,
		Failed:   pinger.Count - stats.PacketsRecv,
		MinRTTMs: float64(stats.MinRtt / time.Millisecond),
		MaxRTTMs: float64(stats.MaxRtt / time.Millisecond),
		AvgRTTMs: float64(stats.AvgRtt / time.Millisecond),
	}
}

func (m *NetPing) doTCPing(config TCPConfig) {
	success := 0
	failed := 0
	var minRTT, maxRTT, totalRTT time.Duration

	for i := 0; i < config.Count; i++ {
		rtt, err := evaluteTcping(config.Target, config.Port, m.timeout)
		if err != nil {
			failed++
			continue
		}
		if rtt > maxRTT {
			maxRTT = rtt
		}
		if rtt < minRTT || minRTT == 0 {
			minRTT = rtt
		}

		totalRTT += rtt
		success++
	}

	var avgRTTMs float64

	if success > 0 {
		avgRTTMs = math.Round(float64(totalRTT/time.Millisecond) / float64(success))
	}

	m.resultChannel <- &Result{
		Valid:    true,
		Type:     "tcping",
		Total:    config.Count,
		Success:  success,
		Failed:   failed,
		MinRTTMs: float64(minRTT / time.Millisecond),
		MaxRTTMs: float64(maxRTT / time.Millisecond),
		AvgRTTMs: avgRTTMs,
	}
}

// Register the plugin to the MetricInputs array.
func init() {
	ilogtail.MetricInputs["metric_input_netping"] = func() ilogtail.MetricInput {
		return &NetPing{
			// here you could set default value.
		}
	}
}
