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

package containercenter

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path"
	"strings"
	"sync"
	"time"

	"github.com/containerd/containerd"
	containerdcriserver "github.com/containerd/containerd/pkg/cri/server"
	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
)

const (
	maxMsgSize            = 1024 * 1024 * 16
	defaultContextTimeout = time.Second * 10
)

var criRuntimeWrapper *CRIRuntimeWrapper

// CRIRuntimeWrapper wrapper for containerd client
type innerContainerInfo struct {
	State  CriContainerState
	Pid    int
	Name   string
	Status string
}

type CRIRuntimeWrapper struct {
	containerCenter *ContainerCenter
	nativeClient    *containerd.Client
	client          *RuntimeServiceClient
	runtimeInfo     CriVersionInfo

	containersLock sync.Mutex

	containers       map[string]*innerContainerInfo
	containerHistory map[string]bool

	stopCh <-chan struct{}

	rootfsLock             sync.RWMutex
	rootfsCache            map[string]string
	listContainerStartTime int64 // in nanosecond
}

func NewCRIRuntimeWrapper(containerCenter *ContainerCenter) (*CRIRuntimeWrapper, error) {
	if fi, err := os.Stat(containerdUnixSocket); err != nil || fi.IsDir() {
		return nil, fmt.Errorf("cri runtime endpoint %s is not valid", containerdUnixSocket)
	}

	client, err := NewRuntimeServiceClient(defaultContextTimeout, maxMsgSize)
	if err != nil {
		logger.Errorf(context.Background(), "CONNECT_CRI_RUNTIME_ALARM", "Connect remote cri-runtime failed: %v", err)
		return nil, err
	}

	ctx, cancel := getContextWithTimeout(defaultContextTimeout)
	defer cancel()

	containerResp, err := client.ListContainers(ctx)
	if err != nil {
		logger.Errorf(context.Background(), "CONNECT_CRI_RUNTIME_ALARM", "List containers from cri-runtime failed: %v", err)
		return nil, err
	} else if len(containerResp.Containers) == 0 {
		err = errors.New("remote cri-runtime has no container")
		logger.Errorf(context.Background(), "CONNECT_CRI_RUNTIME_ALARM", "Remote cri-runtime is invalid: %v", err)
		return nil, err
	}

	var containerdClient *containerd.Client
	if *flags.EnableContainerdUpperDirDetect {
		containerdClient, err = containerd.New(containerdUnixSocket, containerd.WithDefaultNamespace("k8s.io"))
		if err == nil {
			_, err = containerdClient.Version(context.Background())
		}
		if err != nil {
			logger.Warning(context.Background(), "CONTAINERD_CLIENT_ALARM", "Connect containerd failed", err)
			containerdClient = nil
		}
	}

	return &CRIRuntimeWrapper{
		containerCenter:        containerCenter,
		client:                 client,
		nativeClient:           containerdClient,
		runtimeInfo:            client.info,
		containers:             make(map[string]*innerContainerInfo),
		containerHistory:       make(map[string]bool),
		stopCh:                 make(<-chan struct{}),
		rootfsCache:            make(map[string]string),
		listContainerStartTime: time.Now().UnixNano(),
	}, nil
}

