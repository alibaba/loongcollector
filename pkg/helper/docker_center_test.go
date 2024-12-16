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

package helper

import (
	"context"
	"errors"
	"os"
	"sync"
	"testing"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"
	"github.com/docker/docker/api/types/events"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/mock"
	"github.com/stretchr/testify/require"
)

var hostFileContent1 = `
192.168.5.3	8be13ee0dd9e
127.0.0.1  	  localhost
::1     localhost ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters
`

var hostFileContent2 = `
# Kubernetes-managed hosts file.
127.0.0.1	localhost
::1	localhost ip6-localhost ip6-loopback
fe00::0	ip6-localnet
fe00::0	ip6-mcastprefix
fe00::1	ip6-allnodes
fe00::2	ip6-allrouters
172.20.4.5	nginx-5fd7568b67-4sh8c
`

func resetDockerCenter() {
	dockerCenterInstance = nil
	onceDocker = sync.Once{}

}

func TestGetIpByHost_1(t *testing.T) {
	hostFileName := "./tmp_TestGetIpByHost.txt"
	os.WriteFile(hostFileName, []byte(hostFileContent1), 0x777)
	ip := getIPByHosts(hostFileName, "8be13ee0dd9e")
	if ip != "192.168.5.3" {
		t.Errorf("GetIpByHosts = %v, want %v", ip, "192.168.5.3")
	}
	os.Remove(hostFileName)
}

func TestGetIpByHost_2(t *testing.T) {
	hostFileName := "./tmp_TestGetIpByHost.txt"
	os.WriteFile(hostFileName, []byte(hostFileContent2), 0x777)
	ip := getIPByHosts(hostFileName, "nginx-5fd7568b67-4sh8c")
	if ip != "172.20.4.5" {
		t.Errorf("GetIpByHosts = %v, want %v", ip, "172.20.4.5")
	}
	os.Remove(hostFileName)
}

