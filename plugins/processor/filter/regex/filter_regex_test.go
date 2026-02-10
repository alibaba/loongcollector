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
	"testing"
	"time"

	"github.com/pingcap/check"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

var _ = check.Suite(&processorTestSuite{})

func Test(t *testing.T) {
	logger.InitTestLogger()
	check.TestingT(t)
}

type processorTestSuite struct {
	processor pipeline.ProcessorV1
}

func (s *processorTestSuite) SetUpTest(c *check.C) {
	s.processor = pipeline.Processors["processor_filter_regex"]().(pipeline.ProcessorV1)
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestInitError(c *check.C) {
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.IsNil)
	processor, _ := s.processor.(*ProcessorRegexFilter)
	processor.Exclude = map[string]string{"key": "("}
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
}

func (s *processorTestSuite) TestDefault(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorRegexFilter)
		processor.Exclude = map[string]string{"key": ".*"}
		processor.Include = map[string]string{"key1": ".*", "content": "[\\s\\S]*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "value1")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "value2")

		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key2")
	}
	{
		var log = `10.200.98.220 - - [26/Jun/2017:13:45:41 +0800] "POST /PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java" 215519025`
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegexFilter)
		reg := `([\d\.]+) \S+ \S+ \[(\S+) \S+\] "(\w+) ([^\"]*)" ([\d\.]+) (\d+) (\d+) (\d+|-) "([^\"]*)" "([^\"]*)".* (\d+)`
		processor.Include = map[string]string{"content": reg}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}

}

func (s *processorTestSuite) TestNotMatch(c *check.C) {

	{
		var log = `123abc10.200.98.220 - - [26/Jun/2017:13:45:41 +0800] "POST /PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java" 215519025xxxx`
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegexFilter)
		reg := `^([\d\.]+) \S+ \S+ \[(\S+) \S+\] "(\w+) ([^\"]*)" ([\d\.]+) (\d+) (\d+) (\d+|-) "([^\"]*)" "([^\"]*)".* (\d+)`
		processor.Include = map[string]string{"content": reg}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 0)
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorRegexFilter)
		processor.Include = map[string]string{"key": ".*"}
		processor.Exclude = map[string]string{"key1": ".*", "content": ".*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 0)
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorRegexFilter)
		processor.Include = map[string]string{"key1": ".*", "content": ".*"}
		processor.Exclude = map[string]string{"key2": ".*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 0)
	}
}

func TestFilterEvent(t *testing.T) {
	timeNow := uint64(time.Now().UnixNano())
	tests := []struct {
		group   *models.PipelineGroupEvents
		include map[string]string
		exclude map[string]string
		want    *models.PipelineGroupEvents
	}{
		{
			group: &models.PipelineGroupEvents{
				Group: models.NewGroup(models.NewMetadata(), models.NewTags()),
				Events: []models.PipelineEvent{
					models.NewSimpleLog([]byte("hello world1"), models.NewTagsWithMap(map[string]string{
						"tag1": "xxxxx",
					}), timeNow),
					models.NewSimpleLog([]byte("hello world2"), models.NewTagsWithMap(map[string]string{
						"tag1": "yyyyy",
					}), timeNow),
				},
			},
			exclude: map[string]string{
				"content": `hello\sworld1`,
			},
			want: &models.PipelineGroupEvents{
				Group: models.NewGroup(models.NewMetadata(), models.NewTags()),
				Events: []models.PipelineEvent{
					models.NewSimpleLog([]byte("hello world2"), models.NewTagsWithMap(map[string]string{
						"tag1": "yyyyy",
					}), timeNow),
				},
			},
		},
		{
			group: &models.PipelineGroupEvents{
				Group: models.NewGroup(models.NewMetadata(), models.NewTags()),
				Events: []models.PipelineEvent{
					models.NewSimpleLog([]byte("hello world1"), models.NewTagsWithMap(map[string]string{
						"tag1": "xxxxx",
					}), timeNow),
					models.NewSimpleLog([]byte("hello world2"), models.NewTagsWithMap(map[string]string{
						"tag1": "yyyyy",
					}), timeNow),
				},
			},
			include: map[string]string{
				"content": `hello\sworld1`,
			},
			want: &models.PipelineGroupEvents{
				Group: models.NewGroup(models.NewMetadata(), models.NewTags()),
				Events: []models.PipelineEvent{
					models.NewSimpleLog([]byte("hello world1"), models.NewTagsWithMap(map[string]string{
						"tag1": "xxxxx",
					}), timeNow),
				},
			},
		},
		{
			group: &models.PipelineGroupEvents{
				Group: models.NewGroup(models.NewMetadata(), models.NewTags()),
				Events: []models.PipelineEvent{
					models.NewSimpleLog([]byte("hello world1"), models.NewTagsWithMap(map[string]string{
						"tag1": "xxxxx",
					}), timeNow),
					models.NewSimpleLog([]byte("hello world2"), models.NewTagsWithMap(map[string]string{
						"tag1": "yyyyy",
					}), timeNow),
				},
			},
			include: map[string]string{
				"tag1": `xxx`,
			},
			want: &models.PipelineGroupEvents{
				Group: models.NewGroup(models.NewMetadata(), models.NewTags()),
				Events: []models.PipelineEvent{
					models.NewSimpleLog([]byte("hello world1"), models.NewTagsWithMap(map[string]string{
						"tag1": "xxxxx",
					}), timeNow),
				},
			},
		},
	}

	for _, test := range tests {
		p := &ProcessorRegexFilter{
			Include: test.include,
			Exclude: test.exclude,
		}
		err := p.Init(mock.NewEmptyContext("p", "l", "c"))
		assert.Nil(t, err)

		ctx := pipeline.NewObservePipelineConext(10)
		p.Process(test.group, ctx)
		select {
		case <-time.After(time.Second * 10):
			assert.Fail(t, "should got data returned by processor")
		case data := <-ctx.Collector().Observe():
			assert.Equal(t, test.want, data)
		}
	}
}