// createContainerInfo convert cri container to docker spec to adapt the history logic.
func (cw *CRIRuntimeWrapper) createContainerInfo(containerID string) (detail *DockerInfoDetail, sandboxID string, state CriContainerState, err error) {
	ctx, cancel := getContextWithTimeout(defaultContextTimeout)
	status, err := cw.client.ContainerStatus(ctx, containerID, true)
	cancel()
	if err != nil {
		return nil, "", ContainerStateContainerUnknown, err
	}

	var ci containerdcriserver.ContainerInfo
	foundInfo := false
	if statusinfo := status.Info; statusinfo != nil {
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
		return nil, "", ContainerStateContainerUnknown, fmt.Errorf("can not find container info from CRI::ContainerStatus, containerId: %s", containerID)
	}

	labels := status.Status.Labels
	if labels == nil {
		labels = map[string]string{}
	}

	var image string
	if status.Status.Image != nil && status.Status.Image.Image != "" {
		image = status.Status.Image.Image
	} else {
		image = status.Status.ImageRef
	}

	// Judge Container Liveness by Pid
	state = status.Status.State
	stateStatus := ContainerStatusExited
	if state == ContainerStateContainerRunning && ContainerProcessAlive(int(ci.Pid)) {
		stateStatus = ContainerStatusRunning
	}
	dockerContainer := types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:      containerID,
			Created: time.Unix(0, status.Status.CreatedAt).Format(time.RFC3339Nano),
			LogPath: status.Status.LogPath,
			State: &types.ContainerState{
				Status: stateStatus,
				Pid:    int(ci.Pid),
			},
			HostConfig: &container.HostConfig{
				VolumeDriver: ci.Snapshotter,
				Runtime:      cw.runtimeInfo.RuntimeName,
				LogConfig: container.LogConfig{
					Type: "json-file",
				},
			},
		},
		Config: &container.Config{
			Labels: labels,
			Image:  image,
		},
	}

	if status.Status.Metadata != nil {
		dockerContainer.Name = status.Status.Metadata.Name
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
			dockerContainer.Mounts = append(dockerContainer.Mounts, types.MountPoint{
				Source:      mount.Source,
				Destination: mount.Destination,
				Driver:      mount.Type,
			})
		}
	}
	if ci.Snapshotter != "" && ci.SnapshotKey != "" {
		uppDir := cw.getContainerUpperDir(ci.SnapshotKey, ci.Snapshotter)
		if uppDir != "" {
			dockerContainer.GraphDriver.Data = map[string]string{
				"UpperDir": uppDir,
			}
		}
	}
	if len(hostnamePath) > 0 {
		hn, _ := os.ReadFile(GetMountedFilePath(hostnamePath))
		dockerContainer.Config.Hostname = strings.Trim(string(hn), "\t \n")
	}
	dockerContainer.HostnamePath = hostnamePath
	dockerContainer.HostsPath = hostsPath
	formatContainerJSONPath(&dockerContainer)
	return cw.containerCenter.CreateInfoDetail(dockerContainer, envConfigPrefix, false), ci.SandboxID, state, nil
}

