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

package grok

import (
	"encoding/binary"
	"math/rand"
	"net"
	"regexp"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/dlclark/regexp2"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type MockParam struct {
	name         string
	num          int
	separator    string
	separatorReg string
	mockFunc1    func(int, string, string) (string, string)
	mockFunc2    func(int, string, string) []*protocol.Log
}

/*
	Compare regexp2 with regexp
*/

var (
	expression  = "((\\{.*?\\})?((cpu_load.*\\.avg\\.60)|(cpu_util.*\\.avg\\.60)|(memory_free.*\\.avg\\.60)|(memory_usage.*\\.avg\\.60)|(periodictask:.+)|(thrift\\..*dropped_conns\\.count\\.60)|(thrift\\..*killed_tasks\\.count\\.60)|(thrift\\..*num_calls\\.sum\\.60)|(thrift\\..*num_exceptions\\.sum\\.60)|(thrift\\..*num_processed\\.sum\\.60)|(thrift\\..*process_delay\\.p99\\.60)|(thrift\\..*process_time\\.p99\\.60)|(thrift\\..*queue_timeouts\\.count\\.60)|(thrift\\..*rejected_conns\\.count\\.60)|(thrift\\..*server_overloaded\\.count\\.60)|(thrift\\..*time_process_us\\.avg\\.60)|(thrift\\..*timeout_tasks\\.count\\.60)|(thrift\\.accepted_connections\\.count\\.60)|(thrift\\.active_requests\\.avg\\.60)|(thrift\\.adaptive_ideal_rtt_us)|(thrift\\.adaptive_min_concurrency)|(thrift\\.adaptive_sampled_rtt_us)|(thrift\\.admission_control\\..*)|(thrift\\.eventbase_busy_time\\.p99.60)|(thrift\\.eventbase_idle_time\\.p99.60)|(thrift\\.max_requests)|(thrift\\.num_active_requests)|(thrift\\.num_busy_pool_workers)|(thrift\\.num_idle_pool_workers)|(thrift\\.queued_requests)|(thrift\\.queued_requests\\.avg\\.60)|(thrift\\.queuelag.*\\.sum\\.60)|(thrift\\.queues\\..*\\.60)|(thrift\\.received_requests\\.count\\.60)|(thrift\\.server_load)))"
	testStrings = []string{
		"system.cpu-idle",
		"system.cpu-iowait",
		"system.cpu-nice",
		"system.cpu-sys",
		"system.cpu-softirq",
		"system.cpu-hardirq",
		"system.cpu-user",
		"system.cpu-busy-pct",
		"system.cpu-util-pct",
		"system.load-1",
		"system.load-5",
		"system.uptime",
		"system.mem_free",
		"system.mem_free_nobuffer_nocache",
		"system.mem_used",
		"system.mem-util-pct",
		"system.mem_slab",
		"system.mem_anon",
		"system.mem_kernel",
		"system.mem_total",
		"system.n_eth-rxbyt_s",
		"system.n_eth-txbyt_s",
		"system.n_eth-util-pct",
		"system.n_eth-rxpck_s",
		"system.n_eth-txpck_s",
		"system.n_eth-rxerr_s",
		"system.n_eth-txerr_s",
		"system.rx_errors",
		"system.tx_errors",
		"system.net.tcp.attempt_fails_per_s",
		"system.net.tcp.estab_rsts_per_s",
		"system.net.tcp.rsts_per_s",
		"system.net.tcp.rxmits_per_s",
		"system.net.tcp.socket_count",
		"system.net.tcp.socket_time_wait_count",
		"system.net.tcp6.socket_count",
		"system.net.udp.error_drop_per_s",
		"system.connection_count_estab",
		"system.connection_count_total",
		"system.swap_free",
		"system.swap_used",
		"system.swap_pagein_per_s",
		"system.swap_pageout_per_s",
	}
)

var params1 = []MockParam{
	{"original", 10, " ", "\\s", mockData1, nil},
	{"new", 10, "\n", "\n", mockDataSingleLine1, nil},
	{"original", 100, " ", "\\s", mockData1, nil},
	{"new", 100, "\n", "\n", mockDataSingleLine1, nil},
	{"original", 1000, " ", "\\s", mockData1, nil},
	{"new", 1000, "\n", "\n", mockDataSingleLine1, nil},
}

func mockData1(num int, separator, separatorReg string) (string, string) {
	line := strings.Join(testStrings, " ")
	reg := expression
	var res, pattern string
	for i := 0; i < num; i++ {
		res += line
		pattern += reg
		if i != num-1 {
			res += separator
			pattern += separatorReg
		}
	}
	return pattern, res
}

