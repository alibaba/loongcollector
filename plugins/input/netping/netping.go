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
	"context"
	"crypto/rand"
	"fmt"
	"io/ioutil"
	"math"
	"math/big"
	"net"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	goping "github.com/go-ping/ping"
)

const (
	PingTypeIcmp    = "ping"
	PingTypeTcping  = "tcping"
	PingTypeHttping = "httping"

	DefaultIntervalSeconds = 60    // default interval
	MinIntervalSeconds     = 5     // min interval seconds
	MaxIntervalSeconds     = 86400 // max interval seconds
	DefaultTimeoutSeconds  = 5     // default timeout is 5s
	MinTimeoutSeconds      = 1     // min timeout seconds
	MaxTimeoutSeconds      = 30    // max timeout seconds
)

type Result struct {
	Valid   bool // if the result is meaningful for count
	Label   string
	Type    string
	Total   int
	Success int
	Failed  int

	// valid for icmping/tcping
	MinRTTMs    float64
	MaxRTTMs    float64
	AvgRTTMs    float64
	TotalRTTMs  float64
	StdDevRTTMs float64

	// valid for httping
	HTTPRTMs         int
	HTTPResponseSize int
	HasHTTPSCert     bool
	HTTPSCertLabels  string
	HTTPSCertTTLDay  int
}

type ResolveResult struct {
	Label   string
	Success bool
	RTMs    float64
}

type ICMPConfig struct {
	Src    string            `json:"src"`
	Target string            `json:"target"`
	Count  int               `json:"count"`
	Name   string            `json:"name"`
	Labels map[string]string `json:"labels"`
}

type TCPConfig struct {
	Src    string            `json:"src"`
	Target string            `json:"target"`
	Port   int               `json:"port"`
	Count  int               `json:"count"`
	Name   string            `json:"name"`
	Labels map[string]string `json:"labels"`
}

type HTTPConfig struct {
	Src                    string            `json:"src"`
	Method                 string            `json:"method"` // default is GET
	ExpectResponseContains string            `json:"expect_response_contains"`
	ExpectCode             int               `json:"expect_code"`
	Target                 string            `json:"target"`
	Name                   string            `json:"name"`
	Labels                 map[string]string `json:"labels"`
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
	HTTPConfigs     []HTTPConfig `json:"http" comment:"the http config list, example: {\"src\" : \"${IP_ADDR}\",  \"target\" : \"${http url}\"}"`
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
			if c.Name == "" {
				c.Name = fmt.Sprintf("%s -> %s", c.Src, c.Target)
			}
			localICMPConfigs = append(localICMPConfigs, c)
			m.hasConfig = true

			if !m.DisableDNS {
				m.resolveHostMap.Store(c.Target, "")
			}
		}
	}
	m.ICMPConfigs = localICMPConfigs

	// get tcp target
	localTCPConfigs := make([]TCPConfig, 0)
	for _, c := range m.TCPConfigs {
		if c.Src == m.ip {
			if c.Name == "" {
				c.Name = fmt.Sprintf("%s -> %s:%d", c.Src, c.Target, c.Port)
			}

			localTCPConfigs = append(localTCPConfigs, c)
			m.hasConfig = true
			if !m.DisableDNS {
				m.resolveHostMap.Store(c.Target, "")
			}
		}
	}
	m.TCPConfigs = localTCPConfigs

	// get http target
	localHTTPConfigs := make([]HTTPConfig, 0)
	for _, c := range m.HTTPConfigs {
		if c.Src == m.ip {
			if !strings.HasPrefix(c.Target, "http") {
				// add default schema
				c.Target = fmt.Sprintf("http://%s", c.Target)
			}
			u, err := url.Parse(c.Target)
			if err != nil {
				logger.Error(context.GetRuntimeContext(), "netping failed to parse httping target")
				continue
			}

			if u.Host == "" {
				logger.Error(context.GetRuntimeContext(), "netping failed to parse httping target, get empty host")
				continue
			}

			if c.Method == "" {
				c.Method = "GET"
			}

			if c.ExpectCode == 0 {
				c.ExpectCode = 200
			}

			if c.Name == "" {
				c.Name = fmt.Sprintf("%s -> %s://%s", c.Src, u.Scheme, u.Host)
			}

			localHTTPConfigs = append(localHTTPConfigs, c)
			m.hasConfig = true
			if !m.DisableDNS {
				realHost := strings.Split(u.Host, ":")
				m.resolveHostMap.Store(realHost[0], "")
			}
		}
	}
	m.HTTPConfigs = localHTTPConfigs

	m.icmpPrivileged = true

	m.resolveChannel = make(chan *ResolveResult, 100)
	m.resultChannel = make(chan *Result, 100)
	m.timeout = time.Duration(m.TimeoutSeconds) * time.Second
	logger.Info(context.GetRuntimeContext(),
		"netping init result, hasConfig: ", m.hasConfig, " localIP: ", m.ip,
		" timeout: ", m.timeout, " interval: ", m.IntervalSeconds)

	return m.IntervalSeconds * 1000, nil
}

