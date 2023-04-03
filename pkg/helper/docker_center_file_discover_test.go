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
	"io/ioutil"
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

func TestTryReadStaticContainerInfo(t *testing.T) {
	defer os.Remove("./static_container.json")
	defer os.Unsetenv(staticContainerInfoPathEnvKey)
	ioutil.WriteFile("./static_container.json", []byte(staticDockerConfig), os.ModePerm)
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

	ioutil.WriteFile("./static_container.json", []byte(staticDockerConfig2), os.ModePerm)

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
	ioutil.WriteFile("./static_container.json", []byte(staticDockerConfig), os.ModePerm)
	os.Setenv(staticContainerInfoPathEnvKey, "./static_container.json")
	instance := getDockerCenterInstance()
	allInfo := instance.containerMap
	require.Equal(t, 1, len(allInfo))
	for id, info := range allInfo {
		require.Equal(t, "123abc", id)
		require.Equal(t, "192.168.1.1", info.ContainerIP)
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
	ioutil.WriteFile(filepath.Join(dir, "0.log"), []byte("abc"), os.ModePerm)
	checkSameDevInode(t, logName, filepath.Join(dir, "0.log"))

	ioutil.WriteFile(filepath.Join(dir, "99.log"), []byte("abcdef"), os.ModePerm)
	time.Sleep(time.Second * 2)
	checkSameDevInode(t, logName, filepath.Join(dir, "99.log"))

	ioutil.WriteFile(filepath.Join(dir, "100.log"), []byte("abcde"), os.ModePerm)
	time.Sleep(time.Second * 2)
	checkSameDevInode(t, logName, filepath.Join(dir, "100.log"))

	ioutil.WriteFile(filepath.Join(dir, "101.log"), []byte("abcdefg"), os.ModePerm)
	time.Sleep(time.Second * 2)
	checkSameDevInode(t, logName, filepath.Join(dir, "101.log"))
}