func (cw *CRIRuntimeWrapper) fetchAll() error {
	// fetchAll and syncContainers must be isolated
	// if one procedure read container list then locked out
	// when it resumes, it may process on a staled list and make wrong decisions
	cw.containersLock.Lock()
	defer cw.containersLock.Unlock()
	ctx, cancel := context.WithTimeout(context.Background(), time.Minute)
	defer cancel()
	containersResp, err := cw.client.ListContainers(ctx)
	if err != nil {
		return err
	}
	sandboxResp, err := cw.client.ListPodSandbox(ctx)
	if err != nil {
		return err
	}
	sandboxMap := make(map[string]*CriPodSandbox, len(sandboxResp.Items))
	for _, item := range sandboxResp.Items {
		sandboxMap[item.ID] = item
	}

	allContainerMap := make(map[string]bool)           // all listable containers
	runningMap := make(map[string]bool)                // status running
	containerMap := make(map[string]*DockerInfoDetail) // pid exists
	for i, c := range containersResp.Containers {
		logger.Debugf(context.Background(), "CRIRuntime ListContainers [%v]: %+v", i, c)
		allContainerMap[c.ID] = true
		switch c.State {
		case ContainerStateContainerRunning:
			runningMap[c.ID] = true
		case ContainerStateContainerExited:
			runningMap[c.ID] = false
		default:
			continue
		}

		dockerContainer, _, _, err := cw.createContainerInfo(c.ID)
		if err != nil {
			logger.Errorf(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "Create container info from cri-runtime error, Container Info: %+v, err: %v", c, err)
			continue
		}
		if dockerContainer == nil || dockerContainer.ContainerInfo.ContainerJSONBase == nil {
			logger.Error(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "Create container info from cri-runtime error, Container Info:%+v", c)
			continue
		}
		if dockerContainer.Status() != ContainerStatusRunning {
			continue
		}
		cw.containers[c.ID] = &innerContainerInfo{
			State:  c.State,
			Pid:    dockerContainer.ContainerInfo.State.Pid,
			Name:   dockerContainer.ContainerInfo.Name,
			Status: dockerContainer.Status(),
		}
		cw.containerHistory[c.ID] = true
		containerMap[c.ID] = dockerContainer

		// append the pod labels to the k8s info.
		if sandbox, ok := sandboxMap[c.PodSandboxID]; ok {
			cw.wrapperK8sInfoByLabels(sandbox.Labels, dockerContainer)
		}
		logger.Debugf(context.Background(), "Create container info, id:%v\tname:%v\tcreated:%v\tstatus:%v\tdetail:%+v",
			dockerContainer.IDPrefix(), c.Metadata.Name, dockerContainer.ContainerInfo.Created, dockerContainer.Status(), c)
	}
	cw.containerCenter.updateContainers(containerMap)

	// delete not running containers
	for k := range cw.containers {
		if running, ok := runningMap[k]; !ok || !running {
			cw.containerCenter.markRemove(k)
			delete(cw.containers, k)
		}
	}

	// delete obsolete history
	for k := range cw.containerHistory {
		if _, ok := allContainerMap[k]; !ok {
			delete(cw.containerHistory, k)
		}
	}

	return nil
}

func (cw *CRIRuntimeWrapper) loopSyncContainers() {
	ticker := time.NewTicker(DefaultSyncContainersPeriod)
	for {
		select {
		case <-cw.stopCh:
			cw.client.Close()
			ticker.Stop()
			return
		case <-ticker.C:
			if err := cw.syncContainers(); err != nil {
				logger.Errorf(context.Background(), "SYNC_CONTAINERD_ALARM", "syncContainers error: %v", err)
			}
		}
	}
}

