package pluginmanager

import (
	"encoding/json"
	"path/filepath"

	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
)

var caCachedFullList map[string]*containercenter.DockerInfoDetail

type Mount struct {
	Source      string
	Destination string
}

type DockerFileUpdateCmd struct {
	ID        string
	MetaDatas []string // 容器信息
	Mounts    []Mount  // 容器挂载路径
	UpperDir  string   // 容器默认路径
	LogPath   string   // 标准输出路径
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
	cmd.MetaDatas = make([]string, 0, len(info.ContainerNameTag)*2)
	for key, val := range info.ContainerNameTag {
		cmd.MetaDatas = append(cmd.MetaDatas, key)
		cmd.MetaDatas = append(cmd.MetaDatas, val)
	}
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
