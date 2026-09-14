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

//go:build linux
// +build linux

package containercenter

import (
	"context"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/moby/moby/api/types/container"
	runtimespec "github.com/opencontainers/runtime-spec/specs-go"

	"github.com/stretchr/testify/require"
)

func TestLookupContainerRootfsAbsDir(t *testing.T) {
	crirt := &CRIRuntimeWrapper{
		containerCenter: nil,
		client:          nil,
		runtimeInfo:     CriVersionInfo{},
		containers:      make(map[string]*innerContainerInfo),
		stopCh:          make(<-chan struct{}),
		rootfsCache:     make(map[string]string),
	}

	container := container.InspectResponse{
		ID: "1234567890abcde",
	}
	dir := crirt.lookupContainerRootfsAbsDir(container)
	require.Equal(t, dir, "")
}

func TestContainerShouldMarkRemove(t *testing.T) {
	crirt := &CRIRuntimeWrapper{
		containerCenter: nil,
		client:          nil,
		runtimeInfo:     CriVersionInfo{},
		containers:      make(map[string]*innerContainerInfo),
		stopCh:          make(<-chan struct{}),
		rootfsCache:     make(map[string]string),
	}

	tests := []struct {
		name                             string
		forceReleaseDeletedFileFDTimeout int // in seconds, -1 means disabled
		containerStatus                  string
		containerState                   CriContainerState
		expectedResult                   bool
	}{
		{
			name:                             "ForceRelease enabled (0) with exited status",
			forceReleaseDeletedFileFDTimeout: 0,
			containerStatus:                  ContainerStatusExited,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   true,
		},
		{
			name:                             "ForceRelease enabled (0) with running status",
			forceReleaseDeletedFileFDTimeout: 0,
			containerStatus:                  ContainerStatusRunning,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   false,
		},
		{
			name:                             "ForceRelease enabled (positive) with exited status",
			forceReleaseDeletedFileFDTimeout: 120,
			containerStatus:                  ContainerStatusExited,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   true,
		},
		{
			name:                             "ForceRelease enabled (positive) with running status",
			forceReleaseDeletedFileFDTimeout: 120,
			containerStatus:                  ContainerStatusRunning,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   false,
		},
		{
			name:                             "ForceRelease disabled with exited state",
			forceReleaseDeletedFileFDTimeout: -1,
			containerStatus:                  ContainerStatusRunning,
			containerState:                   ContainerStateContainerExited,
			expectedResult:                   true,
		},
		{
			name:                             "ForceRelease disabled with running state",
			forceReleaseDeletedFileFDTimeout: -1,
			containerStatus:                  ContainerStatusRunning,
			containerState:                   ContainerStateContainerRunning,
			expectedResult:                   false,
		},
		{
			name:                             "ForceRelease disabled with created state",
			forceReleaseDeletedFileFDTimeout: -1,
			containerStatus:                  ContainerStatusExited,
			containerState:                   ContainerStateContainerCreated,
			expectedResult:                   false,
		},
		{
			name:                             "ForceRelease enabled with exited status and exited state",
			forceReleaseDeletedFileFDTimeout: 60,
			containerStatus:                  ContainerStatusExited,
			containerState:                   ContainerStateContainerExited,
			expectedResult:                   true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Save and restore the global variable
			originalTimeout := ForceReleaseDeletedFileFDTimeout
			defer func() {
				ForceReleaseDeletedFileFDTimeout = originalTimeout
			}()

			// Set the timeout for this test case
			ForceReleaseDeletedFileFDTimeout = time.Duration(tt.forceReleaseDeletedFileFDTimeout) * time.Second

			innerContainer := &innerContainerInfo{
				State:  tt.containerState,
				Pid:    12345,
				Name:   "test-container",
				Status: tt.containerStatus,
			}

			result := crirt.containerShouldMarkRemove(innerContainer)
			require.Equal(t, tt.expectedResult, result)
		})
	}
}

