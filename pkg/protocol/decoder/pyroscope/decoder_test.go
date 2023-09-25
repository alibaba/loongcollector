// Copyright 2023 iLogtail Authors
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

package pyroscope

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"net/http"
	"os"
	"sort"
	"testing"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/pyroscope-io/pyroscope/pkg/structs/transporttrie"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestDecoder_DecodeTire(t *testing.T) {

	type value struct {
		k string
		v uint64
	}
	values := []value{
		{"foo;bar;baz", 1},
		{"foo;bar;baz;a", 1},
		{"foo;bar;baz;b", 1},
		{"foo;bar;baz;c", 1},

		{"foo;bar;bar", 1},

		{"foo;bar;qux", 1},

		{"foo;bax;bar", 1},

		{"zoo;boo", 1},
		{"zoo;bao", 1},
	}

	trie := transporttrie.New()
	for _, v := range values {
		trie.Insert([]byte(v.k), v.v)
	}
	var buf bytes.Buffer
	trie.Serialize(&buf)
	println(buf.String())
	request, err := http.NewRequest("POST", "http://localhost:8080?aggregationType=sum&from=1673495500&name=demo.cpu{a=b}&sampleRate=100&spyName=ebpfspy&units=samples&until=1673495510", &buf)
	request.Header.Set("Content-Type", "binary/octet-stream+trie")
	assert.NoError(t, err)
	d := new(Decoder)
	logs, err := d.Decode(buf.Bytes(), request, map[string]string{"cluster": "sls-mall"})
	assert.NoError(t, err)
	assert.True(t, len(logs) == 9)
	log := logs[1]
	require.Equal(t, ReadLogVal(log, "name"), "baz")
	require.Equal(t, ReadLogVal(log, "stack"), "bar\nfoo")
	require.Equal(t, ReadLogVal(log, "language"), "ebpf")
	require.Equal(t, ReadLogVal(log, "type"), "profile_cpu")
	require.Equal(t, ReadLogVal(log, "units"), "nanoseconds")
	require.Equal(t, ReadLogVal(log, "valueTypes"), "cpu")
	require.Equal(t, ReadLogVal(log, "aggTypes"), "sum")
	require.Equal(t, ReadLogVal(log, "dataType"), "CallStack")
	require.Equal(t, ReadLogVal(log, "durationNs"), "10000000000")
	require.Equal(t, ReadLogVal(log, "labels"), "{\"__name__\":\"demo\",\"a\":\"b\",\"cluster\":\"sls-mall\"}")
	require.Equal(t, ReadLogVal(log, "val"), "10000000.00")
}

func TestDecoder_DecodePprofCumulative(t *testing.T) {
	data, err := os.ReadFile("test/dump_pprof_mem_data")
	require.NoError(t, err)
	var length uint32
	buffer := bytes.NewBuffer(data)
	require.NoError(t, binary.Read(buffer, binary.BigEndian, &length))
	data = data[4 : 4+int(length)]
	var d helper.DumpData
	require.NoError(t, json.Unmarshal(data, &d))
	request, err := http.NewRequest("POST", d.Req.URL, bytes.NewReader(d.Req.Body))
	require.NoError(t, err)
	request.Header = d.Req.Header
	dec := new(Decoder)
	logs, err := dec.Decode(d.Req.Body, request, map[string]string{"cluster": "sls-mall"})
	require.NoError(t, err)
	require.Equal(t, len(logs), 4)
	sort.Slice(logs, func(i, j int) bool {
		if ReadLogVal(logs[i], "name") < ReadLogVal(logs[j], "name") {
			return true
		} else if ReadLogVal(logs[i], "name") == ReadLogVal(logs[j], "name") {
			return ReadLogVal(logs[i], "valueTypes") < ReadLogVal(logs[j], "valueTypes")
		}
		return false
	})
	require.Equal(t, ReadLogVal(logs[0], "name"), "compress/flate.NewWriter /Users/evan/sdk/go1.19.4/src/compress/flate/deflate.go")
	require.Equal(t, ReadLogVal(logs[0], "valueTypes"), "alloc_objects")
	require.Equal(t, ReadLogVal(logs[0], "val"), "1.00")

	require.Equal(t, ReadLogVal(logs[1], "name"), "compress/flate.NewWriter /Users/evan/sdk/go1.19.4/src/compress/flate/deflate.go")
	require.Equal(t, ReadLogVal(logs[1], "valueTypes"), "alloc_space")
	require.Equal(t, ReadLogVal(logs[1], "val"), "924248.00")

	require.Equal(t, ReadLogVal(logs[2], "name"), "runtime/pprof.WithLabels /Users/evan/sdk/go1.19.4/src/runtime/pprof/label.go")
	require.Equal(t, ReadLogVal(logs[2], "valueTypes"), "alloc_objects")
	require.Equal(t, ReadLogVal(logs[2], "val"), "1820.00")

	require.Equal(t, ReadLogVal(logs[3], "name"), "runtime/pprof.WithLabels /Users/evan/sdk/go1.19.4/src/runtime/pprof/label.go")
	require.Equal(t, ReadLogVal(logs[3], "valueTypes"), "alloc_space")
	require.Equal(t, ReadLogVal(logs[3], "val"), "524432.00")
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
