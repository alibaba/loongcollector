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
	"crypto/rand"
	"fmt"
	"math"
	"math/big"
	"net"
	"sync"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	goping "github.com/go-ping/ping"
)

type Result struct {
	Valid       bool // if the result is meaningful for count
	Label       string
	Type        string
	Total       int
	Success     int
	Failed      int
	MinRTTMs    float64
	MaxRTTMs    float64
	AvgRTTMs    float64
	TotalRTTMs  float64
	StdDevRTTMs float64
}

type ResolveResult struct {
	Label   string
	Success bool
	RTMs    float64
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
	hostname        string
	ip              string
	resolveHostMap  *sync.Map
	resolveChannel  chan *ResolveResult
	DisableDNS      bool         `json:"disable_dns_metric" comment:"disable dns resolve metric, default is false"`
	TimeoutSeconds  int          `json:"timeout_seconds" comment:"the timeout of ping/tcping, unit is second,must large than or equal 1, less than  30, default is 5"`
	IntervalSeconds int          `json:"interval_seconds" comment:"the interval of ping/tcping, unit is second,must large than or equal 5, less than 86400 and timeout_seconds, default is 60"`
	ICMPConfigs     []ICMPConfig `json:"icmp" comment:"the icmping config list, example:  {\"src\" : \"${IP_ADDR}\",  \"target\" : \"${REMOTE_HOST}\", \"count\" : 3}"`
	TCPConfigs      []TCPConfig  `json:"tcp" comment:"the tcping config list, example: {\"src\" : \"${IP_ADDR}\",  \"target\" : \"${REMOTE_HOST}\", \"port\" : ${PORT}, \"count\" : 3}"`
}

const (
	DefaultIntervalSeconds = 60    // default interval
	MinIntervalSeconds     = 5     // min interval seconds
	MaxIntervalSeconds     = 86400 // max interval seconds
	DefaultTimeoutSeconds  = 5     // default timeout is 5s
	MinTimeoutSeconds      = 1     // min timeout seconds
	MaxTimeoutSeconds      = 30    // max timeout seconds
)

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

func (m *NetPing) processTimeoutAndInterval() {

	if m.IntervalSeconds <= MinIntervalSeconds {
		m.IntervalSeconds = DefaultIntervalSeconds
	} else if m.IntervalSeconds > MaxIntervalSeconds {
		m.IntervalSeconds = DefaultIntervalSeconds
	}

	if m.TimeoutSeconds <= MinTimeoutSeconds {
		m.TimeoutSeconds = DefaultTimeoutSeconds
	} else if m.TimeoutSeconds > MaxTimeoutSeconds {
		m.TimeoutSeconds = DefaultTimeoutSeconds
	}

	if m.TimeoutSeconds > m.IntervalSeconds {
		m.TimeoutSeconds = DefaultTimeoutSeconds
	}
}

func (m *NetPing) Init(context ilogtail.Context) (int, error) {
	logger.Info(context.GetRuntimeContext(), "netping init")
	m.context = context

	m.hostname = util.GetHostName()

	m.processTimeoutAndInterval()

	m.ip = util.GetIPAddress()

	m.hasConfig = false
	// get local icmp target
	localICMPConfigs := make([]ICMPConfig, 0)

	m.resolveHostMap = &sync.Map{}

	for _, c := range m.ICMPConfigs {
		if c.Src == m.ip {
			localICMPConfigs = append(localICMPConfigs, c)
			m.hasConfig = true
		}
	}
	m.ICMPConfigs = localICMPConfigs

	// get tcp target
	localTCPConfigs := make([]TCPConfig, 0)
	for _, c := range m.TCPConfigs {
		if c.Src == m.ip {
			localTCPConfigs = append(localTCPConfigs, c)
			m.hasConfig = true
		}
	}
	m.TCPConfigs = localTCPConfigs

	m.icmpPrivileged = true

	m.resolveChannel = make(chan *ResolveResult, 100)
	m.resultChannel = make(chan *Result, 100)
	m.timeout = time.Duration(m.TimeoutSeconds) * time.Second
	logger.Info(context.GetRuntimeContext(),
		"netping init result, hasConfig: ", m.hasConfig, " localIP: ", m.ip, " timeout: ", m.timeout, " interval: ", m.IntervalSeconds)

	return m.IntervalSeconds * 1000, nil
}

func (m *NetPing) Description() string {
	return "a icmp-ping/tcp-ping plugin for logtail"
}

