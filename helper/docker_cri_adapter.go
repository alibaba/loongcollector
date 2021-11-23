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

// +build linux

package helper

import (
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net"
	"net/url"
	"os"
	"path"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"

	containerdcriserver "github.com/containerd/cri/pkg/server"
	docker "github.com/fsouza/go-dockerclient"
	"go.uber.org/atomic"
	"google.golang.org/grpc"
	cri "k8s.io/cri-api/pkg/apis/runtime/v1alpha2"
)

const kubeRuntimeAPIVersion = "0.1.0"
const maxMsgSize = 1024 * 1024 * 16

var DefaultSyncContainersPeriod = time.Second * 10
var containerdUnixSocket = "/run/containerd/containerd.sock"
var criRuntimeWrapper *CRIRuntimeWrapper

var defaultContainerDFlag atomic.Int32

// CRIRuntimeWrapper wrapper for containerd client
type CRIRuntimeWrapper struct {
	// client       *containerd.Client
	dockerCenter *DockerCenter

	client         cri.RuntimeServiceClient
	runtimeVersion *cri.VersionResponse

	containersLock sync.RWMutex
	containers     map[string]cri.ContainerState

	stopCh <-chan struct{}
}

func IsCRIRuntimeValid(criRuntimeEndpoint string) bool {
	if len(os.Getenv("USE_CONTAINERD")) > 0 {
		return true
	}
	// Verify docker.sock existence.
	for _, sock := range []string{"/var/run/docker.sock", "/run/docker.sock"} {
		if fi, err := os.Stat(sock); err == nil && !fi.IsDir() {
			return false
		}
	}

	stat, err := os.Stat(criRuntimeEndpoint)
	if err != nil || stat.IsDir() {
		return false
	}

	//TODO: make a cri client and test version() @jiangbo

	return true
}

// GetAddressAndDialer returns the address parsed from the given endpoint and a dialer.
func GetAddressAndDialer(endpoint string) (string, func(addr string, timeout time.Duration) (net.Conn, error), error) {
	protocol, addr, err := parseEndpointWithFallbackProtocol(endpoint, "unix")
	if err != nil {
		return "", nil, err
	}
	if protocol != "unix" {
		return "", nil, fmt.Errorf("only support unix socket endpoint")
	}

	return addr, dial, nil
}

func dial(addr string, timeout time.Duration) (net.Conn, error) {
	return net.DialTimeout("unix", addr, timeout)
}

func parseEndpointWithFallbackProtocol(endpoint string, fallbackProtocol string) (protocol string, addr string, err error) {
	if protocol, addr, err = parseEndpoint(endpoint); err != nil && protocol == "" {
		fallbackEndpoint := fallbackProtocol + "://" + endpoint
		protocol, addr, err = parseEndpoint(fallbackEndpoint)
		if err == nil {
			logger.Infof(context.Background(), "Using %q as endpoint is deprecated, please consider using full url format %q.", endpoint, fallbackEndpoint)
		}
	}
	return
}

func parseEndpoint(endpoint string) (string, string, error) {
	u, err := url.Parse(endpoint)
	if err != nil {
		return "", "", err
	}

	switch u.Scheme {
	case "tcp":
		return "tcp", u.Host, nil

	case "unix":
		return "unix", u.Path, nil

	case "":
		return "", "", fmt.Errorf("Using %q as endpoint is deprecated, please consider using full url format", endpoint)

	default:
		return u.Scheme, "", fmt.Errorf("protocol %q not supported", u.Scheme)
	}
}

func newRuntimeServiceClient() (cri.RuntimeServiceClient, error) {
	addr, dailer, err := GetAddressAndDialer("unix://" + containerdUnixSocket)
	if err != nil {
		return nil, err
	}

	ctx, cancel := context.WithTimeout(context.Background(), time.Second*10)
	defer cancel()

	conn, err := grpc.DialContext(ctx, addr, grpc.WithInsecure(), grpc.WithDialer(dailer), grpc.WithDefaultCallOptions(grpc.MaxCallRecvMsgSize(maxMsgSize))) //nolint:staticcheck
	if err != nil {
		return nil, err
	}

	return cri.NewRuntimeServiceClient(conn), nil
}

