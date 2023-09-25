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

//go:build !windows
// +build !windows

package helper

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/logger"
)

func init() {
	logger.InitTestLogger(logger.OptionDebugLevel, logger.OptionOpenConsole)

}

var staticDockerConfig = `[
{
    "ID" : "123abc",
    "Name" : "xxx_info",
	"Created": "2022-09-16T12:55:14.930245185+08:00",
	"State": {
		"Pid": 999999999908,
		"Status": "running"
	},
    "HostName" : "app-online",
    "IP" : "192.168.1.1",
    "Image" : "centos:latest",
    "LogPath" : "/var/lib/docker/xxxx/0.log",
    "Labels" : {
		"a" : "b",
		"io.kubernetes.container.name": "node-exporter",
        "io.kubernetes.pod.name": "node-exporter-5q796",
        "io.kubernetes.pod.namespace": "arms-prom",
        "io.kubernetes.pod.uid": "dd43f0eb-1194-11ea-aa77-3e20fcedfa8f"
    },
	"LogType" : "json-file",
	"UpperDir" : "/apsarapangu/disk12/docker/overlay/b6ff04a15c7ec040b3ef0857cb091d1c74de27d4d5daf32884a842055e9fbb6d/upper",
    "Env" : {
		"x" : "y",
		"xxx" : "yyyy"
    },
    "Mounts" : [
        {
            "Source": "/home/admin/logs",
            "Destination" : "/home/admin/logs",
            "Driver" : "host"
        },
        {
            "Source": "/var/lib/docker",
            "Destination" : "/docker",
            "Driver" : "host"
        }
    ]
}
]
`

var staticDockerConfig2 = `[
{
    "ID" : "123abc-2",
    "Name" : "xxx_info",
	"Created": "2022-09-16T12:56:14.000000000+08:00",
	"State": {
		"Pid": 999999999909,
		"Status": "running"
	},
    "Image" : "centos:latest",
    "LogPath" : "/var/lib/docker/xxxx/1.log",
    "Labels" : {
		"a" : "b",
		"io.kubernetes.container.name": "node-exporter",
        "io.kubernetes.pod.name": "node-exporter-5q796",
        "io.kubernetes.pod.namespace": "arms-prom",
        "io.kubernetes.pod.uid": "dd43f0eb-1194-11ea-aa77-3e20fcedfa8f"
    },
	"LogType" : "json-file",
	"UpperDir" : "/apsarapangu/disk12/docker/overlay/b6ff04a15c7ec040b3ef0857cb091d1c74de27d4d5daf32884a842055e9fbb6d/upper",
    "Env" : {
		"x" : "y",
		"xxx" : "yyyy"
    },
    "Mounts" : [
        {
            "Source": "/home/admin/logs",
            "Destination" : "/home/admin/logs",
            "Driver" : "host"
        },
        {
            "Source": "/var/lib/docker",
            "Destination" : "/docker",
            "Driver" : "host"
        }
    ]
}
]
`

