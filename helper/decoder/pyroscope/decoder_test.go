package pyroscope

import (
	"bytes"
	"net/http"
	"testing"

	"github.com/alibaba/ilogtail/plugins/test"

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
	require.Equal(t, test.ReadLogVal(log, "name"), "baz")
	require.Equal(t, test.ReadLogVal(log, "stack"), "bar\nfoo")
	require.Equal(t, test.ReadLogVal(log, "language"), "ebpf")
	require.Equal(t, test.ReadLogVal(log, "type"), "profile_cpu")
	require.Equal(t, test.ReadLogVal(log, "units"), "samples")
	require.Equal(t, test.ReadLogVal(log, "valueTypes"), "cpu")
	require.Equal(t, test.ReadLogVal(log, "aggTypes"), "sum")
	require.Equal(t, test.ReadLogVal(log, "dataType"), "CallStack")
	require.Equal(t, test.ReadLogVal(log, "durationNs"), "10000000000")
	require.Equal(t, test.ReadLogVal(log, "labels"), "{\"__name__\":\"demo\",\"a\":\"b\",\"cluster\":\"sls-mall\"}")
	require.Equal(t, test.ReadLogVal(log, "val"), "1.00")
}