func TestGetAllAcceptedInfoV2(t *testing.T) {
	resetDockerCenter()
	dc := getDockerCenterInstance()

	newContainer := func(id string) *DockerInfoDetail {
		return dc.CreateInfoDetail(types.ContainerJSON{
			ContainerJSONBase: &types.ContainerJSONBase{
				ID:    id,
				Name:  id,
				State: &types.ContainerState{},
			},
			Config: &container.Config{
				Env: make([]string, 0),
			},
		}, "", false)
	}

	fullList := make(map[string]bool)
	matchList := make(map[string]*DockerInfoDetail)

	// Init.
	{
		dc.updateContainers(map[string]*DockerInfoDetail{
			"c1": newContainer("c1"),
		})

		newCount, delCount, matchAddedList, matchDeletedList := dc.getAllAcceptedInfoV2(
			fullList,
			matchList,
			nil, nil, nil, nil, nil, nil, nil, nil, nil)
		require.Equal(t, len(fullList), 1)
		require.Equal(t, len(matchList), 1)
		require.True(t, fullList["c1"])
		require.True(t, matchList["c1"] != nil)
		require.Equal(t, newCount, 1)
		require.Equal(t, delCount, 0)
		require.Equal(t, len(matchAddedList), 0)
		require.Equal(t, len(matchDeletedList), 0)
	}

	// New container.
	{
		dc.updateContainer("c2", newContainer("c2"))

		newCount, delCount, matchAddedList, matchDeletedList := dc.getAllAcceptedInfoV2(
			fullList,
			matchList,
			nil, nil, nil, nil, nil, nil, nil, nil, nil)
		require.Equal(t, len(fullList), 2)
		require.Equal(t, len(matchList), 2)
		require.True(t, fullList["c1"])
		require.True(t, fullList["c2"])
		require.True(t, matchList["c1"] != nil)
		require.True(t, matchList["c2"] != nil)
		require.Equal(t, newCount, 1)
		require.Equal(t, delCount, 0)
		require.Equal(t, len(matchAddedList), 1)
		require.Equal(t, len(matchDeletedList), 0)
	}

	// Delete container.
	{
		delete(dc.containerMap, "c1")

		newCount, delCount, matchAddedList, matchDeletedList := dc.getAllAcceptedInfoV2(
			fullList,
			matchList,
			nil, nil, nil, nil, nil, nil, nil, nil, nil)
		require.Equal(t, len(fullList), 1)
		require.Equal(t, len(matchList), 1)
		require.True(t, fullList["c2"])
		require.True(t, matchList["c2"] != nil)
		require.Equal(t, newCount, 0)
		require.Equal(t, delCount, 1)
		require.Equal(t, len(matchAddedList), 0)
		require.Equal(t, len(matchDeletedList), 1)
	}

	// New and Delete container.
	{
		dc.updateContainers(map[string]*DockerInfoDetail{
			"c3": newContainer("c3"),
			"c4": newContainer("c4"),
		})
		delete(dc.containerMap, "c2")

		newCount, delCount, matchAddedList, matchDeletedList := dc.getAllAcceptedInfoV2(
			fullList,
			matchList,
			nil, nil, nil, nil, nil, nil, nil, nil, nil)
		require.Equal(t, len(fullList), 2)
		require.Equal(t, len(matchList), 2)
		require.True(t, fullList["c3"])
		require.True(t, fullList["c4"])
		require.True(t, matchList["c3"] != nil)
		require.True(t, matchList["c4"] != nil)
		require.Equal(t, newCount, 2)
		require.Equal(t, delCount, 1)
		require.Equal(t, len(matchAddedList), 2)
		require.Equal(t, len(matchDeletedList), 1)
	}

	// With unmatched filter.
	fullList = make(map[string]bool)
	matchList = make(map[string]*DockerInfoDetail)
	{
		newCount, delCount, matchAddedList, matchDeletedList := dc.getAllAcceptedInfoV2(
			fullList,
			matchList,
			map[string]string{
				"label": "label",
			},
			nil, nil, nil, nil, nil, nil, nil, nil)
		require.Equal(t, len(fullList), 2)
		require.Equal(t, len(matchList), 0)
		require.True(t, fullList["c3"])
		require.True(t, fullList["c4"])
		require.Equal(t, newCount, 0)
		require.Equal(t, delCount, 0)
		require.Equal(t, len(matchAddedList), 0)
		require.Equal(t, len(matchDeletedList), 0)
	}

	// Delete unmatched container.
	{
		delete(dc.containerMap, "c3")

		newCount, delCount, matchAddedList, matchDeletedList := dc.getAllAcceptedInfoV2(
			fullList,
			matchList,
			map[string]string{
				"label": "label",
			},
			nil, nil, nil, nil, nil, nil, nil, nil)
		require.Equal(t, len(fullList), 1)
		require.Equal(t, len(matchList), 0)
		require.True(t, fullList["c4"])
		require.Equal(t, newCount, 0)
		require.Equal(t, delCount, 0)
		require.Equal(t, len(matchAddedList), 0)
		require.Equal(t, len(matchDeletedList), 0)
	}
}

func TestK8SInfo_IsMatch(t *testing.T) {
	type fields struct {
		Namespace       string
		Pod             string
		ContainerName   string
		Labels          map[string]string
		PausedContainer bool
	}
	type args struct {
		filter *K8SFilter
	}
	filter := func(ns, pod, container string, includeK8sLabels, excludeK8sLabels map[string]string) *K8SFilter {
		filter, _ := CreateK8SFilter(ns, pod, container, includeK8sLabels, excludeK8sLabels)
		return filter
	}
	tests := []struct {
		name   string
		fields fields
		args   args
		want   bool
	}{
		{
			name: "namespaceMatch",
			fields: fields{
				Namespace:     "ns",
				Pod:           "pod",
				ContainerName: "container",
				Labels: map[string]string{
					"a": "b",
				},
				PausedContainer: false,
			},
			args: args{
				filter: filter("^ns$", "", "", nil, nil),
			},
			want: true,
		},
		{
			name: "podMatch",
			fields: fields{
				Namespace:     "ns",
				Pod:           "pod",
				ContainerName: "container",
				Labels: map[string]string{
					"a": "b",
				},
				PausedContainer: false,
			},
			args: args{
				filter: filter("", "^pod$", "", nil, nil),
			},
			want: true,
		},
		{
			name: "containerMatch",
			fields: fields{
				Namespace:     "ns",
				Pod:           "pod",
				ContainerName: "container",
				Labels: map[string]string{
					"a": "b",
				},
				PausedContainer: false,
			},
			args: args{
				filter: filter("", "", "^container$", nil, nil),
			},
			want: true,
		},
		{
			name: "includeLabelMatch",
			fields: fields{
				Namespace:     "ns",
				Pod:           "pod",
				ContainerName: "container",
				Labels: map[string]string{
					"a": "b",
					"c": "d",
				},
				PausedContainer: false,
			},
			args: args{
				filter: filter("^ns$", "", "", map[string]string{
					"a": "b",
					"e": "f",
				}, nil),
			},
			want: true,
		},
		{
			name: "excludeLabelMatch",
			fields: fields{
				Namespace:     "ns",
				Pod:           "pod",
				ContainerName: "container",
				Labels: map[string]string{
					"a": "b",
					"c": "d",
				},
				PausedContainer: false,
			},
			args: args{
				filter: filter("^ns$", "", "", nil, map[string]string{
					"a": "b",
				}),
			},
			want: false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			info := &K8SInfo{
				Namespace:       tt.fields.Namespace,
				Pod:             tt.fields.Pod,
				ContainerName:   tt.fields.ContainerName,
				Labels:          tt.fields.Labels,
				PausedContainer: tt.fields.PausedContainer,
			}
			assert.Equalf(t, tt.want, info.IsMatch(tt.args.filter), "IsMatch(%v)", tt.args.filter)
		})
	}
}

