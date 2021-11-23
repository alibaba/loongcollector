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
	"reflect"
	"strings"
	"testing"
)

func TestMetricLabelValidator_validate(t *testing.T) {

}

func Test_metricLabelLogValidator_validate(t *testing.T) {
	type fields struct {
		ExpectLabelNum   int
		ExpectLabelNames []string
	}
	type args struct {
		content string
	}
	tests := []struct {
		name        string
		fields      fields
		args        args
		wantReports []*Report
	}{
		{
			name:   "test-wrong-num",
			fields: fields{ExpectLabelNum: 1},
			args: args{
				content: "tag1#$#value1|tag2#$#value2",
			},
			wantReports: []*Report{
				{
					Validator: metricsLabelValidator,
					Name:      "label_num",
					Want:      "1",
					Got:       "2",
				},
			},
		},
		{
			name: "test-wrong-pattern",
			fields: fields{ExpectLabelNames: []string{
				"tag1", "tag2",
			}},
			args: args{
				content: "tag1##value1|tag2#$#value2",
			},
			wantReports: []*Report{
				{
					Validator: metricsLabelValidator,
					Name:      "label_pattern",
					Want:      "key#$#value",
					Got:       "tag1##value1",
				},
				{
					Validator: metricsLabelValidator,
					Name:      "label_field",
					Want:      "tag1,tag2",
					NotFound:  "tag1",
				},
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			m := &metricLabelLogValidator{
				ExpectLabelNum:   tt.fields.ExpectLabelNum,
				ExpectLabelNames: tt.fields.ExpectLabelNames,
			}
			m.expectKeys = strings.Join(m.ExpectLabelNames, ",")
			gotReports := m.validate(tt.args.content)
			if !reflect.DeepEqual(gotReports, tt.wantReports) {
				t.Errorf("validate() = %v, want %v", gotReports, tt.wantReports)
			}
		})
	}
}
