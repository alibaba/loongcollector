// Copyright 2026 iLogtail Authors
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

package pluginmanager

import (
	"encoding/json"
	"testing"

	"github.com/moby/moby/api/types/container"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
)

func TestConvertDockerInfosHandlesNilConfig(t *testing.T) {
	info := &containercenter.DockerInfoDetail{
		ContainerInfo: container.InspectResponse{
			ID:    "nil-config",
			Name:  "nil-config",
			State: &container.State{Status: containercenter.ContainerStatusRunning},
		},
	}

	var commands []ContainerInfoCmd
	require.NotPanics(t, func() {
		convertDockerInfos(info, &commands)
	})
	require.Len(t, commands, 1)
	assert.Empty(t, commands[0].Env)
	assert.Empty(t, commands[0].ContainerLabels)
}

func TestConvertDockerInfosHandlesNilState(t *testing.T) {
	info := &containercenter.DockerInfoDetail{
		ContainerInfo: container.InspectResponse{
			ID:     "nil-state",
			Name:   "nil-state",
			Config: &container.Config{},
		},
	}

	var commands []ContainerInfoCmd
	require.NotPanics(t, func() {
		convertDockerInfos(info, &commands)
	})
	require.Len(t, commands, 1)
	assert.Empty(t, commands[0].Status)
	assert.False(t, commands[0].Stopped)
}

func TestConvertDockerInfosFields(t *testing.T) {
	info := newContainersAPIDockerInfo(
		"container-fields",
		containercenter.ContainerStatusRunning,
		101,
		[]string{"PLAIN=value", "WITH_EQUALS=left=middle=right", "INVALID"},
	)
	info.DefaultRootPath = "/var/lib/docker/overlay2/upper"
	info.ContainerInfo.Mounts = []container.MountPoint{
		{Source: "/host/data", Destination: "/data"},
		{Source: "/host/log", Destination: "/var/log"},
	}

	var commands []ContainerInfoCmd
	convertDockerInfos(info, &commands)

	require.Len(t, commands, 1)
	command := commands[0]
	assert.Equal(t, "value", command.Env["PLAIN"])
	assert.Equal(t, "left=middle=right", command.Env["WITH_EQUALS"])
	assert.NotContains(t, command.Env, "INVALID")
	assert.Equal(t, "/var/lib/docker/overlay2/upper", command.UpperDir)
	assert.Equal(t, info.StdoutPath, command.LogPath)
	assert.Equal(t, []Mount{
		{Source: "/host/data", Destination: "/data"},
		{Source: "/host/log", Destination: "/var/log"},
	}, command.Mounts)
	assert.NotEmpty(t, command.MetadataHash)
	assert.Equal(t, info.MetadataHash(), command.MetadataHash)
}

func TestConvertDockerInfosStoppedStatuses(t *testing.T) {
	tests := []struct {
		status  container.ContainerState
		stopped bool
	}{
		{status: containercenter.ContainerStatusRunning, stopped: false},
		{status: "exited", stopped: true},
		{status: "dead", stopped: true},
		{status: "removing", stopped: true},
	}

	for _, test := range tests {
		t.Run(string(test.status), func(t *testing.T) {
			info := newContainersAPIDockerInfo("container-"+string(test.status), test.status, 1, nil)
			var commands []ContainerInfoCmd

			convertDockerInfos(info, &commands)

			require.Len(t, commands, 1)
			assert.Equal(t, string(test.status), commands[0].Status)
			assert.Equal(t, test.stopped, commands[0].Stopped)
		})
	}
}

func TestGetAllContainers(t *testing.T) {
	resetContainersAPIGlobals(t)
	info := newContainersAPIDockerInfo("all-container", containercenter.ContainerStatusRunning, 11, []string{"KEY=value"})
	containercenter.GetContainerMap()[info.ContainerInfo.ID] = info

	raw := GetAllContainers()

	var result AllCmd
	require.NoError(t, json.Unmarshal([]byte(raw), &result))
	require.Len(t, result.All, 1)
	assert.Equal(t, info.ContainerInfo.ID, result.All[0].ID)
	assert.Equal(t, info.MetadataHash(), result.All[0].MetadataHash)
	assert.Equal(t, map[string]string{info.ContainerInfo.ID: info.MetadataHash()}, caCachedFullList)
}