var staticECIConfig = `[
	{
			"ID": "111",
			"Name": "container1",
			"Image": "image1",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container1/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container1",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"KUBERNETES_PORT": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP_ADDR": "192.168.0.1",
					"KUBERNETES_PORT_443_TCP_PORT": "443",
					"KUBERNETES_PORT_443_TCP_PROTO": "tcp",
					"KUBERNETES_SERVICE_HOST": "10.244.197.20",
					"KUBERNETES_SERVICE_PORT": "6443",
					"KUBERNETES_SERVICE_PORT_HTTPS": "443"
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/111/rootfs",
			"Created": "2023-06-01T10:09:32.336427588+08:00",
			"State": {
					"Pid": 1001,
					"Status": "running"
			}
	},
	{
			"ID": "222",
			"Name": "container2",
			"Image": "image2",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container2/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container2",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"KUBERNETES_PORT": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP_ADDR": "192.168.0.1",
					"KUBERNETES_PORT_443_TCP_PORT": "443",
					"KUBERNETES_PORT_443_TCP_PROTO": "tcp",
					"KUBERNETES_SERVICE_HOST": "10.244.197.20",
					"KUBERNETES_SERVICE_PORT": "6443",
					"KUBERNETES_SERVICE_PORT_HTTPS": "443"
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/222/rootfs",
			"Created": "2023-06-01T10:09:33.221371944+08:00",
			"State": {
					"Pid": 1102,
					"Status": "running"
			}
	},
	{
			"ID": "333",
			"Name": "container3",
			"Image": "image3",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container3/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container3",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"KUBERNETES_PORT": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP_ADDR": "192.168.0.1",
					"KUBERNETES_PORT_443_TCP_PORT": "443",
					"KUBERNETES_PORT_443_TCP_PROTO": "tcp",
					"KUBERNETES_SERVICE_HOST": "10.244.197.20",
					"KUBERNETES_SERVICE_PORT": "6443",
					"KUBERNETES_SERVICE_PORT_HTTPS": "443",
					"POD_NAME": "",
					"POD_NAMESPACE": "",
					"aliyun_log_crd_user_defined_id": "k8s-group-xxx"
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/333/rootfs",
			"Created": "2023-06-01T10:09:34.22551185+08:00",
			"State": {
					"Pid": 1181,
					"Status": "running"
			}
	},
	{
			"ID": "444",
			"Name": "container4",
			"Image": "image4",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container4/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container4",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"POD_NAME": "",
					"POD_NAMESPACE": ""
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/444/rootfs",
			"Created": "2023-06-01T10:10:08.442675217+08:00",
			"State": {
					"Pid": 1965,
					"Status": "running"
			}
	},
	{
			"ID": "555",
			"Name": "container5",
			"Image": "image5",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container5/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container5",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"POD_NAME": "",
					"POD_NAMESPACE": ""
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/555/rootfs",
			"Created": "2023-06-01T10:10:08.77858586+08:00",
			"State": {
					"Pid": 2007,
					"Status": "running"
			}
	},
	{
			"ID": "podUidTest",
			"Name": "POD",
			"Image": "pause:latest",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest",
			"Labels": {
					"app": "appTest",
					"attribute": "attTest",
					"deployment": "deploymentTest",
					"environment": "prod",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest",
					"log": "logTest",
					"pod-template-hash": "xxx"
			},
			"LogType": "json-file",
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/null/rootfs",
			"Created": "0001-01-01T00:00:00Z",
			"State": {
					"Pid": 0,
					"Status": ""
			}
	},
	{
			"ID": "666",
			"Name": "container7",
			"Image": "acr.baijia.com/uqun/container7:2022102001",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container7/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container7",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
				"POD_NAME": "",
				"POD_NAMESPACE": ""
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/666/rootfs",
			"Created": "2023-06-01T10:09:29.382815361+08:00",
			"State": {
					"Pid": 694,
					"Status": "running"
			}
	},
	{
			"ID": "777",
			"Name": "container8",
			"Image": "acr.baijia.com/uqun/container8:1.7.0",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container8/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container8",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
				"POD_NAME": "",
				"POD_NAMESPACE": ""
			},
			"Mounts": [
					{
							"Source": "/var/lib/kubelet/pods/podUidTest/volumes/kubernetes.io~empty-dir/container8-volume",
							"Destination": "/ArmsAgent",
							"Driver": ""
					}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/e916e0ba011ab5bf825ba24ffba7e3e5dedd2073e29222dd6238eb874cc27ddd/rootfs",
			"Created": "2023-06-01T10:09:30.213522454+08:00",
			"State": {
					"Pid": 872,
					"Status": "running"
			}
	}
]`

