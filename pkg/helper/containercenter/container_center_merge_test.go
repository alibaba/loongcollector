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
	"os"
	"path/filepath"
	"sync"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
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

// resetStaticContainerGlobals clears the package-level state used by the static
// container provider so a test can drive readStaticConfig deterministically
// without interference from other tests.
func resetStaticContainerGlobals() {
	loadStaticContainerOnce = sync.Once{}
	staticDockerContainerFile = ""
	staticDockerContainers = nil
	staticDockerContainerLastStat = helper.StateOS{}
	staticDockerContainerLastBody = ""
	staticDockerContainerError = nil
}

// TestReadStaticConfigRefreshesLabelsOnPodRebuild is the end-to-end regression for
// the production bug: with static container discovery, when a pod is rebuilt in
// place (business container gets a new id, pause keeps the pod-uid id) and the pod
// labels change, the freshly created business container must be enriched with the
// NEW label values, not the stale ones carried by the lingering old container.
//
// This exercises the real readStaticConfig ordering (markRemove before
// updateContainers/mergeK8sInfo). It is file-driven only: "changing the container
// id" is just editing container.json, so no real container runtime is needed.
func TestReadStaticConfigRefreshesLabelsOnPodRebuild(t *testing.T) {
	resetContainerCenter()
	resetStaticContainerGlobals()

	file := filepath.Join(t.TempDir(), "container.json")
	os.Setenv(staticContainerInfoPathEnvKey, file)
	defer os.Unsetenv(staticContainerInfoPathEnvKey)
	defer resetStaticContainerGlobals()

	// readStaticConfig uses the package-global containerCenterInstance; set it up
	// directly instead of getContainerCenterInstance() to avoid its background
	// discovery goroutine (which would race with our synchronous calls).
	containerCenterInstance = &ContainerCenter{
		containerHelper: &ContainerHelperWrapper{},
		imageCache:      make(map[string]string),
		containerMap:    make(map[string]*DockerInfoDetail),
	}

	// v1: pause(id=uid) with test1..test3 + business sandbox(id=8131), no labels.
	require.NoError(t, os.WriteFile(file, []byte(staticRebuildConfigV1), os.ModePerm))
	containerCenterInstance.readStaticConfig(true)

	oldSandbox := containerCenterInstance.containerMap["8131"]
	require.NotNil(t, oldSandbox)
	require.Equal(t, "111111", oldSandbox.K8SInfo.GetLabel("test1"), "v1: business enriched from pause")
	require.Equal(t, "311111", oldSandbox.K8SInfo.GetLabel("test3"))

	// Rebuild: change pause label values (test3 removed) and give the business
	// container a NEW id. Remove+rewrite so the file gets a new inode and the
	// change is detected reliably.
	require.NoError(t, os.Remove(file))
	require.NoError(t, os.WriteFile(file, []byte(staticRebuildConfigV2), os.ModePerm))
	containerCenterInstance.readStaticConfig(true)

	newSandbox := containerCenterInstance.containerMap["3b59"]
	require.NotNil(t, newSandbox, "new business container must be present")
	require.Equal(t, "111112", newSandbox.K8SInfo.GetLabel("test1"), "rebuilt business must get the NEW value, not stale 111111")
	require.Equal(t, "211112", newSandbox.K8SInfo.GetLabel("test2"))
	require.Empty(t, newSandbox.K8SInfo.GetLabel("test3"), "removed label must not linger")

	// The old business container is flagged for removal and must not be a live source.
	if old := containerCenterInstance.containerMap["8131"]; old != nil {
		require.True(t, old.deleteFlag, "old business container should be marked removed")
	}
}

const staticRebuildConfigV1 = `[
	{
		"ID": "uid",
		"Name": "POD",
		"Image": "pause:latest",
		"LogPath": "/var/log/pods/default_code-interpreter5_uid",
		"Labels": {
			"io.kubernetes.pod.name": "code-interpreter5",
			"io.kubernetes.pod.namespace": "default",
			"io.kubernetes.pod.uid": "uid",
			"test1": "111111",
			"test2": "211111",
			"test3": "311111"
		},
		"LogType": "json-file",
		"Created": "2026-08-06T19:41:32.228133866+08:00",
		"State": { "Pid": 999999999901, "Status": "running" }
	},
	{
		"ID": "8131",
		"Name": "sandbox",
		"Image": "alinux3:latest",
		"LogPath": "/var/log/pods/default_code-interpreter5_uid/sandbox/1.log",
		"Labels": {
			"io.kubernetes.container.name": "sandbox",
			"io.kubernetes.pod.name": "code-interpreter5",
			"io.kubernetes.pod.namespace": "default",
			"io.kubernetes.pod.uid": "uid"
		},
		"LogType": "json-file",
		"Created": "2026-08-06T19:41:32.228133866+08:00",
		"State": { "Pid": 999999999902, "Status": "running" }
	}
]`

const staticRebuildConfigV2 = `[
	{
		"ID": "uid",
		"Name": "POD",
		"Image": "pause:latest",
		"LogPath": "/var/log/pods/default_code-interpreter5_uid",
		"Labels": {
			"io.kubernetes.pod.name": "code-interpreter5",
			"io.kubernetes.pod.namespace": "default",
			"io.kubernetes.pod.uid": "uid",
			"test1": "111112",
			"test2": "211112"
		},
		"LogType": "json-file",
		"Created": "2026-08-06T19:41:32.228133866+08:00",
		"State": { "Pid": 999999999901, "Status": "running" }
	},
	{
		"ID": "3b59",
		"Name": "sandbox",
		"Image": "alinux3:x86-3.220822.1",
		"LogPath": "/var/log/pods/default_code-interpreter5_uid/sandbox/2.log",
		"Labels": {
			"io.kubernetes.container.name": "sandbox",
			"io.kubernetes.pod.name": "code-interpreter5",
			"io.kubernetes.pod.namespace": "default",
			"io.kubernetes.pod.uid": "uid"
		},
		"LogType": "json-file",
		"Created": "2026-08-06T19:55:44.506191037+08:00",
		"State": { "Pid": 999999999903, "Status": "running" }
	}
]`