type fakeCRIAdapterRuntimeService struct {
	listContainersResp *CriListContainersResponse
	listContainersErr  error
	containerStatuses  map[string]*CriContainerStatusResponse
	containerStatusErr map[string]error
	listSandboxResp    *CriListPodSandboxResponse
	listSandboxErr     error
	sandboxStatuses    map[string]*CriPodSandboxStatusResponse
	sandboxStatusErr   map[string]error

	containerStatusCalls map[string]int
	sandboxStatusCalls   map[string]int
}

func (f *fakeCRIAdapterRuntimeService) Version(context.Context) (*CriVersionResponse, error) {
	return &CriVersionResponse{RuntimeName: "containerd", RuntimeAPIVersion: "v1"}, nil
}

func (f *fakeCRIAdapterRuntimeService) ListContainers(context.Context) (*CriListContainersResponse, error) {
	if f.listContainersErr != nil {
		return nil, f.listContainersErr
	}
	if f.listContainersResp == nil {
		return &CriListContainersResponse{}, nil
	}
	return f.listContainersResp, nil
}

func (f *fakeCRIAdapterRuntimeService) ContainerStatus(_ context.Context, containerID string, _ bool) (*CriContainerStatusResponse, error) {
	if f.containerStatusCalls == nil {
		f.containerStatusCalls = make(map[string]int)
	}
	f.containerStatusCalls[containerID]++
	if err := f.containerStatusErr[containerID]; err != nil {
		return nil, err
	}
	return f.containerStatuses[containerID], nil
}

func (f *fakeCRIAdapterRuntimeService) ListPodSandbox(context.Context) (*CriListPodSandboxResponse, error) {
	if f.listSandboxErr != nil {
		return nil, f.listSandboxErr
	}
	if f.listSandboxResp == nil {
		return &CriListPodSandboxResponse{}, nil
	}
	return f.listSandboxResp, nil
}

func (f *fakeCRIAdapterRuntimeService) PodSandboxStatus(_ context.Context, sandboxID string, _ bool) (*CriPodSandboxStatusResponse, error) {
	if f.sandboxStatusCalls == nil {
		f.sandboxStatusCalls = make(map[string]int)
	}
	f.sandboxStatusCalls[sandboxID]++
	if err := f.sandboxStatusErr[sandboxID]; err != nil {
		return nil, err
	}
	if status, ok := f.sandboxStatuses[sandboxID]; ok {
		return status, nil
	}
	return &CriPodSandboxStatusResponse{Status: &CriPodSandboxStatus{}}, nil
}

func newCRIAdapterTestWrapper(t *testing.T, service *fakeCRIAdapterRuntimeService) *CRIRuntimeWrapper {
	t.Helper()

	originalMountPath := DefaultLogtailMountPath
	originalCRIWrapper := criRuntimeWrapper
	DefaultLogtailMountPath = ""
	criRuntimeWrapper = nil
	t.Cleanup(func() {
		DefaultLogtailMountPath = originalMountPath
		criRuntimeWrapper = originalCRIWrapper
	})

	return &CRIRuntimeWrapper{
		containerCenter:  newTestContainerCenter(),
		client:           &RuntimeServiceClient{service: service},
		runtimeInfo:      CriVersionInfo{RuntimeName: "containerd", RuntimeAPIVersion: "v1"},
		containers:       make(map[string]*innerContainerInfo),
		containerHistory: make(map[string]bool),
		stopCh:           make(chan struct{}),
		rootfsCache:      make(map[string]string),
	}
}

func newCRIContainerStatusResponse(
	t *testing.T,
	id string,
	state CriContainerState,
	name string,
	image string,
	imageRef string,
	labels map[string]string,
	info containerdContainerInfo,
) *CriContainerStatusResponse {
	t.Helper()

	rawInfo, err := json.Marshal(info)
	require.NoError(t, err)
	var imageSpec *CriImageSpec
	if image != "" {
		imageSpec = &CriImageSpec{Image: image}
	}
	return &CriContainerStatusResponse{
		Status: &CriContainerStatus{
			ID:        id,
			Metadata:  &CriContainerMetadata{Name: name},
			State:     state,
			CreatedAt: time.Unix(1700000000, 123).UnixNano(),
			Image:     imageSpec,
			ImageRef:  imageRef,
			Labels:    labels,
			LogPath:   "/var/log/containers/" + id + ".log",
		},
		Info: map[string]string{"info": string(rawInfo)},
	}
}