var staticECIConfig2 = `[
	{
			"ID": "111",
			"Name": "container1",
			"Image": "image1",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container1/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container1",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"KUBERNETES_PORT": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP_ADDR": "192.168.0.1",
					"KUBERNETES_PORT_443_TCP_PORT": "443",
					"KUBERNETES_PORT_443_TCP_PROTO": "tcp",
					"KUBERNETES_SERVICE_HOST": "10.244.197.20",
					"KUBERNETES_SERVICE_PORT": "6443",
					"KUBERNETES_SERVICE_PORT_HTTPS": "443"
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/111/rootfs",
			"Created": "2023-06-01T10:09:32.336427588+08:00",
			"State": {
					"Pid": 1001,
					"Status": "running"
			}
	},
	{
			"ID": "222",
			"Name": "container2",
			"Image": "image2",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container2/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container2",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"KUBERNETES_PORT": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP_ADDR": "192.168.0.1",
					"KUBERNETES_PORT_443_TCP_PORT": "443",
					"KUBERNETES_PORT_443_TCP_PROTO": "tcp",
					"KUBERNETES_SERVICE_HOST": "10.244.197.20",
					"KUBERNETES_SERVICE_PORT": "6443",
					"KUBERNETES_SERVICE_PORT_HTTPS": "443"
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/222/rootfs",
			"Created": "2023-06-01T10:09:33.221371944+08:00",
			"State": {
					"Pid": 1102,
					"Status": "running"
			}
	},
	{
			"ID": "333",
			"Name": "container3",
			"Image": "image3",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container3/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container3",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"KUBERNETES_PORT": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP": "tcp://192.168.0.1:443",
					"KUBERNETES_PORT_443_TCP_ADDR": "192.168.0.1",
					"KUBERNETES_PORT_443_TCP_PORT": "443",
					"KUBERNETES_PORT_443_TCP_PROTO": "tcp",
					"KUBERNETES_SERVICE_HOST": "10.244.197.20",
					"KUBERNETES_SERVICE_PORT": "6443",
					"KUBERNETES_SERVICE_PORT_HTTPS": "443",
					"POD_NAME": "",
					"POD_NAMESPACE": "",
					"aliyun_log_crd_user_defined_id": "k8s-group-xxx"
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/333/rootfs",
			"Created": "2023-06-01T10:09:34.22551185+08:00",
			"State": {
					"Pid": 1181,
					"Status": "running"
			}
	},
	{
			"ID": "444",
			"Name": "container4",
			"Image": "image4",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container4/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container4",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"POD_NAME": "",
					"POD_NAMESPACE": ""
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/444/rootfs",
			"Created": "2023-06-01T10:10:08.442675217+08:00",
			"State": {
					"Pid": 1965,
					"Status": "running"
			}
	},
	{
			"ID": "555",
			"Name": "container5",
			"Image": "image5",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container5/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container5",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
					"POD_NAME": "",
					"POD_NAMESPACE": ""
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/555/rootfs",
			"Created": "2023-06-01T10:10:08.77858586+08:00",
			"State": {
					"Pid": 2007,
					"Status": "running"
			}
	},
	{
			"ID": "podUidTest",
			"Name": "POD",
			"Image": "pause:latest",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest",
			"Labels": {
					"app": "appTest",
					"attribute": "attTest",
					"deployment": "deploymentTest",
					"environment": "prod",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest",
					"log": "logTest",
					"pod-template-hash": "xxx",
					"custom": "xxx"
			},
			"LogType": "json-file",
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/null/rootfs",
			"Created": "0001-01-01T00:00:00Z",
			"State": {
					"Pid": 0,
					"Status": ""
			}
	},
	{
			"ID": "666",
			"Name": "container7",
			"Image": "acr.baijia.com/uqun/container7:2022102001",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container7/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container7",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
				"POD_NAME": "",
				"POD_NAMESPACE": ""
			},
			"Mounts": [
				{
					"Source": "/home/admin/logs",
					"Destination" : "/home/admin/logs",
					"Driver" : "host"
				},
				{
					"Source": "/var/lib/docker",
					"Destination" : "/docker",
					"Driver" : "host"
				}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/666/rootfs",
			"Created": "2023-06-01T10:09:29.382815361+08:00",
			"State": {
					"Pid": 694,
					"Status": "running"
			}
	},
	{
			"ID": "777",
			"Name": "container8",
			"Image": "acr.baijia.com/uqun/container8:1.7.0",
			"LogPath": "/var/log/pods/prod_podTest_podUidTest/container8/0.log",
			"Labels": {
					"io.kubernetes.container.name": "container8",
					"io.kubernetes.pod.name": "podTest",
					"io.kubernetes.pod.namespace": "prod",
					"io.kubernetes.pod.uid": "podUidTest"
			},
			"LogType": "json-file",
			"Env": {
				"POD_NAME": "",
				"POD_NAMESPACE": ""
			},
			"Mounts": [
					{
							"Source": "/var/lib/kubelet/pods/podUidTest/volumes/kubernetes.io~empty-dir/container8-volume",
							"Destination": "/ArmsAgent",
							"Driver": ""
					}
			],
			"UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/e916e0ba011ab5bf825ba24ffba7e3e5dedd2073e29222dd6238eb874cc27ddd/rootfs",
			"Created": "2023-06-01T10:09:30.213522454+08:00",
			"State": {
					"Pid": 872,
					"Status": "running"
			}
	}
]`