func mockDataSingleLine1(num int, separator, separatorReg string) (string, string) {
	pattern, log := mockData1(num, separator, separatorReg)
	return "(?s)" + pattern, log
}

// goos: linux
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/plugins/processor/grok
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkRegexpTest/original10                    132	   8299248 ns/op	    8124 B/op	       1 allocs/op
// BenchmarkRegexpTest/new10                         133	   8113802 ns/op	    8061 B/op	       1 allocs/op
// BenchmarkRegexpTest/original100                    13	  87498876 ns/op	  665907 B/op	      11 allocs/op
// BenchmarkRegexpTest/new100                         13	  82544698 ns/op	  665840 B/op	      11 allocs/op
// BenchmarkRegexpTest/original1000                    2	 825507343 ns/op	41835420 B/op	      75 allocs/op
// BenchmarkRegexpTest/new1000                         2	 908423302 ns/op	41835420 B/op	      75 allocs/op

func BenchmarkRegexpTest(b *testing.B) {
	for _, param := range params1 {
		b.Run(param.name+strconv.Itoa(param.num), func(b *testing.B) {
			pattern, log := param.mockFunc1(param.num, param.separator, param.separatorReg)
			reg := regexp.MustCompile(pattern)
			b.ReportAllocs()

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				reg.MatchString(log)
			}
			b.StopTimer()
		})
	}
}

// goos: linux
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/plugins/processor/grok
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkRegexp2Test/original10               258	   4511419 ns/op	   41901 B/op	       1 allocs/op
// BenchmarkRegexp2Test/new10                    254	   4608836 ns/op	   41915 B/op	       1 allocs/op
// BenchmarkRegexp2Test/original100               27	  44154612 ns/op	  461814 B/op	       1 allocs/op
// BenchmarkRegexp2Test/new100                    26	  45141461 ns/op	  465082 B/op	       1 allocs/op
// BenchmarkRegexp2Test/original1000               3	 457185115 ns/op	11299752 B/op	       4 allocs/op
// BenchmarkRegexp2Test/new1000	                   3	 431996927 ns/op	11299754 B/op	       4 allocs/op

func BenchmarkRegexp2Test(b *testing.B) {
	for _, param := range params1 {
		b.Run(param.name+strconv.Itoa(param.num), func(b *testing.B) {
			pattern, log := param.mockFunc1(param.num, param.separator, param.separatorReg)
			reg := regexp2.MustCompile(pattern, regexp2.RE2)
			b.ReportAllocs()

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				reg.MatchString(log)
			}
			b.StopTimer()
		})
	}
}

/*
	Test multiple matchs performance
*/

type IPv4Int uint32

func (i IPv4Int) ip() net.IP {
	ip := make(net.IP, net.IPv6len)
	copy(ip, net.IPv4zero)
	binary.BigEndian.PutUint32(ip.To4(), uint32(i))
	return ip.To16()
}

func RandomIpv4Int() uint32 {
	return rand.New(rand.NewSource(time.Now().UnixNano())).Uint32()
}

var params2 = []MockParam{
	{"original", 10, " ", "\\s", nil, mockData2},
	{"original", 100, " ", "\\s", nil, mockData2},
	{"original", 1000, " ", "\\s", nil, mockData2},
	{"original", 10000, " ", "\\s", nil, mockData2},
}

func mockData2(num int, separator, separatorReg string) []*protocol.Log {
	Logs := []*protocol.Log{}

	for i := 0; i < num; i++ {
		log := new(protocol.Log)
		log.Contents = make([]*protocol.Log_Content, 1)
		log.Contents[0] = &protocol.Log_Content{Key: "content", Value: IPv4Int(RandomIpv4Int()).ip().String()}
		Logs = append(Logs, log)
	}
	return Logs
}

// goos: linux
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/plugins/processor/grok
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkGrokMatchOneTest/original10                10000	    159030 ns/op	    8822 B/op	     150 allocs/op
// BenchmarkGrokMatchOneTest/original100                4135	   2668691 ns/op	   87894 B/op	    1500 allocs/op
// BenchmarkGrokMatchOneTest/original1000                454	   3532302 ns/op	  865847 B/op	   15019 allocs/op
// BenchmarkGrokMatchOneTest/original10000                57	  23169730 ns/op	 8656781 B/op	  151052 allocs/op
func BenchmarkGrokMatchOneTest(b *testing.B) {
	for _, param := range params2 {
		b.Run(param.name+strconv.Itoa(param.num), func(b *testing.B) {
			Logs := param.mockFunc2(param.num, param.separator, param.separatorReg)
			processor := pipeline.Processors["processor_grok"]().(*ProcessorGrok)
			processor.Match = []string{
				"%{IPV4:ipv4}",
			}
			processor.Init(mock.NewEmptyContext("p", "l", "c"))

			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				processor.ProcessLogs(Logs)
			}
			b.StopTimer()
			for _, log := range Logs {
				if log.Contents[1].Key != "ipv4" {
					b.Log("Parse error")
				}
			}
		})
	}
}

