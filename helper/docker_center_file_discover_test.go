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

func TestLoadStaticContainerConfig(t *testing.T) {
	resetDockerCenter()
	ioutil.WriteFile("./static_container.json", []byte(staticDockerConfig), os.ModePerm)
	os.Setenv(staticContainerInfoPathEnvKey, "./static_container.json")
	instance := getDockerCenterInstance()
	allInfo := instance.containerMap
	require.Equal(t, len(allInfo), 1)
	for id, info := range allInfo {
		require.Equal(t, id, "123abc")
		require.Equal(t, info.ContainerIP, "192.168.1.1")
	}

	ioutil.WriteFile("./static_container.json", []byte(staticDockerConfig2), os.ModePerm)

	time.Sleep(time.Second * 2)

	allInfo = instance.containerMap
	require.Equal(t, len(allInfo), 2)
	newInfo, ok := allInfo["123abc-2"]
	require.Equal(t, ok, true)
	require.Equal(t, newInfo.ContainerIP != "", true)
	require.Equal(t, newInfo.ContainerInfo.Config.Hostname != "", true)
	require.Equal(t, newInfo.ContainerInfo.LogPath, "/var/lib/docker/xxxx/1.log")

	for key, val := range allInfo {
		fmt.Printf("%s %v %v %v %v \n", key, *val, *val.ContainerInfo, *val.ContainerInfo.Config, *val.ContainerInfo.NetworkSettings)
	}
	os.Unsetenv(staticContainerInfoPathEnvKey)
	os.Remove("./static_container.json")
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
	_ = os.Remove(filepath.Join(dir, "0.log"))
	_ = os.Remove(filepath.Join(dir, "99.log"))
	_ = os.Remove(filepath.Join(dir, "100.log"))
	_ = os.Remove(filepath.Join(dir, "101.log"))
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
