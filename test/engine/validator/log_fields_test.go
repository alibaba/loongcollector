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

package validator

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func init() {
	logger.InitTestLogger(logger.OptionDebugLevel)
}

func Test_fieldsLogValidator_Valid(t *testing.T) {
	type fields struct {
		ExpectLogFields   []string
		ExpectLogFieldNum int
		ExpectLogTags     []string
		ExpectTagNum      int
		ExpectLogCategory string
		ExpectLogTopic    string
		ExpectLogSource   string
	}
	type args struct {
		log *protocol.LogGroup
	}
	tests := []struct {
		name   string
		fields fields
		args   args
		want   []*Report
	}{
		{
			name: "unexpect-log-fields",
			fields: fields{
				ExpectLogFields: []string{"field1", "fields2"},
			},
			args: args{log: &protocol.LogGroup{Logs: []*protocol.Log{
				{Contents: []*protocol.Log_Content{
					{
						Key:   "field1",
						Value: "val1",
					},
				}},
			}}},
			want: []*Report{
				{
					Validator: fieldsLogValidatorName,
					Name:      "log_field",
					Want:      "field1,fields2",
					NotFound:  "fields2",
				},
			},
		},

		{
			name: "unexpect-log-fields-num",
			fields: fields{
				ExpectLogFieldNum: 2,
			},
			args: args{log: &protocol.LogGroup{Logs: []*protocol.Log{
				{Contents: []*protocol.Log_Content{
					{
						Key:   "field1",
						Value: "val1",
					},
				}},
			}}},
			want: []*Report{
				{
					Validator: fieldsLogValidatorName,
					Name:      "log_field_num",
					Want:      "2",
					Got:       "1",
				},
			},
		},
		{
			name: "unexpect-log-tags",
			fields: fields{
				ExpectLogTags: []string{"tag1", "tag2"},
			},
			args: args{log: &protocol.LogGroup{
				LogTags: []*protocol.LogTag{
					{
						Key:   "tag1",
						Value: "value1",
					},
				},
			}},
			want: []*Report{
				{
					Validator: fieldsLogValidatorName,
					Name:      "log_tag",
					Want:      "tag1,tag2",
					NotFound:  "tag2",
				},
			},
		},
		{
			name: "unexpect-log-tags-num",
			fields: fields{
				ExpectTagNum: 2,
			},
			args: args{log: &protocol.LogGroup{
				LogTags: []*protocol.LogTag{
					{
						Key:   "tag1",
						Value: "value1",
					},
				},
			}},
			want: []*Report{
				{
					Validator: fieldsLogValidatorName,
					Name:      "log_tag_num",
					Want:      "2",
					Got:       "1",
				},
			},
		},
		{
			name: "unexpect-log-category",
			fields: fields{
				ExpectLogCategory: "category1",
			},
			args: args{log: &protocol.LogGroup{
				Category: "category2",
			}},
			want: []*Report{
				{
					Validator: fieldsLogValidatorName,
					Name:      "log_category",
					Want:      "category1",
					Got:       "category2",
				},
			},
		},
		{
			name: "unexpect-log-topic",
			fields: fields{
				ExpectLogTopic: "topic1",
			},
			args: args{log: &protocol.LogGroup{
				Topic: "topic2",
			}},
			want: []*Report{
				{
					Validator: fieldsLogValidatorName,
					Name:      "log_topic",
					Want:      "topic1",
					Got:       "topic2",
				},
			},
		},
		{
			name: "unexpect-log-source",
			fields: fields{
				ExpectLogSource: "source1",
			},
			args: args{log: &protocol.LogGroup{
				Source: "source2",
			}},
			want: []*Report{
				{
					Validator: fieldsLogValidatorName,
					Name:      "log_source",
					Want:      "source1",
					Got:       "source2",
				},
			},
		},
		{
			name: "success",
			fields: fields{
				ExpectLogSource:   "source1",
				ExpectLogTopic:    "topic1",
				ExpectLogCategory: "category1",
				ExpectTagNum:      2,
				ExpectLogFieldNum: 2,
				ExpectLogFields:   []string{"field1", "field2"},
				ExpectLogTags:     []string{"tag1", "tag2"},
			},
			args: args{log: &protocol.LogGroup{
				Source:   "source1",
				Topic:    "topic1",
				Category: "category1",
				LogTags: []*protocol.LogTag{
					{
						Key:   "tag1",
						Value: "value1",
					},
					{
						Key:   "tag2",
						Value: "value2",
					},
				},
				Logs: []*protocol.Log{
					{
						Contents: []*protocol.Log_Content{
							{
								Key:   "field1",
								Value: "val1",
							},
							{
								Key:   "field2",
								Value: "val2",
							},
						},
					},
				},
			}},
			want: nil,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			f := &fieldsLogValidator{
				ExpectLogFields:   tt.fields.ExpectLogFields,
				ExpectLogFieldNum: tt.fields.ExpectLogFieldNum,
				ExpectLogTags:     tt.fields.ExpectLogTags,
				ExpectTagNum:      tt.fields.ExpectTagNum,
				ExpectLogCategory: tt.fields.ExpectLogCategory,
				ExpectLogTopic:    tt.fields.ExpectLogTopic,
				ExpectLogSource:   tt.fields.ExpectLogSource,
			}
			assert.Equal(t, tt.want, f.Valid(tt.args.log))
		})
	}
}