func TestParseContainerInfo(t *testing.T) {
	raw := `{
		"sandboxID":"sandbox-1",
		"pid":123,
		"snapshotKey":"snapshot-1",
		"snapshotter":"overlayfs",
		"config":{"envs":[{"key":"FROM_CONFIG","value":"fallback"}]},
		"runtimeSpec":{
			"process":{"env":["A=B","C=D"]},
			"mounts":[{"destination":"/data","type":"bind","source":"/host/data"}]
		}
	}`

	info, err := parseContainerInfo(raw)

	require.NoError(t, err)
	require.Equal(t, "sandbox-1", info.SandboxID)
	require.Equal(t, uint32(123), info.Pid)
	require.Equal(t, "snapshot-1", info.SnapshotKey)
	require.Equal(t, "overlayfs", info.Snapshotter)
	require.NotNil(t, info.Config)
	require.Len(t, info.Config.Envs, 1)
	require.Equal(t, "FROM_CONFIG", info.Config.Envs[0].Key)
	require.NotNil(t, info.RuntimeSpec)
	require.NotNil(t, info.RuntimeSpec.Process)
	require.Equal(t, []string{"A=B", "C=D"}, info.RuntimeSpec.Process.Env)
	require.Len(t, info.RuntimeSpec.Mounts, 1)
	require.Equal(t, "/host/data", info.RuntimeSpec.Mounts[0].Source)

	_, err = parseContainerInfo(`{"pid":`)
	require.Error(t, err)
}