func (m *NetPing) Description() string {
	return "a icmp-ping/tcp-ping/http-ping plugin for logtail"
}

// Collect is called every trigger interval to collect the metrics and send them to the collector.
func (m *NetPing) Collect(collector ilogtail.Collector) error {
	if !m.hasConfig {
		return nil
	}
	nowTs := time.Now()

	// for dns resolve
	if (len(m.ICMPConfigs) > 0 || len(m.TCPConfigs) > 0 || len(m.HTTPConfigs) > 0) && !m.DisableDNS {
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
			if result.Success {
				helper.AddMetric(collector, "dns_resolve_rt_ms", nowTs, result.Label, result.RTMs)
				helper.AddMetric(collector, "dns_resolve_success", nowTs, result.Label, 1)
				helper.AddMetric(collector, "dns_resolve_failed", nowTs, result.Label, 0)
			} else {
				helper.AddMetric(collector, "dns_resolve_success", nowTs, result.Label, 0)
				helper.AddMetric(collector, "dns_resolve_failed", nowTs, result.Label, 1)
			}
		}
	}

	counter := 0
	for i := range m.ICMPConfigs {
		go m.doICMPing(&m.ICMPConfigs[i])
		counter++
	}

	for i := range m.TCPConfigs {
		go m.doTCPing(&m.TCPConfigs[i])
		counter++
	}

	for i := range m.HTTPConfigs {
		go m.doHTTPing(&m.HTTPConfigs[i])
		counter++
	}
	if counter == 0 {
		// nothing to do
		return nil
	}

	for i := 0; i < counter; i++ {
		result := <-m.resultChannel

		if !result.Valid {
			continue
		}

		helper.AddMetric(collector, fmt.Sprintf("%s_total", result.Type), nowTs, result.Label, float64(result.Total))
		helper.AddMetric(collector, fmt.Sprintf("%s_success", result.Type), nowTs, result.Label, float64(result.Success))
		helper.AddMetric(collector, fmt.Sprintf("%s_failed", result.Type), nowTs, result.Label, float64(result.Failed))

		if (result.Type == PingTypeIcmp || result.Type == PingTypeTcping) && result.Success > 0 {
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_min_ms", result.Type), nowTs, result.Label, result.MinRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_max_ms", result.Type), nowTs, result.Label, result.MaxRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_avg_ms", result.Type), nowTs, result.Label, result.AvgRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_total_ms", result.Type), nowTs, result.Label, result.TotalRTTMs)
			helper.AddMetric(collector, fmt.Sprintf("%s_rtt_stddev_ms", result.Type), nowTs, result.Label, result.StdDevRTTMs)
		} else if result.Type == PingTypeHttping {
			if result.Success > 0 {
				helper.AddMetric(collector, fmt.Sprintf("%s_rt_ms", result.Type), nowTs, result.Label, float64(result.HTTPRTMs))
				helper.AddMetric(collector, fmt.Sprintf("%s_response_bytes", result.Type), nowTs, result.Label, float64(result.HTTPResponseSize))
			}

			if result.HasHTTPSCert {
				helper.AddMetric(collector, fmt.Sprintf("%s_cert_ttl_days", result.Type), nowTs, result.HTTPSCertLabels, float64(result.HTTPSCertTTLDay))
			}
		}
	}

	return nil
}

