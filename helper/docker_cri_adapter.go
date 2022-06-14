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
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	containerdcriserver "github.com/containerd/cri/pkg/server"
	docker "github.com/fsouza/go-dockerclient"
	"google.golang.org/grpc"
	cri "k8s.io/cri-api/pkg/apis/runtime/v1alpha2"
)

const kubeRuntimeAPIVersion = "0.1.0"
const maxMsgSize = 1024 * 1024 * 16

var containerdUnixSocket = "/run/containerd/containerd.sock"
var dockerShimUnixSocket1 = "/var/run/dockershim.sock"
var dockerShimUnixSocket2 = "/run/dockershim.sock"

var criRuntimeWrapper *CRIRuntimeWrapper

// CRIRuntimeWrapper wrapper for containerd client
type innerContainerInfo struct {
	State cri.ContainerState
	Pid   int
}
type CRIRuntimeWrapper struct {
	dockerCenter *DockerCenter

	client         cri.RuntimeServiceClient
	runtimeVersion *cri.VersionResponse

	containersLock sync.RWMutex

	containers map[string]innerContainerInfo

	stopCh <-chan struct{}

	rootfsLock  sync.RWMutex
	rootfsCache map[string]string
}

func IsCRIRuntimeValid(criRuntimeEndpoint string) bool {
	if len(os.Getenv("USE_CONTAINERD")) > 0 {
		return true
	}

	// Verify dockershim.sock existence.
	for _, sock := range []string{dockerShimUnixSocket1, dockerShimUnixSocket2} {
		if fi, err := os.Stat(sock); err == nil && !fi.IsDir() {
			// Having dockershim.sock means k8s + docker cri
			return false
		}
	}

	// Verify containerd.sock cri valid.
	if fi, err := os.Stat(criRuntimeEndpoint); err == nil && !fi.IsDir() {
		if IsCRIStatusValid(criRuntimeEndpoint) {
			return true
		}
	}
	return false
}

func IsCRIStatusValid(criRuntimeEndpoint string) bool {
	addr, dailer, err := GetAddressAndDialer("unix://" + criRuntimeEndpoint)
	if err != nil {
		return false
	}
	ctx, cancel := context.WithTimeout(context.Background(), time.Minute)
	defer cancel()

	conn, err := grpc.DialContext(ctx, addr, grpc.WithInsecure(), grpc.WithDialer(dailer), grpc.WithDefaultCallOptions(grpc.MaxCallRecvMsgSize(1024*1024*16))) //nolint:staticcheck
	if err != nil {
		return false
	}

	client := cri.NewRuntimeServiceClient(conn)
	// check cri status
	for tryCount := 0; tryCount < 5; tryCount++ {
		_, err = client.Status(ctx, &cri.StatusRequest{})
		if err == nil {
			break
		}
		if strings.Contains(err.Error(), "code = Unimplemented") {
			return false
		}
		time.Sleep(time.Millisecond * 100)
	}
	if err != nil {
		return false
	}
	// check running containers
	for tryCount := 0; tryCount < 5; tryCount++ {
		containersResp, err := client.ListContainers(ctx, &cri.ListContainersRequest{Filter: nil})
		if err == nil {
			return containersResp.Containers != nil
		}
		time.Sleep(time.Millisecond * 100)
	}
	return false
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
		return "", "", fmt.Errorf("using %q as endpoint is deprecated, please consider using full url format", endpoint)

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
		containers:     make(map[string]innerContainerInfo),
		stopCh:         make(<-chan struct{}),
		rootfsCache:    make(map[string]string),
	}, nil
}