func (cw *CRIRuntimeWrapper) syncContainers() error {
	cw.containersLock.Lock()
	defer cw.containersLock.Unlock()
	ctx, cancel := getContextWithTimeout(time.Second * 20)
	defer cancel()
	logger.Debug(context.Background(), "cri sync containers", "begin")
	containersResp, err := cw.client.ListContainers(ctx)
	if err != nil {
		return err
	}

	newContainers := map[string]*CriContainer{}
	for i, c := range containersResp.Containers {
		// https://github.com/containerd/containerd/blob/main/pkg/cri/store/container/status.go
		// We only care RUNNING and EXITED
		// This is only an early prune, accurate status must be detected by ContainerProcessAlive
		if c.State != ContainerStateContainerRunning &&
			(c.State != ContainerStateContainerExited || c.CreatedAt < cw.listContainerStartTime) {
			continue
		}
		id := containersResp.Containers[i].ID
		newContainers[id] = containersResp.Containers[i]
		oldInfo, ok := cw.containers[id]
		_, inHistory := cw.containerHistory[id]
		if ok {
			status := ContainerStatusExited
			if oldInfo.Status == ContainerStatusRunning && ContainerProcessAlive(oldInfo.Pid) {
				status = ContainerStatusRunning
			}
			if oldInfo.State != ContainerStateContainerRunning || // not running
				(oldInfo.State == c.State && oldInfo.Name == c.Metadata.Name && oldInfo.Status == status) { // no state change
				continue
			}
		} else if inHistory {
			continue
		}
		if err := cw.fetchOne(id); err != nil {
			logger.Errorf(context.Background(), "CREATE_CONTAINERD_INFO_ALARM", "failed to createContainerInfo, containerId: %s, error: %v", id, err)
		}
	}

	// delete container
	for oldID, c := range cw.containers {
		if _, ok := newContainers[oldID]; !ok || c.State == ContainerStateContainerExited {
			logger.Debug(context.Background(), "cri sync containers remove", oldID)
			cw.containerCenter.markRemove(oldID)
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
		// bytes, _ := json.Marshal(dockerContainer)
		// logger.Debugf(context.Background(), "Create container info: %s", string(bytes))
		logger.Debugf(context.Background(), "Create container info, id:%v\tname:%v\tcreated:%v\tstatus:%v\tdetail=%+v",
			dockerContainer.IDPrefix(), dockerContainer.ContainerInfo.Name, dockerContainer.ContainerInfo.Created, dockerContainer.Status(), dockerContainer.ContainerInfo)
	}

	// cri场景下会拼接好k8s信息，然后再单个updateContainer
	cw.containerCenter.updateContainer(containerID, dockerContainer)
	cw.containerHistory[containerID] = true
	cw.containers[containerID] = &innerContainerInfo{
		status,
		dockerContainer.ContainerInfo.State.Pid,
		dockerContainer.ContainerInfo.Name,
		dockerContainer.Status(),
	}
	return nil
}

func (cw *CRIRuntimeWrapper) wrapperK8sInfoByID(sandboxID string, detail *DockerInfoDetail) {
	ctx, cancel := getContextWithTimeout(defaultContextTimeout)
	status, err := cw.client.PodSandboxStatus(ctx, sandboxID, true)
	cancel()
	if err != nil {
		logger.Debug(context.Background(), "fetchone cannot read k8s info from sandbox, sandboxID", sandboxID)
		return
	}
	cw.wrapperK8sInfoByLabels(status.Status.Labels, detail)
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
	cw.containerCenter.lock.RLock()
	for key := range cw.containerCenter.containerMap {
		usedCacheItem[key] = true
	}
	cw.containerCenter.lock.RUnlock()

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

func (cw *CRIRuntimeWrapper) lookupContainerRootfsAbsDir(info types.ContainerJSON) string {
	// For cri-runtime
	containerID := info.ID
	if dir, ok := cw.lookupRootfsCache(containerID); ok {
		return dir
	}

	// Example: /run/containerd/io.containerd.runtime.v1.linux/k8s.io/{ContainerID}/rootfs/

	var aDirs []string
	customStateDir := os.Getenv("CONTAINERD_STATE_DIR")
	if len(customStateDir) > 0 {
		// /etc/containerd/config.toml
		// state = "/home/containerd"
		// Example /home/containerd/io.containerd.runtime.v2.task/k8s.io/{ContainerID}/rootfs
		aDirs = []string{
			customStateDir,
			"/run/containerd",
			"/var/run/containerd",
		}
	} else {
		aDirs = []string{
			"/run/containerd",
			"/var/run/containerd",
		}
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

func (cw *CRIRuntimeWrapper) getContainerUpperDir(containerid, snapshotter string) string {
	// For Containerd

	if cw.nativeClient == nil {
		return ""
	}

	if dir, ok := cw.lookupRootfsCache(containerid); ok {
		return dir
	}

	si := cw.nativeClient.SnapshotService(snapshotter)
	mounts, err := si.Mounts(context.Background(), containerid)
	if err != nil {
		logger.Warning(context.Background(), "CONTAINERD_CLIENT_ALARM", "cannot get snapshot info, containerID", containerid, "errInfo", err)
		return ""
	}
	for _, m := range mounts {
		if len(m.Options) != 0 {
			for _, i := range m.Options {
				s := strings.Split(i, "=")
				if s[0] == "upperdir" {
					cw.rootfsLock.Lock()
					cw.rootfsCache[containerid] = s[1]
					cw.rootfsLock.Unlock()
					return s[1]
				}
				continue
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
