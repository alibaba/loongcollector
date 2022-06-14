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
	"io/ioutil"
	"os"
	"sync"
	"testing"
	"time"

	docker "github.com/fsouza/go-dockerclient"
	"github.com/stretchr/testify/assert"
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
	ioutil.WriteFile(hostFileName, []byte(hostFileContent1), 0x777)
	ip := getIPByHosts(hostFileName, "8be13ee0dd9e")
	if ip != "192.168.5.3" {
		t.Errorf("GetIpByHosts = %v, want %v", ip, "192.168.5.3")
	}
	os.Remove(hostFileName)
}

func TestGetIpByHost_2(t *testing.T) {
	hostFileName := "./tmp_TestGetIpByHost.txt"
	ioutil.WriteFile(hostFileName, []byte(hostFileContent2), 0x777)
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
		return dc.CreateInfoDetail(&docker.Container{
			ID:     id,
			Name:   id,
			Config: &docker.Config{Env: make([]string, 0)},
		}, "", false)
	}

	fullList := make(map[string]bool)
	matchList := make(map[string]*DockerInfoDetail)

	// Init.
	{
		dc.updateContainers(map[string]*DockerInfoDetail{
			"c1": newContainer("c1"),
		})

		newCount, delCount := dc.getAllAcceptedInfoV2(
			fullList,
			matchList,
			nil, nil, nil, nil, nil, nil, nil, nil, nil)
		require.Equal(t, len(fullList), 1)
		require.Equal(t, len(matchList), 1)
		require.True(t, fullList["c1"])
		require.True(t, matchList["c1"] != nil)
		require.Equal(t, newCount, 1)
		require.Equal(t, delCount, 0)
	}

	// New container.
	{
		dc.updateContainer("c2", newContainer("c2"))

		newCount, delCount := dc.getAllAcceptedInfoV2(
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
	}

	// Delete container.
	{
		ContainerInfoDeletedTimeout = time.Second
		delete(dc.containerMap, "c1")

		newCount, delCount := dc.getAllAcceptedInfoV2(
			fullList,
			matchList,
			nil, nil, nil, nil, nil, nil, nil, nil, nil)
		require.Equal(t, len(fullList), 1)
		require.Equal(t, len(matchList), 1)
		require.True(t, fullList["c2"])
		require.True(t, matchList["c2"] != nil)
		require.Equal(t, newCount, 0)
		require.Equal(t, delCount, 1)
	}

	// New and Delete container.
	{
		ContainerInfoDeletedTimeout = time.Second
		dc.updateContainers(map[string]*DockerInfoDetail{
			"c3": newContainer("c3"),
			"c4": newContainer("c4"),
		})
		delete(dc.containerMap, "c2")

		newCount, delCount := dc.getAllAcceptedInfoV2(
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
	}

	// With unmatched filter.
	fullList = make(map[string]bool)
	matchList = make(map[string]*DockerInfoDetail)
	{
		newCount, delCount := dc.getAllAcceptedInfoV2(
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
	}

	// Delete unmatched container.
	{
		ContainerInfoDeletedTimeout = time.Second
		delete(dc.containerMap, "c3")

		newCount, delCount := dc.getAllAcceptedInfoV2(
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