func TestTryReadStaticContainerInfo(t *testing.T) {
	defer os.Remove("./static_container.json")
	defer os.Unsetenv(staticContainerInfoPathEnvKey)
	os.WriteFile("./static_container.json", []byte(staticDockerConfig), os.ModePerm)
	os.Setenv(staticContainerInfoPathEnvKey, "./static_container.json")
	containerInfo, removedIDs, changed, err := tryReadStaticContainerInfo()
	require.Nil(t, err)
	require.Len(t, containerInfo, 1)
	require.Empty(t, removedIDs)
	require.True(t, changed)
	info := containerInfo[0]
	require.Equal(t, "123abc", info.ID)
	require.Equal(t, "xxx_info", info.Name)
	require.Equal(t, "2022-09-16T12:55:14.930245185+08:00", info.Created)
	require.Equal(t, "/var/lib/docker/xxxx/0.log", info.LogPath)
	require.Equal(t, "b", info.Config.Labels["a"])
	require.Equal(t, "centos:latest", info.Config.Image)
	require.Contains(t, info.Config.Env, "x=y")
	require.Equal(t, "app-online", info.Config.Hostname)
	require.Equal(t, "json-file", info.HostConfig.LogConfig.Type)
	require.Equal(t, "/apsarapangu/disk12/docker/overlay/b6ff04a15c7ec040b3ef0857cb091d1c74de27d4d5daf32884a842055e9fbb6d/upper", info.GraphDriver.Data["UpperDir"])
	require.Equal(t, "192.168.1.1", info.NetworkSettings.IPAddress)
	if runtime.GOOS == "linux" {
		require.Equal(t, ContainerStatusExited, info.State.Status)
	} else {
		require.Equal(t, ContainerStatusRunning, info.State.Status)
	}
	require.Equal(t, 999999999908, info.State.Pid)

	os.WriteFile("./static_container.json", []byte(staticDockerConfig2), os.ModePerm)

	containerInfo, removedIDs, changed, err = tryReadStaticContainerInfo()
	require.Nil(t, err)
	require.Len(t, containerInfo, 1)
	require.Len(t, removedIDs, 1)
	require.True(t, changed)

	require.Equal(t, "123abc", removedIDs[0])
	info = containerInfo[0]
	require.Equal(t, "123abc-2", info.ID)
	require.Equal(t, "xxx_info", info.Name)
	require.Equal(t, "2022-09-16T12:56:14+08:00", info.Created)
	require.Equal(t, "/var/lib/docker/xxxx/1.log", info.LogPath)
	require.Equal(t, 999999999909, info.State.Pid)

	containerInfo, removedIDs, changed, err = tryReadStaticContainerInfo()
	require.Nil(t, err)
	require.Len(t, containerInfo, 1)
	require.Len(t, removedIDs, 0)
	require.False(t, changed)
}

