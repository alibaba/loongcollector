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

package regex

import (
	"context"
	"regexp"
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/pingcap/check"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

var _ = check.Suite(&processorTestSuite{})

func Test(t *testing.T) {
	logger.InitTestLogger()
	check.TestingT(t)
}

type processorTestSuite struct {
	processor pipeline.Processor
}

func (s *processorTestSuite) SetUpTest(c *check.C) {
	s.processor = pipeline.Processors["processor_regex"]()
	require.Error(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestInitError(c *check.C) {
	processor, _ := s.processor.(*ProcessorRegex)
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
	processor.Keys = []string{"key"}
	processor.Regex = "("
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
}

func (s *processorTestSuite) TestDefault(c *check.C) {
	// Case: catch all content as a field.
	{
		log := `2021-08-27 13:04:14.920 77711773 [ThreadName] INFO  content detail`
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"content", "time", "level"}
		processor.Regex = "((\\d{4}[-]\\d{2}[-]\\d{2}\\s\\d{2}[:]\\d{2}[:]\\d{2}[.]\\d{3})[\\S\\s]*(INFO|WARN|ERROR|DEBUG)[\\s\\S]*)"
		processor.KeepSource = false
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*models.Log, 1)
		logArray[0] = logEvent

		outLogs := processortest.Apply(s.processor, logArray...)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("time")), check.Equals, "2021-08-27 13:04:14.920")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("level")), check.Equals, "INFO")
	}
	// Case: end with line feed, must use [\s\S]* if line feed needs to be included.
	{
		log := "[2021-08-27 16:37:09][ONLINE_ROLE],json\nnextline"
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"jsonlog"}

		{
			processor.Regex = "^[^,]+,(.*)$"
			require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
			outLogs := processortest.Apply(s.processor, processortest.Log("content", log))
			c.Assert(len(outLogs), check.Equals, 1)
			c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("jsonlog")), check.Equals, "json\nnextline")
		}
		{
			processor.Regex = "^[^,]+,([\\s\\S]*)$"
			require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
			outLogs := processortest.Apply(s.processor, processortest.Log("content", log))
			c.Assert(len(outLogs), check.Equals, 1)
			c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("jsonlog")), check.Equals, "json\nnextline")
		}
		{
			// Does not include line feed.
			processor.Regex = "^[^,]+,(.*)"
			require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
			outLogs := processortest.Apply(s.processor, processortest.Log("content", log))
			c.Assert(len(outLogs), check.Equals, 1)
			c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("jsonlog")), check.Equals, "json\nnextline")
		}
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "xxxx")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "yyyy")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key3")), check.Equals, "zzzz")
	}
	{
		var log = `10.200.98.220 - - [26/Jun/2017:13:45:41 +0800] "POST /PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java" 215519025`
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"ip", "time", "method", "url", "request_time", "length", "status", "length", "url", "browser", "seqNo"}
		values := []string{"10.200.98.220", "26/Jun/2017:13:45:41", "POST", "/PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1", "0.024", "18204", "200", "37", "-", "aliyun-sdk-java", "215519025"}
		processor.Regex = `([\d\.]+) \S+ \S+ \[(\S+) \S+\] "(\w+) ([^\"]*)" ([\d\.]+) (\d+) (\d+) (\d+|-) "([^\"]*)" "([^\"]*)".* (\d+)`
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		expected := make(map[string]interface{}, len(values))
		for i, value := range values {
			expected[processor.Keys[i]] = value
		}
		c.Assert(outLogs[0].Contents.Iterator(), check.DeepEquals, expected)
	}
	{
		var log = `123dsafsfdf10.200.98.220 - - [26/Jun/2017:13:45:41 +0800] "POST /PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java" 215519025AAAA`
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.FullMatch = false
		processor.Keys = []string{"ip", "time", "method", "url", "request_time", "length", "status", "length", "url", "browser", "seqNo"}
		values := []string{"10.200.98.220", "26/Jun/2017:13:45:41", "POST", "/PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1", "0.024", "18204", "200", "37", "-", "aliyun-sdk-java", "215519025"}
		processor.Regex = `([\d\.]+) \S+ \S+ \[(\S+) \S+\] "(\w+) ([^\"]*)" ([\d\.]+) (\d+) (\d+) (\d+|-) "([^\"]*)" "([^\"]*)".* (\d+)`
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		expected := make(map[string]interface{}, len(values))
		for i, value := range values {
			expected[processor.Keys[i]] = value
		}
		c.Assert(outLogs[0].Contents.Iterator(), check.DeepEquals, expected)
	}
	{
		// not match
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		processor.SourceKey = "content"
		processor.KeepSource = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 4)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "xxxx")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "yyyy")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key3")), check.Equals, "zzzz")
	}
}