// goos: linux
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/plugins/processor/grok
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkGrokMatchTwoTest/original10                10000	    180911 ns/op	   15934 B/op	     250 allocs/op
// BenchmarkGrokMatchTwoTest/original100                2132	   1203559 ns/op	  159395 B/op	    2500 allocs/op
// BenchmarkGrokMatchTwoTest/original1000                252	   5713273 ns/op	 1580173 B/op	   25031 allocs/op
// BenchmarkGrokMatchTwoTest/original10000                25	  54394004 ns/op	15839277 B/op	  252001 allocs/op
func BenchmarkGrokMatchTwoTest(b *testing.B) {
	for _, param := range params2 {
		b.Run(param.name+strconv.Itoa(param.num), func(b *testing.B) {
			Logs := param.mockFunc2(param.num, param.separator, param.separatorReg)
			processor := pipeline.Processors["processor_grok"]().(*ProcessorGrok)
			processor.Match = []string{
				"%{BASE10NUM}",
				"%{IPV4:ipv4}",
			}
			processor.Init(mock.NewEmptyContext("p", "l", "c"))

			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				processor.ProcessLogs(Logs)
			}
			b.StopTimer()
			for _, log := range Logs {
				if log.Contents[1].Key != "ipv4" {
					b.Log("Parse error")
				}
			}
		})
	}
}

// goos: linux
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/plugins/processor/grok
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkGrokMatchThreeTest/original10              10000	    216708 ns/op	   23199 B/op	     350 allocs/op
// BenchmarkGrokMatchThreeTest/original100              1388	   1068029 ns/op	  231260 B/op	    3500 allocs/op
// BenchmarkGrokMatchThreeTest/original1000              130	   8479269 ns/op	 2311385 B/op	   35061 allocs/op
// BenchmarkGrokMatchThreeTest/original10000              13	  84163120 ns/op	22991749 B/op	  353079 allocs/op
func BenchmarkGrokMatchThreeTest(b *testing.B) {
	for _, param := range params2 {
		b.Run(param.name+strconv.Itoa(param.num), func(b *testing.B) {
			Logs := param.mockFunc2(param.num, param.separator, param.separatorReg)
			processor := pipeline.Processors["processor_grok"]().(*ProcessorGrok)
			processor.Match = []string{
				"%{BASE16FLOAT}",
				"%{BASE10NUM}",
				"%{IPV4:ipv4}",
			}
			processor.Init(mock.NewEmptyContext("p", "l", "c"))

			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				processor.ProcessLogs(Logs)
			}
			b.StopTimer()
			for _, log := range Logs {
				if log.Contents[1].Key != "ipv4" {
					b.Log("Parse error")
				}
			}
		})
	}
}

// goos: linux
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/plugins/processor/grok
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkGrokMatchFiveTest/original10                4166	    275679 ns/op	   24108 B/op	     370 allocs/op
// BenchmarkGrokMatchFiveTest/original100                505	   2379337 ns/op	  241500 B/op	    3701 allocs/op
// BenchmarkGrokMatchFiveTest/original1000                43	  24029462 ns/op	 2427019 B/op	   37140 allocs/op
// BenchmarkGrokMatchFiveTest/original10000                5	 240189537 ns/op	24249364 B/op	  376009 allocs/op
func BenchmarkGrokMatchFiveTest(b *testing.B) {
	for _, param := range params2 {
		b.Run(param.name+strconv.Itoa(param.num), func(b *testing.B) {
			Logs := param.mockFunc2(param.num, param.separator, param.separatorReg)
			processor := pipeline.Processors["processor_grok"]().(*ProcessorGrok)
			processor.Match = []string{
				"%{IPV6}",
				"%{MAC}",
				"%{BASE16FLOAT}",
				"%{BASE10NUM}",
				"%{IPV4:ipv4}",
			}
			processor.Init(mock.NewEmptyContext("p", "l", "c"))

			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				processor.ProcessLogs(Logs)
			}
			b.StopTimer()
			for _, log := range Logs {
				if log.Contents[1].Key != "ipv4" {
					b.Log("Parse error")
				}
			}
		})
	}
}