// NewCRIRuntimeWrapper ...
func NewCRIRuntimeWrapper(dockerCenter *DockerCenter) (*CRIRuntimeWrapper, error) {
	client, err := newRuntimeServiceClient()
	if err != nil {
		logger.Errorf(context.Background(), "CONNECT_CRI_RUNTIME_ALARM", "Connect remote cri-runtime failed: %v", err)
		return nil, err
	}

	runtimeVersion, err := getCRIRuntimeVersion(client)
	if err != nil {
		return nil, err
	}

	return &CRIRuntimeWrapper{
		dockerCenter:   dockerCenter,
		client:         client,
		runtimeVersion: runtimeVersion,
		containers:     make(map[string]cri.ContainerState),
		stopCh:         make(<-chan struct{}),
	}, nil
}

// createContainerInfo convert cri container to docker spec to adapt the history logic.
func (cw *CRIRuntimeWrapper) createContainerInfo(_ context.Context, c *cri.Container) (*DockerInfoDetail, error) {
	ctx, cancel := getContextWithTimeout(time.Second * 10)
	status, err := cw.client.ContainerStatus(ctx, &cri.ContainerStatusRequest{
		ContainerId: c.GetId(),
		Verbose:     true,
	})
	cancel()
	if err != nil {
		return nil, err
	}

	var ci containerdcriserver.ContainerInfo
	foundInfo := false
	if statusinfo := status.GetInfo(); statusinfo != nil {
		if info, ok := statusinfo["info"]; ok {
			foundInfo = true
			ci, err = parseContainerInfo(info)
			if err != nil {
				logger.Errorf(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "failed to parse container info, containerId: %s, data: %s, error: %v", c.GetId(), info, err)
			}
		}
	}
	if !foundInfo {
		logger.Warningf(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "can not find container info from CRI::ContainerStatus, containerId: %s", c.GetId())
	}

	labels := c.GetLabels()
	if labels == nil {
		labels = map[string]string{}
	}

	image := status.GetStatus().GetImage().GetImage()
	if image == "" {
		image = status.GetStatus().GetImageRef()
	}

	dockerContainer := &docker.Container{
		ID:      c.GetId(),
		LogPath: status.GetStatus().GetLogPath(),
		Config: &docker.Config{
			Labels: labels,
			Image:  image,
		},
		HostConfig: &docker.HostConfig{
			VolumeDriver: ci.Snapshotter,
			Runtime:      cw.runtimeVersion.RuntimeName,
			LogConfig: docker.LogConfig{
				Type: "json-file",
			},
		},
	}

	if c.GetMetadata() != nil {
		dockerContainer.Name = c.GetMetadata().GetName()
	}

	if ci.RuntimeSpec != nil && ci.RuntimeSpec.Process != nil {
		dockerContainer.Config.Env = ci.RuntimeSpec.Process.Env
	} else {
		var envs []string
		for _, kv := range ci.Config.Envs {
			envs = append(envs, kv.Key+"="+kv.Value)
		}
		dockerContainer.Config.Env = envs
	}

	var hostsPath string
	var hostnamePath string
	if ci.RuntimeSpec != nil {
		for _, mount := range ci.RuntimeSpec.Mounts {
			if mount.Destination == "/etc/hosts" {
				hostsPath = mount.Source
			}
			if mount.Destination == "/etc/hostname" {
				hostnamePath = mount.Source
			}
			dockerContainer.Mounts = append(dockerContainer.Mounts, docker.Mount{
				Source:      mount.Source,
				Destination: mount.Destination,
				Driver:      mount.Type,
			})
		}
	}

	dockerContainer.Config.Mounts = dockerContainer.Mounts
	if len(hostnamePath) > 0 {
		hn, _ := ioutil.ReadFile(GetMountedFilePath(hostnamePath))
		dockerContainer.Config.Hostname = strings.Trim(string(hn), "\t \n")
	}
	dockerContainer.HostnamePath = hostnamePath
	dockerContainer.HostsPath = hostsPath

	return cw.dockerCenter.CreateInfoDetail(dockerContainer, envConfigPrefix, false), nil
}

