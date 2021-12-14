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
	"github.com/alibaba/ilogtail/pkg/util"

	"fmt"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"

	"github.com/stretchr/testify/require"
)

func TestRfc3164(t *testing.T) {
	nowTime := time.Now()
	tests := []struct {
		title  string
		log    []byte
		result *parseResult
	}{
		{
			title: "",
			log:   []byte(`<60>Oct 09 14:36:47 hostname program: message`),
			result: &parseResult{
				hostname: "hostname",
				program:  "program",
				priority: 60,
				facility: 7,
				severity: 4,
				content:  "message",
				time:     time.Date(nowTime.Year(), time.October, 9, 14, 36, 47, 0, nowTime.Location()),
			},
		},
		{
			title: "",
			log:   []byte(`<34>Aug  2 09:49:23 hostname program: message`),
			result: &parseResult{
				hostname: "hostname",
				program:  "program",
				priority: 34,
				facility: 4,
				severity: 2,
				content:  "message",
				time:     time.Date(nowTime.Year(), time.August, 2, 9, 49, 23, 0, nowTime.Location()),
			},
		},
		{
			title: "",
			log:   []byte(`<86>Jul 31 13:14:22 rs1e13316 su: pam_unix(su:session): session closed for user nobody`),
			result: &parseResult{
				hostname: "rs1e13316",
				program:  "su",
				priority: 86,
				facility: 10,
				severity: 6,
				content:  "pam_unix(su:session): session closed for user nobody",
				time:     time.Date(nowTime.Year(), time.July, 31, 13, 14, 22, 0, nowTime.Location()),
			},
		},
		{
			title: "",
			log:   []byte(`<13>Aug  1 14:32:44 ecs-test-yyh root: dfjksdfjkdlsfjsklf`),
			result: &parseResult{
				hostname: "ecs-test-yyh",
				program:  "root",
				priority: 13,
				facility: 1,
				severity: 5,
				content:  "dfjksdfjkdlsfjsklf",
				time:     time.Date(nowTime.Year(), time.August, 1, 14, 32, 44, 0, nowTime.Location()),
			},
		},
		{
			title: "",
			log:   []byte(`<85>Aug  1 14:31:58 ecs-test-yyh polkitd[457]: Registered Authentication Agent for unix-process:22755:258653719 (system bus name :1.10269 [/usr/bin/pkttyagent --notify-fd 5 --fallback], object path /org/freedesktop/PolicyKit1/AuthenticationAgent, locale en_US.UTF-8)`),
			result: &parseResult{
				hostname: "ecs-test-yyh",
				program:  "polkitd",
				priority: 85,
				facility: 10,
				severity: 5,
				content:  `Registered Authentication Agent for unix-process:22755:258653719 (system bus name :1.10269 [/usr/bin/pkttyagent --notify-fd 5 --fallback], object path /org/freedesktop/PolicyKit1/AuthenticationAgent, locale en_US.UTF-8)`,
				time:     time.Date(nowTime.Year(), time.August, 1, 14, 31, 58, 0, nowTime.Location()),
			},
		},
		{
			title: "",
			log:   []byte(`<13>Aug 17 03:42:11 ecs-test-yyh LOGSTASH[-]: hello, a syslog from logstash`),
			result: &parseResult{
				hostname: "ecs-test-yyh",
				program:  "LOGSTASH",
				priority: 13,
				facility: 1,
				severity: 5,
				content:  `hello, a syslog from logstash`,
				time:     time.Date(nowTime.Year(), time.August, 17, 3, 42, 11, 0, nowTime.Location()),
			},
		},
	}

	creator, exist := syslogParsers["rfc3164"]
	require.True(t, exist)
	config := parserConfig{
		ignoreParseFailure: true,
	}
	p := creator(&config)
	require.NotEmpty(t, p)
	for _, test := range tests {
		r, err := p.Parse(test.log)
		require.NoError(t, err)
		require.NotEqual(t, string(test.log), r.content)
		require.Equal(t, test.result, r)
	}
}

