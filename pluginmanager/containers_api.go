package pluginmanager

import (
	"encoding/json"
	"path/filepath"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
)

var caCachedFullList map[string]*containercenter.DockerInfoDetail
var lastUpdateTime int64 = 0

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

type DockerFileUpdateCmd struct {
	ID              string
	Mounts          []Mount // 容器挂载路径
	UpperDir        string  // 容器默认路径
	LogPath         string  // 标准输出路径
	MetaDatas       map[string]string
	K8sInfo         K8sInfo           // 原始k8s信息
	Env             map[string]string // 环境变量信息
	ContainerLabels map[string]string // 容器标签信息
	Stopped         bool              // 容器是否已停止
}

type DockerFileUpdateCmdAll struct {
	AllCmd []DockerFileUpdateCmd
}

type DiffCmd struct {
	Update []DockerFileUpdateCmd
	Delete []string
	Stop   []string
}

func convertDockerInfos(info *containercenter.DockerInfoDetail, allCmd *DockerFileUpdateCmdAll) {
	var cmd DockerFileUpdateCmd
	cmd.ID = info.ContainerInfo.ID

	cmd.UpperDir = filepath.Clean(info.DefaultRootPath)
	cmd.LogPath = filepath.Clean(info.StdoutPath)

	// info.ContainerNameTag
	cmd.MetaDatas = make(map[string]string, len(info.ContainerNameTag))
	for key, val := range info.ContainerNameTag {
		cmd.MetaDatas[key] = val
	}

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

	if allCmd != nil {
		allCmd.AllCmd = append(allCmd.AllCmd, cmd)
	}
}

func GetAllContainers() string {
	allCmd := new(DockerFileUpdateCmdAll)
	// Snapshot current containers
	rawMap := containercenter.GetContainerMap()
	newMap := make(map[string]*containercenter.DockerInfoDetail, len(rawMap))
	for id, info := range rawMap {
		convertDockerInfos(info, allCmd)
		newMap[id] = info
	}
	caCachedFullList = newMap
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
		caCachedFullList = make(map[string]*containercenter.DockerInfoDetail)
	}
	// Snapshot current containers
	rawMap := containercenter.GetContainerMap()
	newMap := make(map[string]*containercenter.DockerInfoDetail, len(rawMap))
	for id, info := range rawMap {
		newMap[id] = info
	}

	diff := new(DiffCmd)
	// Deleted containers
	for id := range caCachedFullList {
		if c, ok := newMap[id]; !ok {
			diff.Delete = append(diff.Delete, id)
		} else if c.Status() != containercenter.ContainerStatusRunning {
			diff.Stop = append(diff.Stop, id)
		}
	}
	// Added containers -> build Update cmds
	for id, info := range newMap {
		if _, ok := caCachedFullList[id]; !ok {
			var tmpAll DockerFileUpdateCmdAll
			convertDockerInfos(info, &tmpAll)
			if len(tmpAll.AllCmd) > 0 {
				diff.Update = append(diff.Update, tmpAll.AllCmd[0])
			}
		}
	}

	// Update cache
	caCachedFullList = newMap

	wrapper := map[string]*DiffCmd{"DiffCmd": diff}
	buf, _ := json.Marshal(wrapper)
	return string(buf)
}
