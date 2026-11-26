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

package inputsyslog

import (
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/pkg/helper"
)

func BenchmarkSyslogParse(b *testing.B) {
	content := "id=tcp,000000000000 level=INFO msg=" + strings.Repeat("a", 193)
	for _, tc := range []struct {
		name     string
		protocol string
		line     string
	}{
		{"Raw", "", content},
		{"RFC3164", "rfc3164", "<34>Oct 11 22:14:15 host service: " + content},
		{"RFC5424", "rfc5424", `<34>1 2003-10-11T22:14:15.003Z host service 17 ID47 [meta sequence="1"] ` + content},
	} {
		b.Run(tc.name, func(b *testing.B) {
			input := []byte(tc.line + "\n")
			syslog := newSyslog()
			syslog.ParseProtocol = tc.protocol
			syslog.parser = syslogParsers[tc.protocol](&parserConfig{})
			collector := helper.NewObservePipelineContext(1).Collector()
			events := collector.Observe()
			clientIP := clientIPField("127.0.0.1:514")
			b.SetBytes(int64(len(input)))
			b.ReportAllocs()
			for b.Loop() {
				syslog.parse(input, clientIP, collector)
				<-events
			}
		})
	}
}