func TestCreateContainerInfo(t *testing.T) {
	t.Run("runtime spec env image and mounts", func(t *testing.T) {
		tempDir := t.TempDir()
		hostnamePath := filepath.Join(tempDir, "hostname")
		hostsPath := filepath.Join(tempDir, "hosts")
		require.NoError(t, os.WriteFile(hostnamePath, []byte("demo-host\n"), 0o600))
		require.NoError(t, os.WriteFile(hostsPath, []byte("127.0.0.1 demo-host\n"), 0o600))

		info := containerdContainerInfo{
			SandboxID:   "sandbox-1",
			Pid:         uint32(os.Getpid()),
			Snapshotter: "overlayfs",
			Config: &containerdContainerConfig{Envs: []*containerdKeyValue{
				{Key: "FROM_CONFIG", Value: "must-not-win"},
			}},
			RuntimeSpec: &runtimespec.Spec{
				Process: &runtimespec.Process{Env: []string{"FROM_RUNTIME=selected", "PLAIN=value"}},
				Mounts: []runtimespec.Mount{
					{Destination: "/etc/hostname", Type: "bind", Source: hostnamePath},
					{Destination: "/etc/hosts", Type: "bind", Source: hostsPath},
					{Destination: "/data", Type: "bind", Source: filepath.Join(tempDir, "data", "..", "data")},
				},
			},
		}
		status := newCRIContainerStatusResponse(
			t,
			"container-1",
			ContainerStateContainerRunning,
			"app",
			"repo/app:v1",
			"sha256:image-1",
			map[string]string{
				k8sPodNameLabel:      "pod-1",
				k8sPodNameSpaceLabel: "default",
				k8sPodUUIDLabel:      "pod-uid",
			},
			info,
		)
		service := &fakeCRIAdapterRuntimeService{
			containerStatuses: map[string]*CriContainerStatusResponse{"container-1": status},
		}
		wrapper := newCRIAdapterTestWrapper(t, service)

		detail, sandboxID, state, err := wrapper.createContainerInfo("container-1")

		require.NoError(t, err)
		require.Equal(t, "sandbox-1", sandboxID)
		require.Equal(t, ContainerStateContainerRunning, state)
		require.Equal(t, "container-1", detail.ContainerInfo.ID)
		require.Equal(t, "app", detail.ContainerInfo.Name)
		require.Equal(t, "repo/app:v1", detail.ContainerInfo.Config.Image)
		require.Equal(t, []string{"FROM_RUNTIME=selected", "PLAIN=value"}, detail.ContainerInfo.Config.Env)
		require.Equal(t, "demo-host", detail.ContainerInfo.Config.Hostname)
		require.Equal(t, hostnamePath, detail.ContainerInfo.HostnamePath)
		require.Equal(t, hostsPath, detail.ContainerInfo.HostsPath)
		require.Len(t, detail.ContainerInfo.Mounts, 3)
		require.Contains(t, detail.ContainerInfo.Mounts, container.MountPoint{
			Source:      filepath.Join(tempDir, "data"),
			Destination: "/data",
			Driver:      "bind",
		})
		require.Equal(t, ContainerStatusRunning, detail.Status())
		require.Equal(t, "containerd", detail.ContainerInfo.HostConfig.Runtime)
	})

	t.Run("config env fallback and image ref", func(t *testing.T) {
		status := newCRIContainerStatusResponse(
			t,
			"container-2",
			ContainerStateContainerExited,
			"fallback",
			"",
			"repo/fallback@sha256:digest",
			nil,
			containerdContainerInfo{
				SandboxID: "sandbox-2",
				Config: &containerdContainerConfig{Envs: []*containerdKeyValue{
					{Key: "FROM_CONFIG", Value: "selected"},
					{Key: "EMPTY", Value: ""},
				}},
			},
		)
		service := &fakeCRIAdapterRuntimeService{
			containerStatuses: map[string]*CriContainerStatusResponse{"container-2": status},
		}
		wrapper := newCRIAdapterTestWrapper(t, service)

		detail, sandboxID, state, err := wrapper.createContainerInfo("container-2")

		require.NoError(t, err)
		require.Equal(t, "sandbox-2", sandboxID)
		require.Equal(t, ContainerStateContainerExited, state)
		require.Equal(t, "repo/fallback@sha256:digest", detail.ContainerInfo.Config.Image)
		require.Equal(t, []string{"FROM_CONFIG=selected", "EMPTY="}, detail.ContainerInfo.Config.Env)
		require.Equal(t, ContainerStatusExited, detail.Status())
	})

	t.Run("missing info", func(t *testing.T) {
		service := &fakeCRIAdapterRuntimeService{
			containerStatuses: map[string]*CriContainerStatusResponse{
				"missing-info": {
					Status: &CriContainerStatus{
						ID:     "missing-info",
						State:  ContainerStateContainerExited,
						Labels: map[string]string{},
					},
				},
			},
		}
		wrapper := newCRIAdapterTestWrapper(t, service)

		detail, sandboxID, state, err := wrapper.createContainerInfo("missing-info")

		require.ErrorContains(t, err, "can not find container info")
		require.Nil(t, detail)
		require.Empty(t, sandboxID)
		require.Equal(t, ContainerStateContainerUnknown, state)
	})

	t.Run("malformed info is handled with zero-value runtime data", func(t *testing.T) {
		service := &fakeCRIAdapterRuntimeService{
			containerStatuses: map[string]*CriContainerStatusResponse{
				"malformed-info": {
					Status: &CriContainerStatus{
						ID:       "malformed-info",
						Metadata: &CriContainerMetadata{Name: "malformed"},
						State:    ContainerStateContainerExited,
						ImageRef: "repo/fallback@sha256:digest",
						Labels:   map[string]string{},
					},
					Info: map[string]string{"info": `{"pid":`},
				},
			},
		}
		wrapper := newCRIAdapterTestWrapper(t, service)

		detail, sandboxID, state, err := wrapper.createContainerInfo("malformed-info")

		require.NoError(t, err)
		require.NotNil(t, detail)
		require.Empty(t, sandboxID)
		require.Equal(t, ContainerStateContainerExited, state)
		require.Equal(t, "repo/fallback@sha256:digest", detail.ContainerInfo.Config.Image)
		require.Equal(t, ContainerStatusExited, detail.Status())
	})

	t.Run("container status error", func(t *testing.T) {
		service := &fakeCRIAdapterRuntimeService{
			containerStatusErr: map[string]error{"status-error": errors.New("status failed")},
		}
		wrapper := newCRIAdapterTestWrapper(t, service)

		detail, _, state, err := wrapper.createContainerInfo("status-error")

		require.ErrorContains(t, err, "status failed")
		require.Nil(t, detail)
		require.Equal(t, ContainerStateContainerUnknown, state)
	})
}