type DockerClientMock struct {
	mock.Mock
}

type ContainerHelperMock struct {
	mock.Mock
}

// Events 实现了 DockerClient 的 Events 方法
func (m *DockerClientMock) Events(ctx context.Context, options types.EventsOptions) (<-chan events.Message, <-chan error) {
	args := m.Called(ctx, options)
	return args.Get(0).(chan events.Message), args.Get(1).(chan error)
}

func (m *DockerClientMock) ImageInspectWithRaw(ctx context.Context, imageID string) (types.ImageInspect, []byte, error) {
	args := m.Called(ctx, imageID)
	return args.Get(0).(types.ImageInspect), args.Get(1).([]byte), args.Error(2)
}

func (m *DockerClientMock) ContainerInspect(ctx context.Context, containerID string) (types.ContainerJSON, error) {
	args := m.Called(ctx, containerID)
	return args.Get(0).(types.ContainerJSON), args.Error(1)
}
func (m *DockerClientMock) ContainerList(ctx context.Context, options types.ContainerListOptions) ([]types.Container, error) {
	args := m.Called(ctx, options)
	return args.Get(0).([]types.Container), args.Error(1)
}

func (m *ContainerHelperMock) ContainerProcessAlive(pid int) bool {
	args := m.Called(pid)
	return args.Get(0).(bool)
}

func TestDockerCenterEvents(t *testing.T) {
	dockerCenterInstance = &DockerCenter{}
	dockerCenterInstance.imageCache = make(map[string]string)
	dockerCenterInstance.containerMap = make(map[string]*DockerInfoDetail)

	mockClient := DockerClientMock{}
	dockerCenterInstance.client = &mockClient

	containerHelper := ContainerHelperMock{}

	// 创建一个模拟的事件通道
	eventChan := make(chan events.Message, 1)
	errChan := make(chan error, 1)

	mockClient.On("Events", mock.Anything, mock.Anything).Return(eventChan, errChan)

	go dockerCenterInstance.eventListener()

	containerHelper.On("ContainerProcessAlive", mock.Anything).Return(false).Once()
	mockClient.On("ContainerInspect", mock.Anything, "event1").Return(types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:    "event1",
			Name:  "name1",
			State: &types.ContainerState{},
		},
		Config: &container.Config{
			Env: make([]string, 0),
		},
	}, nil).Once()

	eventChan <- events.Message{ID: "event1", Status: "rename"}

	time.Sleep(5 * time.Second)
	containerLen := len(dockerCenterInstance.containerMap)
	assert.Equal(t, 1, containerLen)

	containerHelper.On("ContainerProcessAlive", mock.Anything).Return(true).Once()
	mockClient.On("ContainerInspect", mock.Anything, "event1").Return(types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:    "event1",
			Name:  "start",
			State: &types.ContainerState{},
		},
		Config: &container.Config{
			Env: make([]string, 0),
		},
	}, nil).Once()
	eventChan <- events.Message{ID: "event1", Status: "start"}

	time.Sleep(5 * time.Second)
	// 设置期望
	close(eventChan)

	containerLen = len(dockerCenterInstance.containerMap)
	assert.Equal(t, 1, containerLen)
}

