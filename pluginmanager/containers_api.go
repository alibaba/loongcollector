package pluginmanager

import (
	"encoding/json"
	"path/filepath"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
)

var caCachedFullList map[string]struct{}
var lastUpdateTime int64

type Mount struct {
	Source      string
	Destination string
}

type K8sInfo struct {
	Namespace       string
	Pod             string
	ContainerName   string
	Labels          map[string]string
	PausedContainer bool
}

type ContainerInfoCmd struct {
	ID              string
	Name            string
	Mounts          []Mount // 容器挂载路径
	UpperDir        string  // 容器默认路径
	LogPath         string  // 标准输出路径
	MetaDatas       map[string]string
	K8sInfo         K8sInfo           // 原始k8s信息
	Env             map[string]string // 环境变量信息
	ContainerLabels map[string]string // 容器标签信息
	Stopped         bool              // 容器是否已停止
	Status          string            // 容器状态
}

type AllCmd struct {
	All []ContainerInfoCmd
}

type DiffCmd struct {
	Update []ContainerInfoCmd
	Delete []string
	Stop   []string
}

func convertDockerInfos(info *containercenter.DockerInfoDetail, cmds *[]ContainerInfoCmd) {
	var cmd ContainerInfoCmd
	cmd.ID = info.ContainerInfo.ID

	cmd.UpperDir = filepath.Clean(info.DefaultRootPath)
	cmd.LogPath = filepath.Clean(info.StdoutPath)

	// info.ContainerNameTag
	cmd.MetaDatas = make(map[string]string, len(info.ContainerNameTag))
	for key, val := range info.ContainerNameTag {
		cmd.MetaDatas[key] = val
	}
	cmd.Name = info.ContainerInfo.Name
	cmd.Status = info.ContainerInfo.State.Status

	// K8s info
	if info.K8SInfo != nil {
		cmd.K8sInfo = K8sInfo{
			Namespace:       info.K8SInfo.Namespace,
			Pod:             info.K8SInfo.Pod,
			ContainerName:   info.K8SInfo.ContainerName,
			Labels:          make(map[string]string),
			PausedContainer: info.K8SInfo.PausedContainer,
		}
		if info.K8SInfo.Labels != nil {
			for k, v := range info.K8SInfo.Labels {
				cmd.K8sInfo.Labels[k] = v
			}
		}
	} else {
		cmd.K8sInfo = K8sInfo{
			Labels: make(map[string]string),
		}
	}

	// Environment variables
	cmd.Env = make(map[string]string)
	if info.ContainerInfo.Config != nil && info.ContainerInfo.Config.Env != nil {
		for _, env := range info.ContainerInfo.Config.Env {
			parts := strings.SplitN(env, "=", 2)
			if len(parts) == 2 {
				cmd.Env[parts[0]] = parts[1]
			}
		}
	}

	// Container labels
	cmd.ContainerLabels = make(map[string]string)
	if info.ContainerInfo.Config != nil && info.ContainerInfo.Config.Labels != nil {
		for k, v := range info.ContainerInfo.Config.Labels {
			cmd.ContainerLabels[k] = v
		}
	}

	// Container stopped status
	cmd.Stopped = false
	if info.ContainerInfo.State != nil {
		status := info.ContainerInfo.State.Status
		cmd.Stopped = status == "exited" || status == "dead" || status == "removing"
	}

	// Mounts
	cmd.Mounts = make([]Mount, 0, len(info.ContainerInfo.Mounts))
	for _, mount := range info.ContainerInfo.Mounts {
		cmd.Mounts = append(cmd.Mounts, Mount{
			Source:      filepath.Clean(mount.Source),
			Destination: filepath.Clean(mount.Destination),
		})
	}
	*cmds = append(*cmds, cmd)
}

func GetAllContainers() string {
	allCmd := new(AllCmd)
	allCmd.All = make([]ContainerInfoCmd, 0)
	// Snapshot current containers
	rawMap := containercenter.GetContainerMapCopy()
	if len(rawMap) == 0 {
		return ""
	}
	caCachedFullList = make(map[string]struct{})
	for _, info := range rawMap {
		convertDockerInfos(info, &allCmd.All)
		caCachedFullList[info.ContainerInfo.ID] = struct{}{}
	}
	cmdBuf, _ := json.Marshal(allCmd)
	return string(cmdBuf)
}

func GetDiffContainers() string {
	newUpdateTime := containercenter.GetContainersLastUpdateTime()
	if lastUpdateTime != 0 {
		if lastUpdateTime >= newUpdateTime {
			return ""
		}
	}

	if caCachedFullList == nil {
		caCachedFullList = make(map[string]struct{})
	}
	lastUpdateTime = newUpdateTime
	update, delete, stop, changed, newFullList := containercenter.GetContainerDiffForPluginManager(caCachedFullList)
	if !changed {
		return ""
	}
	diff := new(DiffCmd)
	diff.Update = make([]ContainerInfoCmd, 0)
	for _, info := range update {
		convertDockerInfos(info, &diff.Update)
	}
	diff.Delete = delete
	diff.Stop = stop
	caCachedFullList = newFullList

	buf, _ := json.Marshal(diff)
	return string(buf)
}