func TestLoadStaticContainerConfig(t *testing.T) {
	resetDockerCenter()
	defer os.Remove("./static_container.json")
	defer os.Unsetenv(staticContainerInfoPathEnvKey)
	os.WriteFile("./static_container.json", []byte(staticDockerConfig), os.ModePerm)
	os.Setenv(staticContainerInfoPathEnvKey, "./static_container.json")
	instance := getDockerCenterInstance()
	allInfo := instance.containerMap
	require.Equal(t, 1, len(allInfo))
	for id, info := range allInfo {
		require.Equal(t, "123abc", id)
		require.Equal(t, "192.168.1.1", info.ContainerIP)
	}
}

func TestLoadStaticContainerConfigTwice(t *testing.T) {
	resetDockerCenter()
	defer os.Remove("./static_container.json")
	defer os.Unsetenv(staticContainerInfoPathEnvKey)
	os.WriteFile("./static_container.json", []byte(staticECIConfig), os.ModePerm)
	os.Setenv(staticContainerInfoPathEnvKey, "./static_container.json")
	instance := getDockerCenterInstance()
	allInfo := instance.containerMap
	require.Equal(t, 8, len(allInfo))
	for _, info := range allInfo {
		require.Equal(t, 6, len(info.K8SInfo.Labels))
	}

	os.Remove("./static_container.json")
	os.WriteFile("./static_container.json", []byte(staticECIConfig2), os.ModePerm)

	time.Sleep(time.Second * 10)

	allInfo = instance.containerMap
	require.Equal(t, 8, len(allInfo))
	for _, info := range allInfo {
		require.Equal(t, 7, len(info.K8SInfo.Labels))
	}
}

func checkSameDevInode(t *testing.T, oldname, newname string) {
	logNameStat, _ := os.Stat(oldname)
	logStat0, _ := os.Stat(newname)
	logNameOSStat := GetOSState(logNameStat)
	logOSStat0 := GetOSState(logStat0)
	if logOSStat0.Device != logNameOSStat.Device ||
		logOSStat0.Inode != logNameOSStat.Inode ||
		logOSStat0.ModifyTime != logNameOSStat.ModifyTime ||
		logOSStat0.Size != logNameOSStat.Size {
		t.Errorf("now same os stat %s %s \n %+v %+v \n", oldname, newname, logOSStat0, logNameOSStat)
	}
}

func TestScanContainerdFilesAndReLink(t *testing.T) {
	dir, _ := os.Getwd()
	defer func() {
		_ = os.Remove(filepath.Join(dir, "0.log"))
		_ = os.Remove(filepath.Join(dir, "99.log"))
		_ = os.Remove(filepath.Join(dir, "100.log"))
		_ = os.Remove(filepath.Join(dir, "101.log"))
	}()
	fmt.Printf("working dir : %s \n", dir)

	logName := filepath.Join(dir, "stdout.log")
	go scanContainerdFilesAndReLink(logName)

	time.Sleep(time.Second)
	os.WriteFile(filepath.Join(dir, "0.log"), []byte("abc"), os.ModePerm)
	checkSameDevInode(t, logName, filepath.Join(dir, "0.log"))

	os.WriteFile(filepath.Join(dir, "99.log"), []byte("abcdef"), os.ModePerm)
	time.Sleep(time.Second * 2)
	checkSameDevInode(t, logName, filepath.Join(dir, "99.log"))

	os.WriteFile(filepath.Join(dir, "100.log"), []byte("abcde"), os.ModePerm)
	time.Sleep(time.Second * 2)
	checkSameDevInode(t, logName, filepath.Join(dir, "100.log"))

	os.WriteFile(filepath.Join(dir, "101.log"), []byte("abcdefg"), os.ModePerm)
	time.Sleep(time.Second * 2)
	checkSameDevInode(t, logName, filepath.Join(dir, "101.log"))
}