func TestRfc3164WithoutHostnameField(t *testing.T) {
	nowTime := time.Now()
	tests := []struct {
		title  string
		log    []byte
		result *parseResult
	}{
		{
			title: "",
			log:   []byte(`<86>Apr 16 14:33:06 su: pam_unix(su:session): session opened for user root by (uid=0)`),
			result: &parseResult{
				hostname: util.GetHostName(),
				program:  "su",
				priority: 86,
				facility: 10,
				severity: 6,
				content:  `pam_unix(su:session): session opened for user root by (uid=0)`,
				time:     time.Date(nowTime.Year(), time.April, 16, 14, 33, 6, 0, nowTime.Location()),
			},
		},
	}

	creator, exist := syslogParsers["rfc3164"]
	require.True(t, exist)
	config := parserConfig{
		ignoreParseFailure: true,
		addHostname:        true,
	}
	p := creator(&config)
	require.NotEmpty(t, p)
	for _, test := range tests {
		r, err := p.Parse(test.log)
		require.NoError(t, err)
		if -1 == r.priority {
			require.Equal(t, string(test.log), r.content)
		} else {
			require.Equal(t, test.result, r)
		}
	}
}

func TestRfc5424(t *testing.T) {
	tests := []struct {
		title  string
		log    []byte
		result *parseResult
	}{
		{
			title:  "parse failed",
			log:    []byte(`Jul 29 06:20:01 ecs-test-yyh systemd: Started Session 4530 of user root.`),
			result: nil,
		},
		{
			title: "",
			log:   []byte(`<29>1 2016-02-21T04:32:57+00:00 web1 someservice 2341 2 [origin][meta sequence="14125553" service="someservice"] "GET /v1/ok HTTP/1.1" 200 145 "-" "hacheck 0.9.0" 24306 127.0.0.1:40124 575`),
			result: &parseResult{
				hostname: "web1",
				program:  "someservice",
				priority: 29,
				facility: 3,
				severity: 5,
				procID:   proto.String("2341"),
				msgID:    proto.String("2"),
				structuredData: &map[string]map[string]string{
					"origin": {},
					"meta": {
						"sequence": "14125553",
						"service":  "someservice",
					},
				},
				content: `"GET /v1/ok HTTP/1.1" 200 145 "-" "hacheck 0.9.0" 24306 127.0.0.1:40124 575`,
				time:    time.Date(2016, time.February, 21, 4, 32, 57, 0, time.FixedZone("", 0)),
			},
		},
		{
			title: "",
			log:   []byte(`<34>1 2003-10-11T22:14:15.003Z mymachine.example.com su - ID47 - BOM'su root' failed for lonvick on /dev/pts/8`),
			result: &parseResult{
				hostname: "mymachine.example.com",
				program:  "su",
				priority: 34,
				facility: 4,
				severity: 2,
				procID:   nil,
				msgID:    proto.String("ID47"),
				content:  `BOM'su root' failed for lonvick on /dev/pts/8`,
				time:     time.Date(2003, time.October, 11, 22, 14, 15, 3*1000000, time.UTC),
			},
		},
	}

	creator, exist := syslogParsers["rfc5424"]
	require.True(t, exist)
	config := parserConfig{
		ignoreParseFailure: true,
	}
	p := creator(&config)
	require.NotEmpty(t, p)
	for _, test := range tests {
		fmt.Printf("tttt \n")
		r, err := p.Parse(test.log)
		require.NoError(t, err)
		if -1 == r.priority {
			if test.result != nil {
				t.Error("parse fail")
			}
			fmt.Printf("############## %s %s\n", string(test.log), r.content)
			require.Equal(t, string(test.log), r.content)
		} else {
			require.Equal(t, test.result.hostname, r.hostname)
			require.Equal(t, test.result.program, r.program)
			require.Equal(t, test.result.priority, r.priority)
			require.Equal(t, test.result.facility, r.facility)
			require.Equal(t, test.result.severity, r.severity)
			require.Equal(t, test.result.procID, r.procID)
			require.Equal(t, test.result.msgID, r.msgID)
			require.Equal(t, test.result.content, r.content)
			require.Equal(t, test.result.time.Unix(), r.time.Unix())
			require.Equal(t, test.result.structuredData, r.structuredData)
		}
	}
}

