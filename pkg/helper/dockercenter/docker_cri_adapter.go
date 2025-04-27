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

package dockercenter

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"time"

	containerdcriserver "github.com/containerd/containerd/pkg/cri/server"
	"github.com/docker/docker/api/types"
	"google.golang.org/grpc"
	criv1 "k8s.io/cri-api/pkg/apis/runtime/v1"
	criv1alpha2 "k8s.io/cri-api/pkg/apis/runtime/v1alpha2"

	"github.com/alibaba/ilogtail/pkg/logger"
)

const (
	maxMsgSize = 1024 * 1024 * 16
)

var criRuntimeWrapper *CRIRuntimeWrapper

// CRIV1Alpha2Wrapper wrapper for containerd client
type innerContainerInfo struct {
	State criv1alpha2.ContainerState
	// criv1.ContainerState
	Pid    int
	Name   string
	Status string
}

type RuntimeInfo struct {
	version           string
	runtimeName       string
	runtimeVersion    string
	runtimeAPIVersion string
}

type CRIInterface interface {
	createContainerInfo(containerID string) (detail *DockerInfoDetail, sandboxID string, state criv1alpha2.ContainerState, err error)
	fetchAll() error
	fetchOne(containerID string) error
	getContainerUpperDir(containerid string, snapshotter string) string
	lookupContainerRootfsAbsDir(info types.ContainerJSON) string
	lookupRootfsCache(containerID string) (string, bool)
	loopSyncContainers()
	sweepCache()
	syncContainers() error
	wrapperK8sInfoByID(sandboxID string, detail *DockerInfoDetail)
	wrapperK8sInfoByLabels(sandboxLabels map[string]string, detail *DockerInfoDetail)
}

type CRIRuntimeWrapper struct {
	CRIInterface
	RuntimeInfo RuntimeInfo
}

func NewCRIRuntimeWrapper(dockerCenter *DockerCenter, info RuntimeInfo) (*CRIRuntimeWrapper, error) {
	var wrapper CRIInterface
	var err error
	switch info.runtimeAPIVersion {
	case "v1alpha2":
		wrapper, err = NewCRIV1Alpha2Wrapper(dockerCenter, info)
		if err != nil {
			return nil, err
		}
	// Todo: support v1
	// case "v1":
	default:
		return nil, fmt.Errorf("unsupported cri runtimr api version %q", info.runtimeAPIVersion)
	}

	return &CRIRuntimeWrapper{
		CRIInterface: wrapper,
		RuntimeInfo:  info,
	}, nil
}

func IsCRIRuntimeValid(criRuntimeEndpoint string) (bool, RuntimeInfo) {
	// Verify containerd.sock cri valid.
	if fi, err := os.Stat(criRuntimeEndpoint); err != nil || fi.IsDir() {
		return false, RuntimeInfo{}
	}

	addr, dailer, err := GetAddressAndDialer(criRuntimeEndpoint)
	if err != nil {
		return false, RuntimeInfo{}
	}
	ctx, cancel := getContextWithTimeout(time.Minute)
	defer cancel()

	conn, err := grpc.DialContext(ctx, addr, grpc.WithInsecure(), grpc.WithDialer(dailer), grpc.WithDefaultCallOptions(grpc.MaxCallRecvMsgSize(1024*1024*16)))
	if err != nil {
		logger.Debug(context.Background(), "Dial", addr, "failed", err)
		return false, RuntimeInfo{}
	}
	// must close，otherwise connections will leak and case mem increase
	defer conn.Close()

	// 优先尝试使用v1alpha2版本的CRI API
	v1alpha2Client := criv1alpha2.NewRuntimeServiceClient(conn)
	v1alpha2Version, err := v1alpha2Client.Version(ctx, &criv1alpha2.VersionRequest{})
	if err == nil {
		info := RuntimeInfo{
			version:           v1alpha2Version.Version,
			runtimeName:       v1alpha2Version.RuntimeName,
			runtimeVersion:    v1alpha2Version.RuntimeVersion,
			runtimeAPIVersion: v1alpha2Version.RuntimeApiVersion,
		}
		// check running containers
		var containersResp *criv1alpha2.ListContainersResponse
		containersResp, err = v1alpha2Client.ListContainers(ctx, &criv1alpha2.ListContainersRequest{Filter: nil})
		if err == nil {
			logger.Debug(context.Background(), "ListContainers result", containersResp.Containers)
			return containersResp.Containers != nil, info
		}
		logger.Debug(context.Background(), "ListContainers failed", err)
		return false, info
	}
	// 如果v1alpha2失败，尝试使用v1版本的CRI API
	v1Client := criv1.NewRuntimeServiceClient(conn)
	v1Version, err := v1Client.Version(ctx, &criv1.VersionRequest{})
	if err == nil {
		info := RuntimeInfo{
			version:           v1Version.Version,
			runtimeName:       v1Version.RuntimeName,
			runtimeVersion:    v1Version.RuntimeVersion,
			runtimeAPIVersion: v1Version.RuntimeApiVersion,
		}
		// check running containers
		var containersResp *criv1.ListContainersResponse
		containersResp, err = v1Client.ListContainers(ctx, &criv1.ListContainersRequest{Filter: nil})
		if err == nil {
			logger.Debug(context.Background(), "ListContainers result", containersResp.Containers)
			return containersResp.Containers != nil, info
		}
		logger.Debug(context.Background(), "ListContainers failed", err)
		return false, info
	}

	logger.Errorf(context.Background(), "Failed to get CRI version from both v1alpha2 and v1", err.Error())
	return false, RuntimeInfo{}
}

func getContextWithTimeout(timeout time.Duration) (context.Context, context.CancelFunc) {
	return context.WithTimeout(context.Background(), timeout)
}

func parseContainerInfo(data string) (containerdcriserver.ContainerInfo, error) {
	var ci containerdcriserver.ContainerInfo
	err := json.Unmarshal([]byte(data), &ci)
	return ci, err
}

func init() {
	containerdSockPathStr := os.Getenv("CONTAINERD_SOCK_PATH")
	if len(containerdSockPathStr) > 0 {
		containerdUnixSocket = containerdSockPathStr
	}
}
