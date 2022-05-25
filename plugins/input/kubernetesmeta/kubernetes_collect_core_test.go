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

package kubernetesmeta

import "testing"

func TestExtractPodWorkloadName(t *testing.T) {
	tests := []struct {
		name string
		args string
		want string
	}{
		{
			"deployment1",
			"kube-state-metrics-86679c945-5rmck",
			"kube-state-metrics",
		},
		{
			"deployment2",
			"kube-state-metrics-86679c9454-5rmck",
			"kube-state-metrics",
		},
		{
			"set",
			"kube-state-metrics-5rmck",
			"kube-state-metrics",
		},
		{
			"pod",
			"kube-state-metrics",
			"kube-state-metrics",
		},
		{
			"empty",
			"",
			"",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := ExtractPodWorkloadName(tt.args); got != tt.want {
				t.Errorf("ExtractPodWorkloadName() = %v, want %v", got, tt.want)
			}
		})
	}
}