func TestWrapperK8sInfoByLabelsFiltersSandboxInternals(t *testing.T) {
	wrapper := newCRIAdapterTestWrapper(t, &fakeCRIAdapterRuntimeService{})
	detail := &DockerInfoDetail{
		K8SInfo: &K8SInfo{Labels: map[string]string{"existing": "value"}},
	}

	wrapper.wrapperK8sInfoByLabels(map[string]string{
		"team":                   "observability",
		"io.kubernetes.pod.name": "internal-pod",
		"annotation.secret":      "internal-annotation",
	}, detail)

	require.Equal(t, map[string]string{
		"existing": "value",
		"team":     "observability",
	}, detail.K8SInfo.Labels)
}

func TestCRIRuntimeWrapperFetchAll(t *testing.T) {
	t.Run("list containers error", func(t *testing.T) {
		wrapper := newCRIAdapterTestWrapper(t, &fakeCRIAdapterRuntimeService{
			listContainersErr: errors.New("list containers failed"),
		})

		require.ErrorContains(t, wrapper.fetchAll(), "list containers failed")
	})

	t.Run("list sandboxes error", func(t *testing.T) {
		wrapper := newCRIAdapterTestWrapper(t, &fakeCRIAdapterRuntimeService{
			listContainersResp: &CriListContainersResponse{},
			listSandboxErr:     errors.New("list sandboxes failed"),
		})

		require.ErrorContains(t, wrapper.fetchAll(), "list sandboxes failed")
	})

	t.Run("adds running removes exited and prunes history", func(t *testing.T) {
		runningInfo := containerdContainerInfo{
			SandboxID: "sandbox-1",
			Pid:       uint32(os.Getpid()),
			RuntimeSpec: &runtimespec.Spec{
				Process: &runtimespec.Process{Env: []string{"APP_ENV=test"}},
			},
		}
		exitedInfo := containerdContainerInfo{SandboxID: "sandbox-1"}
		service := &fakeCRIAdapterRuntimeService{
			listContainersResp: &CriListContainersResponse{Containers: []*CriContainer{
				{
					ID:           "running",
					PodSandboxID: "sandbox-1",
					Metadata:     &CriContainerMetadata{Name: "running"},
					State:        ContainerStateContainerRunning,
				},
				{
					ID:           "exited",
					PodSandboxID: "sandbox-1",
					Metadata:     &CriContainerMetadata{Name: "exited"},
					State:        ContainerStateContainerExited,
				},
				{
					ID:           "status-error",
					PodSandboxID: "sandbox-1",
					Metadata:     &CriContainerMetadata{Name: "status-error"},
					State:        ContainerStateContainerRunning,
				},
			}},
			containerStatuses: map[string]*CriContainerStatusResponse{
				"running": newCRIContainerStatusResponse(
					t,
					"running",
					ContainerStateContainerRunning,
					"app",
					"repo/app:v1",
					"sha256:running",
					map[string]string{
						k8sPodNameLabel:      "pod-1",
						k8sPodNameSpaceLabel: "default",
						k8sPodUUIDLabel:      "pod-uid",
					},
					runningInfo,
				),
				"exited": newCRIContainerStatusResponse(
					t,
					"exited",
					ContainerStateContainerExited,
					"exited",
					"repo/app:v1",
					"sha256:exited",
					nil,
					exitedInfo,
				),
			},
			containerStatusErr: map[string]error{"status-error": errors.New("status failed")},
			listSandboxResp: &CriListPodSandboxResponse{Items: []*CriPodSandbox{
				{
					ID: "sandbox-1",
					Labels: map[string]string{
						"team":                   "observability",
						"io.kubernetes.pod.name": "must-be-filtered",
						"annotation.internal":    "must-be-filtered",
					},
				},
			}},
		}
		wrapper := newCRIAdapterTestWrapper(t, service)
		staleDetail := &DockerInfoDetail{
			ContainerInfo: container.InspectResponse{
				ID:     "stale",
				State:  &container.State{Status: ContainerStatusRunning},
				Config: &container.Config{},
			},
			lastUpdateTime: time.Now(),
		}
		wrapper.containerCenter.containerMap["stale"] = staleDetail
		wrapper.containers["stale"] = &innerContainerInfo{
			State:  ContainerStateContainerRunning,
			Status: ContainerStatusRunning,
		}
		wrapper.containerHistory["stale"] = true
		wrapper.containerHistory["obsolete"] = true
		wrapper.containerHistory["exited"] = true

		require.NoError(t, wrapper.fetchAll())

		require.Contains(t, wrapper.containers, "running")
		require.NotContains(t, wrapper.containers, "exited")
		require.NotContains(t, wrapper.containers, "status-error")
		require.NotContains(t, wrapper.containers, "stale")
		require.True(t, wrapper.containerHistory["running"])
		require.True(t, wrapper.containerHistory["exited"])
		require.NotContains(t, wrapper.containerHistory, "status-error")
		require.NotContains(t, wrapper.containerHistory, "stale")
		require.NotContains(t, wrapper.containerHistory, "obsolete")

		runningDetail, ok := wrapper.containerCenter.containerMap["running"]
		require.True(t, ok)
		require.Equal(t, ContainerStatusRunning, runningDetail.Status())
		require.Equal(t, "observability", runningDetail.K8SInfo.Labels["team"])
		require.NotContains(t, runningDetail.K8SInfo.Labels, "io.kubernetes.pod.name")
		require.NotContains(t, runningDetail.K8SInfo.Labels, "annotation.internal")
		require.True(t, wrapper.containerCenter.containerMap["stale"].deleteFlag)
	})
}