func (cw *CRIRuntimeWrapper) fetchAll() error {
	ctx, cancel := context.WithTimeout(context.Background(), time.Minute)
	defer cancel()
	containersResp, err := cw.client.ListContainers(ctx, &cri.ListContainersRequest{Filter: nil})
	logger.Debugf(context.Background(), "CRIRuntime ListContainers: %#v, error: %v", containersResp, err)
	if err != nil {
		return err
	}
	sandboxResp, err := cw.client.ListPodSandbox(ctx, &cri.ListPodSandboxRequest{Filter: nil})
	logger.Debugf(context.Background(), "CRIRuntime ListPodSandbox: %#v, error: %v", sandboxResp, err)
	if err != nil {
		return err
	}
	sandboxMap := make(map[string]*cri.PodSandbox, len(sandboxResp.Items))
	for _, item := range sandboxResp.Items {
		sandboxMap[item.Id] = item
	}

	containerMap := make(map[string]*DockerInfoDetail)
	for _, container := range containersResp.Containers {
		if container.State == cri.ContainerState_CONTAINER_EXITED || container.State == cri.ContainerState_CONTAINER_UNKNOWN {
			continue
		}
		dockerContainer, err := cw.createContainerInfo(ctx, container)
		if err != nil {
			logger.Debug(context.Background(), "Create container info from cri-runtime error", err)
			continue
		}
		containerMap[container.GetId()] = dockerContainer

		// append the pod labels to the k8s info.
		if sandbox, ok := sandboxMap[container.PodSandboxId]; ok && dockerContainer.K8SInfo != nil {
			if dockerContainer.K8SInfo.Labels == nil {
				dockerContainer.K8SInfo.Labels = make(map[string]string)
			}
			for k, v := range sandbox.Labels {
				if strings.HasPrefix(k, k8sInnerLabelPrefix) || strings.HasPrefix(k, k8sInnerAnnotationPrefix) {
					continue
				}
				dockerContainer.K8SInfo.Labels[k] = v
			}
		}
		logger.Debug(context.Background(), "Create container info from cri-runtime success, info", *dockerContainer.ContainerInfo, "config", *dockerContainer.ContainerInfo.Config, "detail", *dockerContainer)
	}
	cw.dockerCenter.updateContainers(containerMap)
	return nil
}

func (cw *CRIRuntimeWrapper) loopSyncContainers() {
	listenLoopIntervalStr := os.Getenv("CONTAINERD_LISTEN_LOOP_INTERVAL")
	if len(listenLoopIntervalStr) > 0 {
		listenLoopIntervalSec, _ := strconv.Atoi(listenLoopIntervalStr)
		if listenLoopIntervalSec > 0 {
			DefaultSyncContainersPeriod = time.Second * time.Duration(listenLoopIntervalSec)
		}
	}
	ticker := time.NewTicker(DefaultSyncContainersPeriod)
	for {
		select {
		case <-cw.stopCh:
			return
		case <-ticker.C:
			if err := cw.syncContainers(); err != nil {
				logger.Errorf(context.Background(), "SYNC_CONTAINERD_ALARM", "syncContainers error: %v", err)
			}
		}
	}
}