func TestGetDiffContainersUpdateDeleteAndStop(t *testing.T) {
	t.Run("update", func(t *testing.T) {
		resetContainersAPIGlobals(t)
		original := newContainersAPIDockerInfo("updated-container", containercenter.ContainerStatusRunning, 1, nil)
		updated := newContainersAPIDockerInfo("updated-container", containercenter.ContainerStatusRunning, 2, nil)
		containercenter.GetContainerMap()[updated.ContainerInfo.ID] = updated
		caCachedFullList = map[string]string{original.ContainerInfo.ID: original.MetadataHash()}

		diff := readContainerDiff(t)

		require.Len(t, diff.Update, 1)
		assert.Equal(t, updated.ContainerInfo.ID, diff.Update[0].ID)
		assert.Equal(t, updated.MetadataHash(), diff.Update[0].MetadataHash)
		assert.Empty(t, diff.Delete)
		assert.Empty(t, diff.Stop)
	})

	t.Run("delete", func(t *testing.T) {
		resetContainersAPIGlobals(t)
		caCachedFullList = map[string]string{"deleted-container": "old-hash"}

		diff := readContainerDiff(t)

		assert.Empty(t, diff.Update)
		assert.Equal(t, []string{"deleted-container"}, diff.Delete)
		assert.Empty(t, diff.Stop)
	})

	t.Run("stop", func(t *testing.T) {
		resetContainersAPIGlobals(t)
		running := newContainersAPIDockerInfo("stopped-container", containercenter.ContainerStatusRunning, 1, nil)
		stopped := newContainersAPIDockerInfo("stopped-container", "exited", 1, nil)
		containercenter.GetContainerMap()[stopped.ContainerInfo.ID] = stopped
		caCachedFullList = map[string]string{running.ContainerInfo.ID: running.MetadataHash()}

		diff := readContainerDiff(t)

		assert.Equal(t, []string{"stopped-container"}, diff.Stop)
		assert.Empty(t, diff.Delete)
	})
}

func readContainerDiff(t *testing.T) DiffCmd {
	t.Helper()
	lastUpdateTime = 0
	raw := GetDiffContainers()
	require.NotEmpty(t, raw)

	var diff DiffCmd
	require.NoError(t, json.Unmarshal([]byte(raw), &diff))
	return diff
}

func newContainersAPIDockerInfo(id string, status container.ContainerState, pid int, env []string) *containercenter.DockerInfoDetail {
	info := container.InspectResponse{
		ID:      id,
		Name:    "/" + id,
		LogPath: "/var/lib/docker/containers/" + id + "/" + id + "-json.log",
		State: &container.State{
			Status: status,
			Pid:    pid,
		},
		Config: &container.Config{
			Image:  "example/image:latest",
			Env:    env,
			Labels: map[string]string{"app": "test"},
		},
	}
	return containercenter.CreateContainerInfoDetail(info, *flags.LogConfigPrefix, false)
}

func resetContainersAPIGlobals(t *testing.T) {
	t.Helper()
	containerMap := containercenter.GetContainerMap()
	originalMap := make(map[string]*containercenter.DockerInfoDetail, len(containerMap))
	for id, info := range containerMap {
		originalMap[id] = info
		delete(containerMap, id)
	}

	originalCachedList := caCachedFullList
	originalLastUpdateTime := lastUpdateTime
	caCachedFullList = nil
	lastUpdateTime = 0

	t.Cleanup(func() {
		currentMap := containercenter.GetContainerMap()
		for id := range currentMap {
			delete(currentMap, id)
		}
		for id, info := range originalMap {
			currentMap[id] = info
		}
		caCachedFullList = originalCachedList
		lastUpdateTime = originalLastUpdateTime
	})
}