func (m *NetPing) evaluteDNSResolve(host string) {
	success := true
	start := time.Now()
	ips, resolveErr := net.LookupIP(host)
	rt := time.Since(start)
	if resolveErr != nil {
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
	label.Append("dns_name", host)
	label.Append("src", m.ip)
	label.Append("src_host", m.hostname)
	if !success {
		label.Append("err", resolveErr.Error())
	}

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

func (m *NetPing) doICMPing(config *ICMPConfig) {
	// prepare labels
	var label helper.KeyValues
	label.Append("name", config.Name)
	label.Append("src", config.Src)
	label.Append("dst", config.Target)
	label.Append("src_host", m.hostname)
	for k, v := range config.Labels {
		label.Append(k, v)
	}

	pinger, err := goping.NewPinger(m.getRealTarget(config.Target))
	if err != nil {
		logger.Error(m.context.GetRuntimeContext(), "FAIL_TO_INIT_PING", err.Error())
		label.Append("err", err.Error())
		m.resultChannel <- &Result{
			Valid:  true,
			Type:   PingTypeIcmp,
			Total:  config.Count,
			Failed: config.Count,
			Label:  label.String(),
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
		label.Append("err", err.Error())
		m.resultChannel <- &Result{
			Valid:  true,
			Type:   PingTypeIcmp,
			Total:  config.Count,
			Failed: config.Count,
			Label:  label.String(),
		}
		return
	}
	stats := pinger.Statistics() // get send/receive/duplicate/rtt stats

	var totalRtt time.Duration
	for _, rtt := range stats.Rtts {
		totalRtt += rtt
	}

	m.resultChannel <- &Result{
		Valid:       true,
		Label:       label.String(),
		Type:        PingTypeIcmp,
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

func (m *NetPing) doTCPing(config *TCPConfig) {
	// prepare labels
	var label helper.KeyValues
	label.Append("name", config.Name)
	label.Append("src", config.Src)
	label.Append("dst", config.Target)
	label.Append("port", fmt.Sprint(config.Port))
	label.Append("src_host", m.hostname)

	for k, v := range config.Labels {
		label.Append(k, v)
	}
	failed := 0
	var minRTT, maxRTT, totalRTT time.Duration

	rtts := []time.Duration{}

	target := m.getRealTarget(config.Target)
	var errorInfo error
	for i := 0; i < config.Count; i++ {
		rtt, err := evaluteTcping(target, config.Port, m.timeout)
		if err != nil {
			errorInfo = err
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

	if errorInfo != nil {
		label.Append("err", errorInfo.Error())
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

	m.resultChannel <- &Result{
		Valid:       true,
		Label:       label.String(),
		Type:        PingTypeTcping,
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

func (m *NetPing) doHTTPing(config *HTTPConfig) {
	// prepare labels
	var label helper.KeyValues
	label.Append("name", config.Name)
	label.Append("src", config.Src)
	label.Append("url", config.Target)
	label.Append("src_host", m.hostname)
	label.Append("method", config.Method)
	for k, v := range config.Labels {
		label.Append(k, v)
	}

	dialer := &net.Dialer{
		Timeout: m.timeout,
	}

	httpClient := http.Client{Transport: &http.Transport{
		DialContext: func(ctx context.Context, network, address string) (net.Conn, error) {
			host, port, err := net.SplitHostPort(address)
			if err != nil {
				return nil, err
			}
			target := m.getRealTarget(host)
			if target != host {
				conn, err := dialer.DialContext(ctx, network, target+":"+port)
				if err == nil {
					return conn, nil
				}
			}
			return dialer.DialContext(ctx, network, address)
		},
	}}
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*10)
	defer cancel()

	req, err := http.NewRequestWithContext(ctx, strings.ToUpper(config.Method), config.Target, nil)
	if err != nil {
		label.Append("err", err.Error())
		m.resultChannel <- &Result{
			Valid:   true,
			Type:    PingTypeHttping,
			Label:   label.String(),
			Total:   1,
			Success: 0,
			Failed:  1,
		}
		return
	}

	now := time.Now()
	resp, err := httpClient.Do(req)
	if err != nil {
		logger.Error(m.context.GetRuntimeContext(), "FAIL_TO_RUN_HTTPING", err.Error())
		label.Append("err", err.Error())
		m.resultChannel <- &Result{
			Valid:   true,
			Type:    PingTypeHttping,
			Label:   label.String(),
			Total:   1,
			Success: 0,
			Failed:  1,
		}
		return
	}
	rtMS := time.Since(now).Milliseconds()
	successCount := 1

	defer resp.Body.Close()
	respBody, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		m.resultChannel <- &Result{
			Valid:   true,
			Type:    PingTypeHttping,
			Label:   label.String(),
			Total:   1,
			Success: 0,
			Failed:  1,
		}
		return
	}

	// check status code
	if resp.StatusCode != config.ExpectCode {
		successCount = 0
	}

	// check response body
	if config.ExpectResponseContains != "" &&
		!strings.Contains(string(respBody), config.ExpectResponseContains) {
		successCount = 0
	}

	label.Append("proto", resp.Proto)
	label.Append("url_schema", resp.Request.URL.Scheme)
	label.Append("url_host", resp.Request.URL.Host)
	label.Append("code", fmt.Sprintf("%d", resp.StatusCode))
	label.Append("codex", fmt.Sprintf("%dxx", resp.StatusCode/100))

	var certTTLDay int
	var hasCert bool
	var certLabel helper.KeyValues

	if resp.TLS != nil {
		for _, v := range resp.TLS.PeerCertificates {
			if len(v.DNSNames) > 0 {
				// only check the leaf cert
				certLabel.Append("name", config.Name)
				certLabel.Append("src", config.Src)
				certLabel.Append("url", config.Target)
				certLabel.Append("src_host", m.hostname)
				certLabel.Append("url_host", resp.Request.URL.Host)
				certLabel.Append("subject_commmon_name", v.Subject.CommonName)
				certLabel.Append("issuer_commmon_name", v.Issuer.CommonName)

				certTTLDay = int(v.NotAfter.Sub(now).Hours() / 24)
				hasCert = true
				break
			}
		}
	}

	m.resultChannel <- &Result{
		Valid:            true,
		Type:             PingTypeHttping,
		Label:            label.String(),
		Total:            1,
		Success:          successCount,
		Failed:           1 - successCount,
		HTTPRTMs:         int(rtMS),
		HTTPResponseSize: len(respBody),

		HasHTTPSCert:    hasCert,
		HTTPSCertLabels: certLabel.String(),
		HTTPSCertTTLDay: certTTLDay,
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
