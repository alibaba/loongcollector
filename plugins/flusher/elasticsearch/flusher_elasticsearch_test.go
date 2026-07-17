// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package elasticsearch

import (
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"sort"
	"strconv"
	"strings"
	"sync"
	"testing"

	"github.com/elastic/go-elasticsearch/v8"
	. "github.com/smartystreets/goconvey/convey"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func makeTestLogGroupList() *protocol.LogGroupList {
	fields := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}
	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs:   make([]*protocol.Log, 0, 10),
			Source: "",
		}
		for j := 1; j <= 10; j++ {
			fields["group"] = strconv.Itoa(i)
			fields["message"] = "The message: " + strconv.Itoa(j)
			l := test.CreateLogByFields(fields)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}

func makeParityLogGroups() ([]*protocol.LogGroup, []*models.PipelineGroupEvents) {
	v1Groups := makeTestLogGroupList().GetLogGroupList()[:1]
	return v1Groups, makePipelineGroupEventsFromLogGroups(v1Groups)
}

func makePipelineGroupEventsFromLogGroups(v1Groups []*protocol.LogGroup) []*models.PipelineGroupEvents {
	v2Groups := make([]*models.PipelineGroupEvents, 0, len(v1Groups))
	for _, lg := range v1Groups {
		groupTags := models.NewTags()
		groupTags.Add("host.ip", lg.Source)
		for _, tag := range lg.LogTags {
			groupTags.Add(tag.Key, tag.Value)
		}
		group := models.NewGroup(models.NewMetadata(), groupTags)
		events := make([]models.PipelineEvent, 0, len(lg.Logs))
		for _, log := range lg.Logs {
			ts := uint64(log.GetTime()) * 1e9
			if log.TimeNs != nil {
				ts += uint64(*log.TimeNs)
			}
			v2Log := models.NewLog("", nil, "", "", "", models.NewTags(), ts)
			for _, c := range log.Contents {
				v2Log.GetIndices().Add(c.Key, c.Value)
			}
			events = append(events, v2Log)
		}
		v2Groups = append(v2Groups, &models.PipelineGroupEvents{Group: group, Events: events})
	}
	return v2Groups
}

func newCustomSingleConverter(t *testing.T) *converter.Converter {
	t.Helper()
	cvt, err := converter.NewConverter(
		converter.ProtocolCustomSingle,
		converter.EncodingJSON,
		nil,
		nil,
		&config.GlobalConfig{},
	)
	require.NoError(t, err)
	return cvt
}

func normalizeJSONLines(lines [][]byte) []string {
	out := make([]string, 0, len(lines))
	for _, line := range lines {
		var obj map[string]interface{}
		if err := json.Unmarshal(line, &obj); err != nil {
			out = append(out, string(line))
			continue
		}
		normalized, err := json.Marshal(obj)
		if err != nil {
			out = append(out, string(line))
			continue
		}
		out = append(out, string(normalized))
	}
	sort.Strings(out)
	return out
}

type bulkBodyCapture struct {
	mu     sync.Mutex
	bodies []string
}

func (c *bulkBodyCapture) handler(w http.ResponseWriter, r *http.Request) {
	body, _ := io.ReadAll(r.Body)
	c.mu.Lock()
	c.bodies = append(c.bodies, string(body))
	c.mu.Unlock()
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("X-Elastic-Product", "Elasticsearch")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte(`{"errors":false,"items":[]}`))
}

func (c *bulkBodyCapture) snapshot() []string {
	c.mu.Lock()
	defer c.mu.Unlock()
	out := append([]string(nil), c.bodies...)
	return out
}

func newElasticSearchParityFlusher(t *testing.T, server *httptest.Server) *FlusherElasticSearch {
	t.Helper()
	client, err := elasticsearch.NewClient(elasticsearch.Config{Addresses: []string{server.URL}})
	require.NoError(t, err)

	f := NewFlusherElasticSearch()
	f.context = mock.NewEmptyContext("p", "l", "c")
	f.converter = newCustomSingleConverter(t)
	f.esClient = client
	f.Index = "parity-index"
	f.indexKeys = nil
	f.isDynamicIndex = false
	return f
}

func normalizeBulkBody(body string) string {
	lines := strings.Split(strings.TrimSpace(body), "\n")
	var docs []string
	for i := 0; i < len(lines); i++ {
		if strings.HasPrefix(lines[i], `{"index"`) {
			if i+1 < len(lines) {
				docs = append(docs, lines[i+1])
			}
		}
	}
	return strings.Join(normalizeJSONLines(toBytes(docs)), "\n")
}

func toBytes(lines []string) [][]byte {
	out := make([][]byte, len(lines))
	for i, line := range lines {
		out[i] = []byte(line)
	}
	return out
}

func TestGetIndexKeys(t *testing.T) {
	Convey("Given an empty index", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should not be extracted correctly", func() {
				So(err, ShouldNotBeNil)
				So(isDynamicIndex, ShouldBeFalse)
				So(keys, ShouldBeNil)
			})
		})
	})
	Convey("Given a normal index", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "normal_index",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeFalse)
				So(len(keys), ShouldEqual, 0)
			})
		})
	})
	Convey("Given a variable index", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "index_${var}",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeFalse)
				So(len(keys), ShouldEqual, 0)
			})
		})
	})
	Convey("Given a field dynamic index expression", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "index_%{content.field}",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeTrue)
				So(len(keys), ShouldEqual, 1)
				So(keys[0], ShouldEqual, "content.field")
			})
		})
	})
	Convey("Given a timestamp dynamic index expression", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "index_%{+yyyyMM}",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeTrue)
				So(len(keys), ShouldEqual, 0)
			})
		})
	})
	Convey("Given a composite dynamic index expression", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "index_%{content.field}_%{tag.host.ip}_%{+yyyyMMdd}",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeTrue)
				So(len(keys), ShouldEqual, 2)
				So(keys[0], ShouldEqual, "content.field")
				So(keys[1], ShouldEqual, "tag.host.ip")
			})
		})
	})
}

func TestFlusherElasticSearch_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := makeParityLogGroups()

	flushCapture := &bulkBodyCapture{}
	flushServer := httptest.NewServer(http.HandlerFunc(flushCapture.handler))
	defer flushServer.Close()
	flushFlusher := newElasticSearchParityFlusher(t, flushServer)
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportCapture := &bulkBodyCapture{}
	exportServer := httptest.NewServer(http.HandlerFunc(exportCapture.handler))
	defer exportServer.Close()
	exportFlusher := newElasticSearchParityFlusher(t, exportServer)
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, normalizeBulkBody(flushCapture.snapshot()[0]), normalizeBulkBody(exportCapture.snapshot()[0]))
}