func TestAutoParser(t *testing.T) {
	nowTime := time.Now()
	tests := []struct {
		title  string
		log    []byte
		result *parseResult
	}{
		{
			title: "RFC3164 1",
			log:   []byte(`<34>Aug  2 09:49:23 hostname program: message`),
			result: &parseResult{
				hostname: "hostname",
				program:  "program",
				priority: 34,
				facility: 4,
				severity: 2,
				content:  "message",
				time:     time.Date(nowTime.Year(), time.August, 2, 9, 49, 23, 0, nowTime.Location()),
			},
		},
		{
			title: "RFC3164 2",
			log:   []byte(`<86>Jul 31 13:14:22 rs1e13316 su: pam_unix(su:session): session closed for user nobody`),
			result: &parseResult{
				hostname: "rs1e13316",
				program:  "su",
				priority: 86,
				facility: 10,
				severity: 6,
				content:  "pam_unix(su:session): session closed for user nobody",
				time:     time.Date(nowTime.Year(), time.July, 31, 13, 14, 22, 0, nowTime.Location()),
			},
		},
		{
			title: "RFC5424 1",
			log:   []byte(`<29>1 2016-02-21T04:32:57+00:00 web1 someservice 2341 2 [origin][meta sequence="14125553" service="someservice"] "GET /v1/ok HTTP/1.1" 200 145 "-" "hacheck 0.9.0" 24306 127.0.0.1:40124 575`),
			result: &parseResult{
				hostname: "web1",
				program:  "someservice",
				priority: 29,
				facility: 3,
				severity: 5,
				procID:   proto.String("2341"),
				msgID:    proto.String("2"),
				structuredData: &map[string]map[string]string{
					"origin": {},
					"meta": {
						"sequence": "14125553",
						"service":  "someservice",
					},
				},
				content: `"GET /v1/ok HTTP/1.1" 200 145 "-" "hacheck 0.9.0" 24306 127.0.0.1:40124 575`,
				time:    time.Date(2016, time.February, 21, 4, 32, 57, 0, time.FixedZone("", 0)),
			},
		},
		{
			title: "RFC5424 2",
			log:   []byte(`<34>1 2003-10-11T22:14:15.003Z mymachine.example.com su - ID47 - BOM'su root' failed for lonvick on /dev/pts/8`),
			result: &parseResult{
				hostname: "mymachine.example.com",
				program:  "su",
				priority: 34,
				facility: 4,
				severity: 2,
				procID:   nil,
				msgID:    proto.String("ID47"),
				content:  `BOM'su root' failed for lonvick on /dev/pts/8`,
				time:     time.Date(2003, time.October, 11, 22, 14, 15, 3*1000000, time.UTC),
			},
		},
		{
			title:  "All parsers failed",
			log:    []byte(`<341 2003-10-11T22:14:15.003Z mymachine.example.com su - ID47 - BOM'su root' failed for lonvick on /dev/pts/8`),
			result: nil,
		},
	}

	creator, exist := syslogParsers["auto"]
	require.True(t, exist)
	config := parserConfig{
		ignoreParseFailure: true,
	}
	p := creator(&config)
	require.NotEmpty(t, p)
	for _, test := range tests {
		r, err := p.Parse(test.log)
		require.NoError(t, err)
		if -1 == r.priority {
			require.Equal(t, string(test.log), r.content)
		} else {
			require.Equal(t, test.result, r)
		}
	}

	// Disable ignoreParseFailure.
	config.ignoreParseFailure = false
	p = creator(&config)
	require.NotEmpty(t, p)
	for _, test := range tests {
		r, err := p.Parse(test.log)
		if err != nil {
			if test.result != nil {
				t.Errorf("parse error %s %s", test.title, string(test.log))
			}
			require.Empty(t, r)
			continue
		}
		require.Equal(t, test.result.hostname, r.hostname)
		require.Equal(t, test.result.program, r.program)
		require.Equal(t, test.result.priority, r.priority)
		require.Equal(t, test.result.facility, r.facility)
		require.Equal(t, test.result.severity, r.severity)
		require.Equal(t, test.result.procID, r.procID)
		require.Equal(t, test.result.msgID, r.msgID)
		require.Equal(t, test.result.content, r.content)
		require.Equal(t, test.result.time.Unix(), r.time.Unix())
		require.Equal(t, test.result.structuredData, r.structuredData)
	}
}