func TestDockerCenterFetchAll(t *testing.T) {
	dockerCenterInstance = &DockerCenter{}
	dockerCenterInstance.imageCache = make(map[string]string)
	dockerCenterInstance.containerMap = make(map[string]*DockerInfoDetail)

	mockClient := DockerClientMock{}
	dockerCenterInstance.client = &mockClient

	containerHelper := ContainerHelperMock{}

	mockContainerListResult := []types.Container{
		{ID: "id1"},
		{ID: "id2"},
		{ID: "id3"},
	}

	containerHelper.On("ContainerProcessAlive", mock.Anything).Return(true)

	mockClient.On("ContainerList", mock.Anything, mock.Anything).Return(mockContainerListResult, nil).Once()

	mockClient.On("ContainerInspect", mock.Anything, "id1").Return(types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:    "id1",
			Name:  "event_name1",
			State: &types.ContainerState{},
		},
		Config: &container.Config{
			Env: make([]string, 0),
		},
	}, nil).Once()
	mockClient.On("ContainerInspect", mock.Anything, "id2").Return(types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:    "id2",
			Name:  "event_name2",
			State: &types.ContainerState{},
		},
		Config: &container.Config{
			Env: make([]string, 0),
		},
	}, nil).Once()
	// one failed inspect
	mockClient.On("ContainerInspect", mock.Anything, "id3").Return(types.ContainerJSON{}, errors.New("id3 not exist")).Times(3)

	err := dockerCenterInstance.fetchAll()
	assert.Nil(t, err)

	containerLen := len(dockerCenterInstance.containerMap)
	assert.Equal(t, 2, containerLen)

	mockContainerListResult2 := []types.Container{
		{ID: "id4"},
		{ID: "id5"},
	}

	mockClient.On("ContainerList", mock.Anything, mock.Anything).Return(mockContainerListResult2, nil).Once()

	mockClient.On("ContainerInspect", mock.Anything, "id4").Return(types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:    "id4",
			Name:  "event_name4",
			State: &types.ContainerState{},
		},
		Config: &container.Config{
			Env: make([]string, 0),
		},
	}, nil).Once()

	mockClient.On("ContainerInspect", mock.Anything, "id5").Return(types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:    "id5",
			Name:  "event_name5",
			State: &types.ContainerState{},
		},
		Config: &container.Config{
			Env: make([]string, 0),
		},
	}, nil).Once()

	err = dockerCenterInstance.fetchAll()
	assert.Nil(t, err)

	containerLen = len(dockerCenterInstance.containerMap)
	assert.Equal(t, 4, containerLen)
}

func TestDockerCenterFetchAllAndOne(t *testing.T) {
	dockerCenterInstance = &DockerCenter{}
	dockerCenterInstance.imageCache = make(map[string]string)
	dockerCenterInstance.containerMap = make(map[string]*DockerInfoDetail)

	mockClient := DockerClientMock{}
	dockerCenterInstance.client = &mockClient

	containerHelper := ContainerHelperMock{}

	mockContainerListResult := []types.Container{
		{ID: "id1"},
		{ID: "id2"},
	}

	mockClient.On("ContainerList", mock.Anything, mock.Anything).Return(mockContainerListResult, nil)

	mockClient.On("ContainerInspect", mock.Anything, "id1").Return(types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:    "id1",
			Name:  "event_name1",
			State: &types.ContainerState{},
		},
		Config: &container.Config{
			Env: make([]string, 0),
		},
	}, nil)
	mockClient.On("ContainerInspect", mock.Anything, "id2").Return(types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:    "id2",
			Name:  "event_name2",
			State: &types.ContainerState{},
		},
		Config: &container.Config{
			Env: make([]string, 0),
		},
	}, nil)

	containerHelper.On("ContainerProcessAlive", mock.Anything).Return(true).Times(2)

	err := dockerCenterInstance.fetchAll()
	assert.Nil(t, err)

	dockerCenterInstance.markRemove("id1")
	dockerCenterInstance.markRemove("id2")

	containerHelper.On("ContainerProcessAlive", mock.Anything).Return(false).Times(2)
	err = dockerCenterInstance.fetchAll()
	assert.Nil(t, err)

	containerLen := len(dockerCenterInstance.containerMap)
	assert.Equal(t, 2, containerLen)

	for _, container := range dockerCenterInstance.containerMap {
		assert.Equal(t, true, container.deleteFlag)
	}
}