// createContainerInfo convert cri container to docker spec to adapt the history logic.
func (cw *CRIRuntimeWrapper) createContainerInfo(containerID string) (detail *DockerInfoDetail, sandboxID string, state cri.ContainerState, err error) {
	ctx, cancel := getContextWithTimeout(time.Second * 10)
	status, err := cw.client.ContainerStatus(ctx, &cri.ContainerStatusRequest{
		ContainerId: containerID,
		Verbose:     true,
	})
	cancel()
	if err != nil {
		return nil, "", cri.ContainerState_CONTAINER_UNKNOWN, err
	}

	var ci containerdcriserver.ContainerInfo
	foundInfo := false
	if statusinfo := status.GetInfo(); statusinfo != nil {
		if info, ok := statusinfo["info"]; ok {
			foundInfo = true
			ci, err = parseContainerInfo(info)
			if err != nil {
				logger.Errorf(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "failed to parse container info, containerId: %s, data: %s, error: %v", containerID, info, err)
			}
		}
	}

	if !foundInfo {
		logger.Warningf(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "can not find container info from CRI::ContainerStatus, containerId: %s", containerID)
		return nil, "", cri.ContainerState_CONTAINER_UNKNOWN, fmt.Errorf("can not find container info from CRI::ContainerStatus, containerId: %s", containerID)
	}
	// only check pid for container that is older than DefaultSyncContainersPeriod
	// to give a chance to collect emphemeral containers
	if time.Since(time.Unix(0, status.GetStatus().GetCreatedAt())) > DefaultSyncContainersPeriod {
		exist := ContainerProcessAlive(int(ci.Pid))
		if !exist {
			return nil, "", cri.ContainerState_CONTAINER_UNKNOWN, fmt.Errorf("find container %s pid %d was already stopped", containerID, ci.Pid)
		}
	}

	labels := status.GetStatus().GetLabels()
	if labels == nil {
		labels = map[string]string{}
	}

	image := status.GetStatus().GetImage().GetImage()
	if image == "" {
		image = status.GetStatus().GetImageRef()
	}

	dockerContainer := &docker.Container{
		ID:      containerID,
		LogPath: status.GetStatus().GetLogPath(),
		Config: &docker.Config{
			Labels: labels,
			Image:  image,
		},
		State: docker.State{
			Pid: int(ci.Pid),
		},
		HostConfig: &docker.HostConfig{
			VolumeDriver: ci.Snapshotter,
			Runtime:      cw.runtimeVersion.RuntimeName,
			LogConfig: docker.LogConfig{
				Type: "json-file",
			},
		},
	}

	if status.GetStatus().GetMetadata() != nil {
		dockerContainer.Name = status.GetStatus().GetMetadata().GetName()
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

	return cw.dockerCenter.CreateInfoDetail(dockerContainer, envConfigPrefix, false), ci.SandboxID, status.GetStatus().GetState(), nil
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
	cw.containersLock.Lock()
	defer cw.containersLock.Unlock()
	for _, container := range containersResp.Containers {
		if container.State == cri.ContainerState_CONTAINER_EXITED || container.State == cri.ContainerState_CONTAINER_UNKNOWN {
			continue
		}
		dockerContainer, _, _, err := cw.createContainerInfo(container.GetId())
		if err != nil {
			logger.Debug(context.Background(), "Create container info from cri-runtime error", err)
			continue
		}
		cw.containers[container.GetId()] = innerContainerInfo{
			State: container.State,
			Pid:   dockerContainer.ContainerInfo.State.Pid,
		}
		containerMap[container.GetId()] = dockerContainer

		// append the pod labels to the k8s info.
		if sandbox, ok := sandboxMap[container.PodSandboxId]; ok {
			cw.wrapperK8sInfoByLabels(sandbox.GetLabels(), dockerContainer)
		}
		logger.Debug(context.Background(), "Create container info from cri-runtime success, info", *dockerContainer.ContainerInfo, "config", *dockerContainer.ContainerInfo.Config, "detail", *dockerContainer)
	}
	cw.dockerCenter.updateContainers(containerMap)

	for k := range cw.containers {
		if _, ok := containerMap[k]; !ok {
			cw.dockerCenter.markRemove(k)
			delete(cw.containers, k)
		}
	}
	return nil
}

func (cw *CRIRuntimeWrapper) loopSyncContainers() {
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
	logger.Debug(context.Background(), "cri sync containers", "begin")
	containersResp, err := cw.client.ListContainers(ctx, &cri.ListContainersRequest{})
	if err != nil {
		return err
	}

	newContainers := map[string]*cri.Container{}
	for i, container := range containersResp.Containers {
		if container.State == cri.ContainerState_CONTAINER_EXITED || container.State == cri.ContainerState_CONTAINER_UNKNOWN {
			continue
		}
		id := containersResp.Containers[i].GetId()
		newContainers[id] = containersResp.Containers[i]
	}

	// update container
	for id, container := range newContainers {
		cw.containersLock.RLock()
		oldInfo, ok := cw.containers[id]
		cw.containersLock.RUnlock()
		if !ok || oldInfo.State != container.State {
			if err := cw.fetchOne(id); err != nil {
				logger.Errorf(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "failed to createContainerInfo, containerId: %s, error: %v", id, err)
				continue
			}
		} else if ok {
			exist := ContainerProcessAlive(oldInfo.Pid)
			if !exist {
				delete(newContainers, id)
			}
		}
	}

	// delete container
	cw.containersLock.Lock()
	defer cw.containersLock.Unlock()
	for oldID := range cw.containers {
		if _, ok := newContainers[oldID]; !ok {
			cw.dockerCenter.markRemove(oldID)
			logger.Debug(context.Background(), "cri sync containers remove", oldID)
			delete(cw.containers, oldID)
		}
	}
	logger.Debug(context.Background(), "cri sync containers", "done")
	return nil
}

func (cw *CRIRuntimeWrapper) fetchOne(containerID string) error {
	logger.Debug(context.Background(), "trigger fetchOne")
	dockerContainer, sandboxID, status, err := cw.createContainerInfo(containerID)
	if err != nil {
		return err
	}
	cw.wrapperK8sInfoByID(sandboxID, dockerContainer)

	if logger.DebugFlag() {
		bytes, _ := json.Marshal(dockerContainer)
		logger.Debugf(context.Background(), "cri create container info : %s", string(bytes))
	}

	cw.dockerCenter.updateContainer(containerID, dockerContainer)
	cw.containersLock.Lock()
	defer cw.containersLock.Unlock()
	cw.containers[containerID] = innerContainerInfo{
		status,
		dockerContainer.ContainerInfo.State.Pid,
	}
	return nil
}

func (cw *CRIRuntimeWrapper) wrapperK8sInfoByID(sandboxID string, detail *DockerInfoDetail) {
	ctx, cancel := getContextWithTimeout(time.Second * 10)
	status, err := cw.client.PodSandboxStatus(ctx, &cri.PodSandboxStatusRequest{
		PodSandboxId: sandboxID,
		Verbose:      true,
	})
	cancel()
	if err != nil {
		logger.Debug(context.Background(), "fetchone cannot read k8s info from sandbox, sandboxID", sandboxID)
		return
	}
	cw.wrapperK8sInfoByLabels(status.GetStatus().GetLabels(), detail)
}

func (cw *CRIRuntimeWrapper) wrapperK8sInfoByLabels(sandboxLabels map[string]string, detail *DockerInfoDetail) {
	if detail.K8SInfo == nil || sandboxLabels == nil {
		return
	}
	if detail.K8SInfo.Labels == nil {
		detail.K8SInfo.Labels = make(map[string]string)
	}
	for k, v := range sandboxLabels {
		if strings.HasPrefix(k, k8sInnerLabelPrefix) || strings.HasPrefix(k, k8sInnerAnnotationPrefix) {
			continue
		}
		detail.K8SInfo.Labels[k] = v
	}
}

func (cw *CRIRuntimeWrapper) sweepCache() {
	// clear unuseful cache
	usedCacheItem := make(map[string]bool)
	cw.dockerCenter.lock.RLock()
	for key := range cw.dockerCenter.containerMap {
		usedCacheItem[key] = true
	}
	cw.dockerCenter.lock.RUnlock()

	cw.rootfsLock.Lock()
	for key := range cw.rootfsCache {
		if _, ok := usedCacheItem[key]; !ok {
			delete(cw.rootfsCache, key)
		}
	}
	cw.rootfsLock.Unlock()
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

func (cw *CRIRuntimeWrapper) lookupRootfsCache(containerID string) (string, bool) {
	cw.rootfsLock.RLock()
	defer cw.rootfsLock.RUnlock()
	dir, ok := cw.rootfsCache[containerID]
	return dir, ok
}

func (cw *CRIRuntimeWrapper) lookupContainerRootfsAbsDir(info *docker.Container) string {
	// For cri-runtime
	containerID := info.ID
	if dir, ok := cw.lookupRootfsCache(containerID); ok {
		return dir
	}

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

	for _, a := range aDirs {
		for _, c := range cDirs {
			for _, d := range dDirs {
				for _, b := range bDirs {
					dir := path.Join(a, b, c, info.ID, d)
					if fi, err := os.Stat(dir); err == nil && fi.IsDir() {
						cw.rootfsLock.Lock()
						cw.rootfsCache[containerID] = dir
						cw.rootfsLock.Unlock()
						return dir
					}
				}
			}
		}
	}

	return ""
}

func ContainerProcessAlive(pid int) bool {
	procStatPath := GetMountedFilePath(fmt.Sprintf("/proc/%d/stat", pid))
	exist, err := util.PathExists(procStatPath)
	if err != nil {
		logger.Error(context.Background(), "DETECT_CONTAINER_ALARM", "stat container proc path", procStatPath, "error", err)
	} else if !exist {
		return false
	}
	return true
}

func init() {
	containerdSockPathStr := os.Getenv("CONTAINERD_SOCK_PATH")
	if len(containerdSockPathStr) > 0 {
		containerdUnixSocket = containerdSockPathStr
	}
}