func (m *NetPing) evaluteDNSResolve(host string) {
	success := true
	start := time.Now()
	ips, err := net.LookupIP(host)
	rt := time.Since(start)
	if err != nil {
		success = false
		m.resolveHostMap.Store(host, "")
	} else {
		var n int64
		nBig, err := rand.Int(rand.Reader, big.NewInt(int64(len(ips))))
		if err == nil {
			n = nBig.Int64()
		}
		m.resolveHostMap.Store(host, ips[n].String())
	}

	var label helper.KeyValues
	label.Append("target", host)
	label.Append("src", m.ip)
	label.Append("src_host", m.hostname)

	m.resolveChannel <- &ResolveResult{
		Success: success,
		RTMs:    float64(rt.Milliseconds()),
		Label:   label.String(),
	}
}

func (m *NetPing) getRealTarget(target string) string {
	realTarget := ""
	v, exists := m.resolveHostMap.Load(target)
	if exists {
		realTarget = v.(string)
	}
	if realTarget == "" {
		realTarget = target
	}
	return realTarget
}

// Collect is called every trigger interval to collect the metrics and send them to the collector.
func (m *NetPing) Collect(collector ilogtail.Collector) error {
	if !m.hasConfig {
		return nil
	}
	nowTs := time.Now()

	// for dns resolve
	if (len(m.ICMPConfigs) > 0 || len(m.TCPConfigs) > 0) && !m.DisableDNS {
		resolveCounter := 0
		m.resolveHostMap.Range(
			func(key, value interface{}) bool {
				host := key.(string)
				ip := net.ParseIP(host)
				if ip == nil {
					go m.evaluteDNSResolve(host)
					resolveCounter++
				}
				return true
			})

		for i := 0; i < resolveCounter; i++ {
			result := <-m.resolveChannel
			successCount := 0
			if result.Success {
				successCount++
				helper.AddMetric(collector, "dns_resolve_rt_ms", nowTs, result.Label, result.RTMs)
			}
			helper.AddMetric(collector, "dns_resolve_success", nowTs, result.Label, float64(successCount))
		}
	}

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
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_min_ms", result.Type), nowTs, result.Label, result.MinRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_max_ms", result.Type), nowTs, result.Label, result.MaxRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_avg_ms", result.Type), nowTs, result.Label, result.AvgRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_total_ms", result.Type), nowTs, result.Label, result.TotalRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_stddev_ms", result.Type), nowTs, result.Label, result.StdDevRTTMs)
		}
	}

	return nil
}

func (m *NetPing) doICMPing(config ICMPConfig) {
	pinger, err := goping.NewPinger(m.getRealTarget(config.Target))
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
	label.Append("src_host", m.hostname)

	var totalRtt time.Duration
	for _, rtt := range stats.Rtts {
		totalRtt += rtt
	}

	m.resultChannel <- &Result{
		Valid:       true,
		Label:       label.String(),
		Type:        "ping",
		Total:       pinger.Count,
		Success:     stats.PacketsRecv,
		Failed:      pinger.Count - stats.PacketsRecv,
		MinRTTMs:    float64(stats.MinRtt / time.Millisecond),
		MaxRTTMs:    float64(stats.MaxRtt / time.Millisecond),
		AvgRTTMs:    float64(stats.AvgRtt / time.Millisecond),
		TotalRTTMs:  float64(totalRtt / time.Millisecond),
		StdDevRTTMs: float64(stats.StdDevRtt / time.Millisecond),
	}
}

func (m *NetPing) doTCPing(config TCPConfig) {

	failed := 0
	var minRTT, maxRTT, totalRTT time.Duration

	rtts := []time.Duration{}

	target := m.getRealTarget(config.Target)
	for i := 0; i < config.Count; i++ {
		rtt, err := evaluteTcping(target, config.Port, m.timeout)
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
		rtts = append(rtts, rtt)
	}

	var avgRTT float64
	var stdDevRtt float64
	if len(rtts) > 0 {
		avgRTT = math.Round(float64(totalRTT) / float64(len(rtts)))

		var sd float64
		for _, rtt := range rtts {
			sd += math.Pow(float64(rtt)-avgRTT, 2)
		}
		stdDevRtt = math.Round(math.Sqrt(sd / float64(len(rtts))))

	}

	var label helper.KeyValues
	label.Append("src", config.Src)
	label.Append("dst", config.Target)
	label.Append("port", fmt.Sprint(config.Port))
	label.Append("src_host", m.hostname)

	m.resultChannel <- &Result{
		Valid:       true,
		Label:       label.String(),
		Type:        "tcping",
		Total:       config.Count,
		Success:     len(rtts),
		Failed:      failed,
		MinRTTMs:    float64(minRTT / time.Millisecond),
		MaxRTTMs:    float64(maxRTT / time.Millisecond),
		AvgRTTMs:    avgRTT / float64(time.Millisecond),
		TotalRTTMs:  float64(totalRTT / time.Millisecond),
		StdDevRTTMs: stdDevRtt / float64(time.Millisecond),
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