func (s *processorTestSuite) TestNotMatch(c *check.C) {
	{
		// not match key
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3", "key4"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		processor.SourceKey = "content"
		processor.KeepSource = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}
	{
		// not match key
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3", "keyx"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		processor.SourceKey = "content"
		processor.KeepSource = true
		processor.NoMatchError = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}
}

func (s *processorTestSuite) TestNoKeyAlarmAndPreserve(c *check.C) {
	{
		// no key
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		processor.SourceKey = "xxxx"
		processor.NoMatchError = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3"}
		processor.Regex = `(\d+)\s(\w+)\s(\w+)`
		processor.SourceKey = "content"
		processor.KeepSource = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}
}

func (s *processorTestSuite) TestKeepSourceIfParseError(c *check.C) {
	const sourceKeyName = "content"
	const sourceValue = "xxxx\nyyyy\nzzzz"

	// not match key
	processor, _ := s.processor.(*ProcessorRegex)
	processor.Keys = []string{"key1", "key2", "key3", "key4"}
	processor.Regex = `(\w+)\s(\w+)\s(\w+)`
	processor.SourceKey = sourceKeyName
	processor.KeepSource = false
	require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

	processor.KeepSourceIfParseError = false
	{
		outLogs := processortest.Apply(s.processor, processortest.Log(sourceKeyName, sourceValue))
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 0)
	}

	processor.KeepSourceIfParseError = true
	{
		outLogs := processortest.Apply(s.processor, processortest.Log(sourceKeyName, sourceValue))
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Contains(sourceKeyName), check.IsTrue)
		c.Assert(outLogs[0].Contents.Get(sourceKeyName), check.Equals, sourceValue)
	}
}

// TestProcessorRegex_ProcessParse verifies that Process extracts the
// configured capture groups from the SourceKey value into new keys.
func TestProcessorRegex_ProcessParse(t *testing.T) {
	processor := &ProcessorRegex{
		Regex:      `(\w+)\s(\w+)\s(\w+)`,
		Keys:       []string{"key1", "key2", "key3"},
		SourceKey:  "content",
		KeepSource: true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "xxxx yyyy zzzz")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "xxxx", contents.Get("key1"))
	assert.Equal(t, "yyyy", contents.Get("key2"))
	assert.Equal(t, "zzzz", contents.Get("key3"))
	// KeepSource=true keeps the original field.
	assert.Equal(t, "xxxx yyyy zzzz", contents.Get("content"))
}

// TestProcessorRegex_ProcessKeepSourceFalse verifies the source field is
// dropped on a successful parse when KeepSource is false.
func TestProcessorRegex_ProcessKeepSourceFalse(t *testing.T) {
	processor := &ProcessorRegex{
		Regex:      `(\w+)\s(\w+)\s(\w+)`,
		Keys:       []string{"key1", "key2", "key3"},
		SourceKey:  "content",
		KeepSource: false,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "xxxx yyyy zzzz")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.False(t, contents.Contains("content"), "source key must be removed when KeepSource is false")
	assert.Equal(t, "xxxx", contents.Get("key1"))
	assert.Equal(t, "zzzz", contents.Get("key3"))
}

// TestProcessorRegex_ProcessNoMatchKeepsSource verifies that on a parse
// failure the source is kept (KeepSourceIfParseError) and no keys are added.
func TestProcessorRegex_ProcessNoMatchKeepsSource(t *testing.T) {
	processor := &ProcessorRegex{
		Regex:                  `(\d+)\s(\w+)\s(\w+)`,
		Keys:                   []string{"key1", "key2", "key3"},
		SourceKey:              "content",
		KeepSource:             false,
		KeepSourceIfParseError: true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "xxxx yyyy zzzz")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.True(t, contents.Contains("content"), "source kept on parse error (KeepSourceIfParseError)")
	assert.False(t, contents.Contains("key1"), "no keys extracted on no-match")
}

