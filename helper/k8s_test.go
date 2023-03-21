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

package helper

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestExtractPodWorkload(t *testing.T) {
	type args struct {
		name string
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "pod",
			args: args{
				name: "kube-state-metrics-86679c9454-5rmck",
			},
			want: "kube-state-metrics",
		},

		{
			name: "set",
			args: args{
				name: "kube-state-metrics-5rmck",
			},
			want: "kube-state-metrics",
		},
		{
			name: "statefulset",
			args: args{
				name: "kube-state-metrics-0",
			},
			want: "kube-state-metrics",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			assert.Equalf(t, tt.want, ExtractPodWorkload(tt.args.name), "ExtractPodWorkload(%v)", tt.args.name)
		})
	}
}

func TestExtractStatefulSetNum(t *testing.T) {
	num := ExtractStatefulSetNum("kube-state-metrics-1")
	assert.Equal(t, num, 1)
	num = ExtractStatefulSetNum("kube-state-metrics-1-1")
	assert.Equal(t, num, 1)
}
