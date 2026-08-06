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

package containercenter

import (
	"testing"

	"github.com/stretchr/testify/require"
)

func TestK8SInfoMerge(t *testing.T) {
	cases := []struct {
		name     string
		info     map[string]string
		other    map[string]string
		expected map[string]string
	}{
		{
			name:     "both empty stays nil",
			info:     nil,
			other:    nil,
			expected: nil,
		},
		{
			name:     "info empty takes other",
			info:     nil,
			other:    map[string]string{"a": "1", "b": "2"},
			expected: map[string]string{"a": "1", "b": "2"},
		},
		{
			name:     "other empty keeps info",
			info:     map[string]string{"a": "1", "b": "2"},
			other:    nil,
			expected: map[string]string{"a": "1", "b": "2"},
		},
		{
			name:     "disjoint keys are unioned",
			info:     map[string]string{"a": "1"},
			other:    map[string]string{"b": "2"},
			expected: map[string]string{"a": "1", "b": "2"},
		},
		{
			// The previous len-based merge dropped the smaller map's unique key.
			name:     "smaller side unique key is preserved",
			info:     map[string]string{"only": "me"},
			other:    map[string]string{"x": "1", "y": "2"},
			expected: map[string]string{"only": "me", "x": "1", "y": "2"},
		},
		{
			name:     "info value wins on overlap",
			info:     map[string]string{"k": "info", "a": "1"},
			other:    map[string]string{"k": "other", "b": "2"},
			expected: map[string]string{"k": "info", "a": "1", "b": "2"},
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			info := &K8SInfo{Labels: tc.info}
			other := &K8SInfo{Labels: tc.other}
			info.Merge(other)

			require.Equal(t, tc.expected, info.Labels)
			require.Equal(t, tc.expected, other.Labels)
		})
	}
}

// TestK8SInfoMergeNoAlias verifies the two K8SInfo objects do not share the
// same underlying map after Merge, so a later write to one does not leak into
// the other.
func TestK8SInfoMergeNoAlias(t *testing.T) {
	info := &K8SInfo{Labels: map[string]string{"a": "1"}}
	other := &K8SInfo{Labels: map[string]string{"b": "2"}}
	info.Merge(other)

	info.Labels["only-info"] = "x"
	require.NotContains(t, other.Labels, "only-info")

	other.Labels["only-other"] = "y"
	require.NotContains(t, info.Labels, "only-other")
}

// TestK8SInfoMergeNilAndSelf ensures nil operands and self-merge are no-ops and
// do not deadlock.
func TestK8SInfoMergeNilAndSelf(t *testing.T) {
	var nilInfo *K8SInfo
	nilInfo.Merge(&K8SInfo{})
	(&K8SInfo{}).Merge(nil)

	self := &K8SInfo{Labels: map[string]string{"a": "1"}}
	self.Merge(self)
	require.Equal(t, map[string]string{"a": "1"}, self.Labels)
}

// TestMergeK8sInfoSkipsDeletedContainer reproduces the in-place pod rebuild case:
// a stale container (deleteFlag) still lingers in the container map alongside the
// freshly created pause and business containers of the same pod. The stale labels
// must not leak onto the fresh containers.
func TestMergeK8sInfoSkipsDeletedContainer(t *testing.T) {
	const ns, pod = "default", "app-0"

	// Old business container from the previous incarnation, already enriched with
	// stale label values and a label (test3) that no longer exists. Marked deleted.
	stale := &DockerInfoDetail{
		deleteFlag: true,
		K8SInfo: &K8SInfo{
			Namespace: ns, Pod: pod, ContainerName: "app",
			Labels: map[string]string{"test1": "old1", "test2": "old2", "test3": "old3"},
		},
	}
	// Fresh pause container after the rebuild, carrying the new label values.
	pause := &DockerInfoDetail{
		K8SInfo: &K8SInfo{
			Namespace: ns, Pod: pod, ContainerName: "POD", PausedContainer: true,
			Labels: map[string]string{"test1": "new1", "test2": "new2"},
		},
	}
	// Fresh business container, no labels yet.
	business := &DockerInfoDetail{
		K8SInfo: &K8SInfo{Namespace: ns, Pod: pod, ContainerName: "app"},
	}

	dc := &ContainerCenter{
		containerMap: map[string]*DockerInfoDetail{
			"stale-id":    stale,
			"pause-id":    pause,
			"business-id": business,
		},
	}
	dc.mergeK8sInfo()

	want := map[string]string{"test1": "new1", "test2": "new2"}
	require.Equal(t, want, business.K8SInfo.Labels, "fresh business must get new labels only")
	require.Equal(t, want, pause.K8SInfo.Labels, "pause labels must stay fresh")
	// The deleted container is untouched and never contributes its stale labels.
	require.Equal(t, map[string]string{"test1": "old1", "test2": "old2", "test3": "old3"}, stale.K8SInfo.Labels)
}