// TestProcessorRegex_ProcessPassesThroughMetric verifies a Metric event is
// emitted unchanged (not dropped) by the log-only processor.
func TestProcessorRegex_ProcessPassesThroughMetric(t *testing.T) {
	processor := &ProcessorRegex{
		Regex:     `(\w+)`,
		Keys:      []string{"key1"},
		SourceKey: "content",
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

func TestProcessorRegexOptionalCaptures(t *testing.T) {
	for _, tc := range []struct {
		name string
		keys []string
		want models.KeyValueSlice[any]
	}{
		{
			name: "unmatched source capture",
			keys: []string{"content", "empty", "last"},
			want: models.KeyValueSlice[any]{{Key: "before", Value: "before"}, {Key: "empty", Value: ""}, {Key: "last", Value: "kept"}},
		},
		{
			name: "empty source capture",
			keys: []string{"missing", "content", "last"},
			want: models.KeyValueSlice[any]{{Key: "content", Value: ""}, {Key: "before", Value: "before"}, {Key: "last", Value: "kept"}},
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			processor := &ProcessorRegex{
				Regex:                  `^(x)?()(kept)$`,
				Keys:                   tc.keys,
				SourceKey:              "content",
				KeepSourceIfParseError: true,
			}
			require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
			contents := models.NewOrderedLogContents(2)
			contents.Add("content", "kept")
			contents.Add("before", "before")
			log := &models.Log{Contents: contents}
			outLogs := processortest.Apply(processor, log)
			require.Len(t, outLogs, 1)

			var got models.KeyValueSlice[any]
			for key, value := range models.LogContentEntries(outLogs[0].GetIndices()) {
				got = append(got, models.KeyValue[any]{Key: key, Value: value})
			}
			require.Equal(t, tc.want, got)
		})
	}
}

func TestByteBackedRegexCapturesOwnTheirText(t *testing.T) {
	p := &ProcessorRegex{
		Regex: "^(.*): (.*)$", Keys: []string{"level", "message"},
		SourceKey: "content", FullMatch: true, KeepSource: true,
	}
	require.NoError(t, p.Init(mock.NewEmptyContext("p", "l", "c")))
	body := []byte("INFO: 非 ASCII")
	log := &models.Log{Contents: models.NewLogContents()}
	log.SetBody(body)
	p.processLogEvent(log)
	clear(body)
	require.Equal(t, "INFO", log.Contents.Get("level"))
	require.Equal(t, "非 ASCII", log.Contents.Get("message"))
}

func TestTrailingCapturePrefixMatchesOriginal(t *testing.T) {
	inputs := []string{
		"id=load,7 level=INFO msg=", "id=load,7 level=INFO msg=line1\nline2",
		"id=load,7 level=INFO msg=\xff", "id=load,7 level=WARN msg=x",
		"aaa", "a", "ab-tail", "含中文",
		"id=load,7 level=INFO msg=a msg=b", "id=load,7 level=INFO missing",
		"id=load,7 level=INFO MSG=case fold", "id=load,7 level=INFO 标记=unicode",
		"id=load,7 level=INFO msg=" + strings.Repeat("x", 5000),
	}
	expressions := []string{
		`^id=(\w+,\d+) level=(INFO) msg=(.*)$`,
		`^id=(.+) msg=(.*)$`,
		`^id=(\w+,\d+) level=(INFO)(?i: msg=)(.*)$`,
		`^id=(\w+,\d+) level=(INFO) 标记=(.*)$`,
		`^(a+?)(b)?(.*)$`,
		`^(?P<prefix>[a-z]+)(?P<tail>.*)\z`,
	}
	for _, expression := range expressions {
		t.Run(expression, func(t *testing.T) {
			pattern := "(?s)" + expression
			original := regexp.MustCompile(pattern)
			prefix, endLiteral := compileTrailingCapturePrefix(pattern, original.NumSubexp())
			require.NotNil(t, prefix)
			keys := make([]string, original.NumSubexp())
			p := ProcessorRegex{Keys: keys, FullMatch: true, trailingCapturePrefix: prefix, prefixEndLiteral: endLiteral}
			for _, input := range inputs {
				want := original.FindStringSubmatchIndex(input)
				if want != nil {
					want = want[2:]
				}
				var match []int
				if length := p.matchInputLength(input); length >= 0 {
					match = prefix.FindStringSubmatchIndex(input[:length])
				}
				var offsets [16]int
				got := p.configuredCaptures(match, len(input), offsets[:0], func() string {
					t.Fatal("unexpected alarm preview")
					return ""
				})
				require.Equal(t, want, got, "input %q", input)
			}
		})
	}
}

func TestRegexDoesNotReuseExposedBody(t *testing.T) {
	p := ProcessorRegex{
		Regex: `^text=(.*)$`, Keys: []string{"message"}, SourceKey: models.BodyKey,
	}
	require.NoError(t, p.Init(mock.NewEmptyContext("p", "l", "c")))
	for _, access := range []func(*models.Log) []byte{
		func(log *models.Log) []byte { return log.GetBody() },
		func(log *models.Log) []byte { return log.GetIndices().Iterator()[models.BodyKey].([]byte) },
		func(log *models.Log) []byte { return log.GetIndices().SortTo(nil)[0].Value.([]byte) },
	} {
		log := &models.Log{Contents: models.NewOrderedLogContents(1)}
		models.SetOwnedLogBody(log, []byte("text=original message"))
		exposed := access(log)
		p.processLogEvent(log)
		clear(exposed)
		require.False(t, log.Contents.Contains(models.BodyKey))
		require.Equal(t, "original message", log.Contents.Get("message"))
	}
}