func (cw *CRIRuntimeWrapper) syncContainers() error {
	ctx, cancel := getContextWithTimeout(time.Second * 20)
	defer cancel()
	containersResp, err := cw.client.ListContainers(ctx, &cri.ListContainersRequest{})
	if err != nil {
		return err
	}

	cw.containersLock.Lock()
	defer cw.containersLock.Unlock()

	oldContainers := cw.containers

	newContainers := map[string]*cri.Container{}
	for i := range containersResp.Containers {
		id := containersResp.Containers[i].GetId()
		newContainers[id] = containersResp.Containers[i]
	}

	// update container
	for id, container := range newContainers {
		if oldState, ok := oldContainers[id]; !ok || oldState != container.State {
			ctx, cancel := getContextWithTimeout(time.Second * 10)
			dockerContainer, err := cw.createContainerInfo(ctx, container)
			cancel()
			if err != nil {
				logger.Errorf(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "failed to createContainerInfo, containerId: %s, error: %v", id, err)
				continue
			}
			cw.dockerCenter.updateContainer(id, dockerContainer)
			cw.containers[id] = container.State
		}
	}

	// delete container
	for oldID := range cw.containers {
		if _, ok := newContainers[oldID]; !ok {
			cw.dockerCenter.markRemove(oldID)
			delete(cw.containers, oldID)
		}
	}

	return nil
}

func (cw *CRIRuntimeWrapper) run() error {
	logger.Info(context.Background(), "CRIRuntime background syncer", "start")
	_ = cw.fetchAll()

	timerFetch := func() {
		defer dockerCenterRecover()
		lastFetchAllTime := time.Now()
		for {
			time.Sleep(time.Duration(10) * time.Second)
			logger.Debug(context.Background(), "docker clean timeout container info", "start")
			cw.dockerCenter.cleanTimeoutContainer()
			logger.Debug(context.Background(), "docker clean timeout container info", "done")
			if time.Since(lastFetchAllTime) >= FetchAllInterval {
				logger.Info(context.Background(), "CRIRuntime fetch all", "start")
				cw.dockerCenter.readStaticConfig(true)
				err := cw.fetchAll()
				logger.Info(context.Background(), "CRIRuntime fetch all", err)
				lastFetchAllTime = time.Now()
			}

		}
	}
	go timerFetch()

	go cw.loopSyncContainers()

	return nil
}

func getContextWithTimeout(timeout time.Duration) (context.Context, context.CancelFunc) {
	return context.WithTimeout(context.Background(), timeout)
}

func getCRIRuntimeVersion(client cri.RuntimeServiceClient) (*cri.VersionResponse, error) {
	ctx, cancel := getContextWithTimeout(time.Second * 10)
	defer cancel()
	return client.Version(ctx, &cri.VersionRequest{Version: kubeRuntimeAPIVersion})
}

func parseContainerInfo(data string) (containerdcriserver.ContainerInfo, error) {
	var ci containerdcriserver.ContainerInfo
	err := json.Unmarshal([]byte(data), &ci)
	return ci, err
}

func lookupContainerRootfsAbsDir(info *docker.Container) string {
	// For cri-runtime

	// Example: /run/containerd/io.containerd.runtime.v1.linux/k8s.io/{ContainerID}/rootfs/
	aDirs := []string{
		"/run/containerd",
		"/var/run/containerd",
	}

	bDirs := []string{
		"io.containerd.runtime.v2.task",
		"io.containerd.runtime.v1.linux",
		"runc",
	}

	cDirs := []string{
		"k8s.io",
		"",
	}

	dDirs := []string{
		"rootfs",
		"root",
	}

	// if default is ok, skip stat
	if defaultContainerDFlag.Load() == 1 {
		return path.Join(aDirs[0], bDirs[0], cDirs[0], info.ID, dDirs[0])
	}

	for aIndex, a := range aDirs {
		for bIndex, b := range bDirs {
			for cIndex, c := range cDirs {
				for dIndex, d := range dDirs {
					dir := path.Join(a, b, c, info.ID, d)
					if fi, err := os.Stat(dir); err == nil && fi.IsDir() {
						if aIndex == 0 && bIndex == 0 && cIndex == 0 && dIndex == 0 {
							defaultContainerDFlag.Store(1)
						}
						return dir
					}
				}
			}
		}
	}

	return ""
}

func init() {
	containerdSockPathStr := os.Getenv("CONTAINERD_SOCK_PATH")
	if len(containerdSockPathStr) > 0 {
		containerdUnixSocket = containerdSockPathStr
	}
}