func TestCRIRuntimeWrapperSyncContainers(t *testing.T) {
	service := &fakeCRIAdapterRuntimeService{
		listContainersResp: &CriListContainersResponse{Containers: []*CriContainer{
			{
				ID:           "new-running",
				PodSandboxID: "sandbox-1",
				Metadata:     &CriContainerMetadata{Name: "new-running"},
				State:        ContainerStateContainerRunning,
				CreatedAt:    200,
			},
			{
				ID:           "transitioned",
				PodSandboxID: "sandbox-1",
				Metadata:     &CriContainerMetadata{Name: "transitioned"},
				State:        ContainerStateContainerExited,
				CreatedAt:    200,
			},
			{
				ID:        "history-only",
				Metadata:  &CriContainerMetadata{Name: "history-only"},
				State:     ContainerStateContainerRunning,
				CreatedAt: 200,
			},
			{
				ID:        "created-state",
				Metadata:  &CriContainerMetadata{Name: "created-state"},
				State:     ContainerStateContainerCreated,
				CreatedAt: 200,
			},
			{
				ID:        "old-exited",
				Metadata:  &CriContainerMetadata{Name: "old-exited"},
				State:     ContainerStateContainerExited,
				CreatedAt: 99,
			},
			{
				ID:        "fetch-error",
				Metadata:  &CriContainerMetadata{Name: "fetch-error"},
				State:     ContainerStateContainerRunning,
				CreatedAt: 200,
			},
		}},
		containerStatuses: map[string]*CriContainerStatusResponse{
			"new-running": newCRIContainerStatusResponse(
				t,
				"new-running",
				ContainerStateContainerRunning,
				"new-running",
				"repo/app:v1",
				"sha256:new",
				nil,
				containerdContainerInfo{SandboxID: "sandbox-1", Pid: uint32(os.Getpid())},
			),
			"transitioned": newCRIContainerStatusResponse(
				t,
				"transitioned",
				ContainerStateContainerExited,
				"transitioned",
				"repo/app:v1",
				"sha256:transitioned",
				nil,
				containerdContainerInfo{SandboxID: "sandbox-1"},
			),
		},
		containerStatusErr: map[string]error{"fetch-error": errors.New("status failed")},
		sandboxStatuses: map[string]*CriPodSandboxStatusResponse{
			"sandbox-1": {
				Status: &CriPodSandboxStatus{Labels: map[string]string{"team": "observability"}},
			},
		},
	}
	wrapper := newCRIAdapterTestWrapper(t, service)
	wrapper.listContainerStartTime = 100
	wrapper.containerHistory["history-only"] = true
	wrapper.containerHistory["transitioned"] = true
	wrapper.containers["transitioned"] = &innerContainerInfo{
		State:  ContainerStateContainerRunning,
		Pid:    -1,
		Name:   "transitioned",
		Status: ContainerStatusRunning,
	}
	wrapper.containers["deleted"] = &innerContainerInfo{
		State:  ContainerStateContainerRunning,
		Pid:    os.Getpid(),
		Name:   "deleted",
		Status: ContainerStatusRunning,
	}
	wrapper.containerCenter.containerMap["transitioned"] = &DockerInfoDetail{
		ContainerInfo: container.InspectResponse{
			ID:     "transitioned",
			Name:   "transitioned",
			State:  &container.State{Status: ContainerStatusRunning},
			Config: &container.Config{},
		},
		lastUpdateTime: time.Now(),
	}
	wrapper.containerCenter.containerMap["deleted"] = &DockerInfoDetail{
		ContainerInfo: container.InspectResponse{
			ID:     "deleted",
			Name:   "deleted",
			State:  &container.State{Status: ContainerStatusRunning},
			Config: &container.Config{},
		},
		lastUpdateTime: time.Now(),
	}

	require.NoError(t, wrapper.syncContainers())

	require.Contains(t, wrapper.containers, "new-running")
	require.True(t, wrapper.containerHistory["new-running"])
	require.Equal(t, 1, service.containerStatusCalls["new-running"])
	require.Equal(t, 1, service.containerStatusCalls["transitioned"])
	require.Zero(t, service.containerStatusCalls["history-only"])
	require.Zero(t, service.containerStatusCalls["created-state"])
	require.Zero(t, service.containerStatusCalls["old-exited"])
	require.Equal(t, 1, service.containerStatusCalls["fetch-error"])
	require.NotContains(t, wrapper.containers, "fetch-error")
	require.NotContains(t, wrapper.containerHistory, "fetch-error")
	require.NotContains(t, wrapper.containers, "transitioned")
	require.True(t, wrapper.containerCenter.containerMap["transitioned"].deleteFlag)
	require.NotContains(t, wrapper.containers, "deleted")
	require.True(t, wrapper.containerCenter.containerMap["deleted"].deleteFlag)
	require.Equal(t, "observability", wrapper.containerCenter.containerMap["new-running"].K8SInfo.Labels["team"])
}

func TestCRIRuntimeWrapperSweepCache(t *testing.T) {
	wrapper := newCRIAdapterTestWrapper(t, &fakeCRIAdapterRuntimeService{})
	wrapper.containerCenter.containerMap["used"] = &DockerInfoDetail{}
	wrapper.rootfsCache["used"] = "/used/rootfs"
	wrapper.rootfsCache["obsolete"] = "/obsolete/rootfs"

	wrapper.sweepCache()

	require.Equal(t, map[string]string{"used": "/used/rootfs"}, wrapper.rootfsCache)
}
