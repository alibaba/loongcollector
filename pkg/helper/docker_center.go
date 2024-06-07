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

package helper

import (
	"context"
	"hash/fnv"
	"path"
	"regexp"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/events"
	docker "github.com/docker/docker/client"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

var dockerCenterInstance *DockerCenter
var containerFindingManager *ContainerDiscoverManager
var onceDocker sync.Once

// set default value to aliyun_logs_
var envConfigPrefix = "aliyun_logs_"

const DockerTimeFormat = "2006-01-02T15:04:05.999999999Z"

var DefaultSyncContainersPeriod = time.Second * 3 // should be same as docker_config_update_interval gflag in C
var ContainerInfoDeletedTimeout = time.Second * time.Duration(120)
var EventListenerTimeout = time.Second * time.Duration(3600)

// "io.kubernetes.pod.name": "logtail-z2224",
// "io.kubernetes.pod.namespace": "kube-system",
// "io.kubernetes.pod.uid": "222e88ff-8f08-11e8-851d-00163f008685",
const k8sPodNameLabel = "io.kubernetes.pod.name"
const k8sPodNameSpaceLabel = "io.kubernetes.pod.namespace"
const k8sPodUUIDLabel = "io.kubernetes.pod.uid"
const k8sInnerLabelPrefix = "io.kubernetes"
const k8sInnerAnnotationPrefix = "annotation."

const (
	ContainerStatusRunning = "running"
	ContainerStatusExited  = "exited"
)

type EnvConfigInfo struct {
	ConfigName    string
	ConfigItemMap map[string]string
}

// K8SFilter used for find specific container
type K8SFilter struct {
	NamespaceReg     *regexp.Regexp
	PodReg           *regexp.Regexp
	ContainerReg     *regexp.Regexp
	IncludeLabels    map[string]string
	ExcludeLabels    map[string]string
	IncludeLabelRegs map[string]*regexp.Regexp
	ExcludeLabelRegs map[string]*regexp.Regexp
	hashKey          uint64
}

// CreateK8SFilter ...
func CreateK8SFilter(ns, pod, container string, includeK8sLabels, excludeK8sLabels map[string]string) (*K8SFilter, error) {
	var filter K8SFilter
	var err error
	var hashStrBuilder strings.Builder
	if len(ns) > 0 {
		if filter.NamespaceReg, err = regexp.Compile(ns); err != nil {
			return nil, err
		}
	}
	hashStrBuilder.WriteString(ns)
	hashStrBuilder.WriteString("$$$")
	if len(pod) > 0 {
		if filter.PodReg, err = regexp.Compile(pod); err != nil {
			return nil, err
		}
	}
	hashStrBuilder.WriteString(pod)
	hashStrBuilder.WriteString("$$$")
	if len(container) > 0 {
		if filter.ContainerReg, err = regexp.Compile(container); err != nil {
			return nil, err
		}
	}
	hashStrBuilder.WriteString(container)
	hashStrBuilder.WriteString("$$$")

	if filter.IncludeLabels, filter.IncludeLabelRegs, err = SplitRegexFromMap(includeK8sLabels); err != nil {
		return nil, err
	}
	for includeKey, val := range includeK8sLabels {
		hashStrBuilder.WriteString(includeKey)
		hashStrBuilder.WriteByte('#')
		hashStrBuilder.WriteString(val)
	}
	if filter.ExcludeLabels, filter.ExcludeLabelRegs, err = SplitRegexFromMap(excludeK8sLabels); err != nil {
		return nil, err
	}
	hashStrBuilder.WriteString("$$$")
	for excludeKey, val := range excludeK8sLabels {
		hashStrBuilder.WriteString(excludeKey)
		hashStrBuilder.WriteByte('#')
		hashStrBuilder.WriteString(val)
	}

	h := fnv.New64a()
	_, _ = h.Write([]byte(hashStrBuilder.String()))
	filter.hashKey = h.Sum64()
	return &filter, nil
}

// "io.kubernetes.container.logpath": "/var/log/pods/222e88ff-8f08-11e8-851d-00163f008685/logtail_0.log",
// "io.kubernetes.container.name": "logtail",
// "io.kubernetes.docker.type": "container",
// "io.kubernetes.pod.name": "logtail-z2224",
// "io.kubernetes.pod.namespace": "kube-system",
// "io.kubernetes.pod.uid": "222e88ff-8f08-11e8-851d-00163f008685",
type K8SInfo struct {
	Namespace       string
	Pod             string
	ContainerName   string
	Labels          map[string]string
	PausedContainer bool

	matchedCache map[uint64]bool
	mu           sync.RWMutex
}

func (info *K8SInfo) IsSamePod(o *K8SInfo) bool {
	return info.Namespace == o.Namespace && info.Pod == o.Pod
}

func (info *K8SInfo) GetLabel(key string) string {
	info.mu.RLock()
	defer info.mu.RUnlock()
	if info.Labels != nil {
		return info.Labels[key]
	}
	return ""
}

// ExtractK8sLabels only work for original docker container.
func (info *K8SInfo) ExtractK8sLabels(containerInfo types.ContainerJSON) {
	// only pause container has k8s labels
	if info.ContainerName == "POD" || info.ContainerName == "pause" {
		info.mu.Lock()
		defer info.mu.Unlock()
		info.PausedContainer = true
		if info.Labels == nil {
			info.Labels = make(map[string]string)
		}
		for key, val := range containerInfo.Config.Labels {
			if strings.HasPrefix(key, k8sInnerLabelPrefix) || strings.HasPrefix(key, k8sInnerAnnotationPrefix) {
				continue
			}
			info.Labels[key] = val
		}
	}
}

func (info *K8SInfo) Merge(o *K8SInfo) {
	info.mu.Lock()
	o.mu.Lock()
	defer info.mu.Unlock()
	defer o.mu.Unlock()

	// only pause container has k8s labels, so we can only check len(labels)
	if len(o.Labels) > len(info.Labels) {
		info.Labels = o.Labels
		info.matchedCache = nil
	}
	if len(o.Labels) < len(info.Labels) {
		o.Labels = info.Labels
		o.matchedCache = nil
	}
}

// IsMatch ...
func (info *K8SInfo) IsMatch(filter *K8SFilter) bool {
	if filter == nil {
		return true
	}
	info.mu.RLock() // 使用读锁
	isPausedContainer := info.PausedContainer
	info.mu.RUnlock() // 解读锁
	if isPausedContainer {
		return false
	}

	info.mu.Lock()         // 使用写锁
	defer info.mu.Unlock() // 解写锁
	if info.matchedCache == nil {
		info.matchedCache = make(map[uint64]bool)
	} else if cacheRst, ok := info.matchedCache[filter.hashKey]; ok {
		return cacheRst
	}
	var rst = info.innerMatch(filter)
	info.matchedCache[filter.hashKey] = rst
	return rst
}

// innerMatch ...
func (info *K8SInfo) innerMatch(filter *K8SFilter) bool {
	if filter.NamespaceReg != nil && !filter.NamespaceReg.MatchString(info.Namespace) {
		return false
	}
	if filter.PodReg != nil && !filter.PodReg.MatchString(info.Pod) {
		return false
	}
	if filter.ContainerReg != nil && !filter.ContainerReg.MatchString(info.ContainerName) {
		return false
	}
	// if labels is nil, create an empty map
	if info.Labels == nil {
		info.Labels = make(map[string]string)
	}
	return isMapLabelsMatch(filter.IncludeLabels, filter.ExcludeLabels, filter.IncludeLabelRegs, filter.ExcludeLabelRegs, info.Labels)
}

type DockerInfoDetail struct {
	StdoutPath       string
	ContainerInfo    types.ContainerJSON
	ContainerNameTag map[string]string
	K8SInfo          *K8SInfo
	EnvConfigInfoMap map[string]*EnvConfigInfo
	ContainerIP      string
	DefaultRootPath  string

	lastUpdateTime time.Time
	deleteFlag     bool
}

func (did *DockerInfoDetail) IDPrefix() string {
	return GetShortID(did.ContainerInfo.ID)
}

func (did *DockerInfoDetail) PodName() string {
	if did.K8SInfo != nil {
		return did.K8SInfo.Pod
	}
	return ""
}

func (did *DockerInfoDetail) FinishedAt() string {
	if did.ContainerInfo.State != nil {
		return did.ContainerInfo.State.FinishedAt
	}
	return ""
}

func (did *DockerInfoDetail) Status() string {
	if did.ContainerInfo.State != nil {
		return did.ContainerInfo.State.Status
	}
	return ""
}

func (did *DockerInfoDetail) IsTimeout() bool {
	nowTime := time.Now()
	if nowTime.Sub(did.lastUpdateTime) > fetchAllSuccessTimeout ||
		(did.deleteFlag && nowTime.Sub(did.lastUpdateTime) > ContainerInfoDeletedTimeout) {
		return true
	}
	return false
}

func (did *DockerInfoDetail) GetExternalTags(envs, k8sLabels map[string]string) map[string]string {
	tags := map[string]string{}
	if len(envs) == 0 && len(k8sLabels) == 0 {
		return tags
	}
	did.GetCustomExternalTags(tags, envs, k8sLabels)
	return tags
}

func (did *DockerInfoDetail) GetCustomExternalTags(tags, envs, k8sLabels map[string]string) {
	if len(envs) == 0 && len(k8sLabels) == 0 {
		return
	}
	for k, realName := range envs {
		tags[realName] = did.GetEnv(k)
	}
	if did.K8SInfo != nil {
		for k, realName := range k8sLabels {
			tags[realName] = did.K8SInfo.GetLabel(k)
		}
	}
}

func (did *DockerInfoDetail) GetEnv(key string) string {
	for _, env := range did.ContainerInfo.Config.Env {
		kvPair := strings.SplitN(env, "=", 2)
		if len(kvPair) != 2 {
			continue
		}
		if key == kvPair[0] {
			return kvPair[1]
		}
	}
	return ""
}

func (did *DockerInfoDetail) DiffName(other *DockerInfoDetail) bool {
	return did.ContainerInfo.ID != other.ContainerInfo.ID || did.ContainerInfo.Name != other.ContainerInfo.Name
}

func (did *DockerInfoDetail) DiffMount(other *DockerInfoDetail) bool {
	return len(did.ContainerInfo.Config.Volumes) != len(other.ContainerInfo.Config.Volumes)
}

func isPathSeparator(c byte) bool {
	return c == '/' || c == '\\'
}

func (did *DockerInfoDetail) FindBestMatchedPath(pth string) (sourcePath, containerPath string) {
	pth = path.Clean(pth)
	pthSize := len(pth)

	// logger.Debugf(context.Background(), "FindBestMatchedPath for container %s, target path: %s, containerInfo: %+v", did.IDPrefix(), pth, did.ContainerInfo)

	// check mounts
	var bestMatchedMounts types.MountPoint
	for _, mount := range did.ContainerInfo.Mounts {
		// logger.Debugf("container(%s-%s) mount: source-%s destination-%s", did.IDPrefix(), did.ContainerInfo.Name, mount.Source, mount.Destination)

		dst := path.Clean(mount.Destination)
		dstSize := len(dst)

		if strings.HasPrefix(pth, dst) &&
			(pthSize == dstSize || (pthSize > dstSize && isPathSeparator(pth[dstSize]))) &&
			len(bestMatchedMounts.Destination) < dstSize {
			bestMatchedMounts = mount
		}
	}
	if len(bestMatchedMounts.Source) > 0 {
		return bestMatchedMounts.Source, bestMatchedMounts.Destination
	}

	return did.DefaultRootPath, ""
}

func (did *DockerInfoDetail) MakeSureEnvConfigExist(configName string) *EnvConfigInfo {
	if did.EnvConfigInfoMap == nil {
		did.EnvConfigInfoMap = make(map[string]*EnvConfigInfo)
	}
	config, ok := did.EnvConfigInfoMap[configName]
	if !ok {
		envConfig := &EnvConfigInfo{
			ConfigName:    configName,
			ConfigItemMap: make(map[string]string),
		}
		did.EnvConfigInfoMap[configName] = envConfig
		return envConfig
	}
	return config
}

// FindAllEnvConfig find and pre process all env config, add tags for docker info
func (did *DockerInfoDetail) FindAllEnvConfig(envConfigPrefix string, selfConfigFlag bool) {

	if len(envConfigPrefix) == 0 {
		return
	}
	selfEnvConfig := false
	for _, env := range did.ContainerInfo.Config.Env {
		kvPair := strings.SplitN(env, "=", 2)
		if len(kvPair) != 2 {
			continue
		}
		key := kvPair[0]
		value := kvPair[1]

		if key == "ALICLOUD_LOG_DOCKER_ENV_CONFIG_SELF" && (value == "true" || value == "TRUE") {
			logger.Debug(context.Background(), "this container is self env config", did.ContainerInfo.Name)
			selfEnvConfig = true
			continue
		}

		if !strings.HasPrefix(key, envConfigPrefix) {
			continue
		}
		logger.Debug(context.Background(), "docker env config, name", did.ContainerInfo.Name, "item", key)
		envKey := key[len(envConfigPrefix):]
		lastIndex := strings.LastIndexByte(envKey, '_')
		var configName string
		// end with '_', invalid, just skip
		if lastIndex == len(envKey)-1 {
			continue
		}

		// raw config
		if lastIndex < 0 {
			configName = envKey
		} else {
			configName = envKey[0:lastIndex]
		}
		// invalid config name
		if len(configName) == 0 {
			continue
		}
		envConfig := did.MakeSureEnvConfigExist(configName)
		if lastIndex < 0 {
			envConfig.ConfigItemMap[""] = value
		} else {
			tailKey := envKey[lastIndex+1:]
			envConfig.ConfigItemMap[tailKey] = value
			// process tags
			if tailKey == "tags" {
				tagKV := strings.SplitN(value, "=", 2)
				// if tag exist in EnvTags, just skip this tag
				if len(tagKV) == 2 {
					if !HasEnvTags(tagKV[0], tagKV[1]) {
						did.ContainerNameTag[tagKV[0]] = tagKV[1]
					} else {
						logger.Info(context.Background(), "skip set this tag, as this exist in self env tags, key", tagKV[0], "value", tagKV[1])
					}
				} else {
					if !HasEnvTags(tagKV[0], tagKV[0]) {
						did.ContainerNameTag[tagKV[0]] = tagKV[0]
					} else {
						logger.Info(context.Background(), "skip set this tag, as this exist in self env tags, key&value", tagKV[0])
					}
				}
			}
		}
	}
	logger.Debug(context.Background(), "docker env", did.ContainerInfo.Config.Env, "prefix", envConfigPrefix, "env config", did.EnvConfigInfoMap, "self env config", selfEnvConfig)
	// ignore self env config
	if !selfConfigFlag && selfEnvConfig {
		did.EnvConfigInfoMap = make(map[string]*EnvConfigInfo)
	}
}

type DockerCenter struct {
	// ContainerMap contains all container information.
	// For the docker scenario, the container list is the same as the result executed with `docker ps` commands. So the container
	// list would also contain the sandbox containers when docker is used as an engine in Kubernetes.
	// For the CRI scenario, the container list only contains the real containers and excludes the sandbox containers. But the
	// sandbox meta would be saved to its bound container.
	containerMap                   map[string]*DockerInfoDetail // all containers will in this map
	client                         *docker.Client
	lastErrMu                      sync.Mutex
	lastErr                        error
	lock                           sync.RWMutex
	lastUpdateMapTime              int64
	eventChan                      chan events.Message
	eventChanLock                  sync.Mutex
	containerStateLock             sync.Mutex
	imageLock                      sync.RWMutex
	imageCache                     map[string]string
	initStaticContainerInfoSuccess bool
}

func getIPByHosts(hostFileName, hostname string) string {
	lines, err := util.ReadLines(hostFileName)
	if err != nil {
		logger.Info(context.Background(), "read container hosts file error, file", hostFileName, "error", err.Error())
		return ""
	}
	for _, line := range lines {
		if strings.HasPrefix(line, "#") {
			continue
		}
		if strings.Index(line, hostname) > 0 {
			line = strings.Trim(line, "#/* \t\n")
			return util.ReadFirstBlock(line)
		}
	}
	if util.GetHostName() == hostname {
		return util.GetIPAddress()
	}
	return ""
}

func (dc *DockerCenter) registerEventListener(c chan events.Message) {
	dc.eventChanLock.Lock()
	defer dc.eventChanLock.Unlock()
	dc.eventChan = c
}

func (dc *DockerCenter) unRegisterEventListener(_ chan events.Message) {
	dc.eventChanLock.Lock()
	defer dc.eventChanLock.Unlock()
	dc.eventChan = nil
}

func (dc *DockerCenter) lookupImageCache(id string) (string, bool) {
	dc.imageLock.RLock()
	defer dc.imageLock.RUnlock()
	imageName, ok := dc.imageCache[id]
	return imageName, ok
}

func (dc *DockerCenter) getImageName(id, defaultVal string) string {
	if len(id) == 0 || dc.client == nil {
		return defaultVal
	}
	if imageName, ok := dc.lookupImageCache(id); ok {
		return imageName
	}

	image, _, err := dc.client.ImageInspectWithRaw(context.Background(), id)
	logger.Debug(context.Background(), "get image name, id", id, "error", err)
	if err == nil && len(image.RepoTags) > 0 {
		dc.imageLock.Lock()
		dc.imageCache[id] = image.RepoTags[0]
		dc.imageLock.Unlock()
		return image.RepoTags[0]
	}
	return defaultVal
}

func (dc *DockerCenter) getIPAddress(info types.ContainerJSON) string {
	if detail, ok := dc.getContainerDetail(info.ID); ok && detail != nil {
		return detail.ContainerIP
	}
	if info.NetworkSettings != nil && len(info.NetworkSettings.IPAddress) > 0 {
		return info.NetworkSettings.IPAddress
	}
	if len(info.Config.Hostname) > 0 && len(info.HostsPath) > 0 {
		return getIPByHosts(GetMountedFilePath(info.HostsPath), info.Config.Hostname)
	}
	return ""
}

// CreateInfoDetail create DockerInfoDetail with docker.Container
// Container property used in this function : HostsPath, Config.Hostname, Name, Config.Image, Config.Env, Mounts
// ContainerInfo.GraphDriver.Data["UpperDir"] Config.Labels
func (dc *DockerCenter) CreateInfoDetail(info types.ContainerJSON, envConfigPrefix string, selfConfigFlag bool) *DockerInfoDetail {
	// Generate Log Tags
	containerNameTag := make(map[string]string)
	k8sInfo := K8SInfo{}
	ip := dc.getIPAddress(info)

	containerNameTag["_image_name_"] = dc.getImageName(info.Image, info.Config.Image)
	if strings.HasPrefix(info.Name, "/k8s_") || strings.HasPrefix(info.Name, "k8s_") || strings.Count(info.Name, "_") >= 4 {
		// 1. container name is k8s
		// k8s_php-redis_frontend-2337258262-154p7_default_d8a2e2dd-3617-11e7-a4b0-ecf4bbe5d414_0
		// terway_terway-multi-ip-mgslw_kube-system_b07b491e-995a-11e9-94ea-00163e080931_8
		tags := strings.SplitN(info.Name, "_", 6)
		// containerNamePrefix：k8s
		// containerName：php-redis
		// podFullName：frontend-2337258262-154p7
		// computeHash：154p7
		// deploymentName：frontend
		// replicaSetName：frontend-2337258262
		// namespace：default
		// podUID：d8a2e2dd-3617-11e7-a4b0-ecf4bbe5d414

		baseIndex := 0
		if len(tags) == 6 {
			baseIndex = 1
		}
		containerNameTag["_container_name_"] = tags[baseIndex]
		containerNameTag["_pod_name_"] = tags[baseIndex+1]
		containerNameTag["_namespace_"] = tags[baseIndex+2]
		containerNameTag["_pod_uid_"] = tags[baseIndex+3]
		k8sInfo.ContainerName = tags[baseIndex]
		k8sInfo.Pod = tags[baseIndex+1]
		k8sInfo.Namespace = tags[baseIndex+2]
		k8sInfo.ExtractK8sLabels(info)
	} else if _, ok := info.Config.Labels[k8sPodNameLabel]; ok {
		// 2. container labels has k8sPodNameLabel
		containerNameTag["_container_name_"] = info.Name
		containerNameTag["_pod_name_"] = info.Config.Labels[k8sPodNameLabel]
		containerNameTag["_namespace_"] = info.Config.Labels[k8sPodNameSpaceLabel]
		containerNameTag["_pod_uid_"] = info.Config.Labels[k8sPodUUIDLabel]
		k8sInfo.ContainerName = info.Name
		k8sInfo.Pod = info.Config.Labels[k8sPodNameLabel]
		k8sInfo.Namespace = info.Config.Labels[k8sPodNameSpaceLabel]
		// the following method is couped with the CRI adapter, only the original docker container labels
		// would be added to the labels of the k8s info.
		k8sInfo.ExtractK8sLabels(info)
	} else {
		// 3. treat as normal container
		if strings.HasPrefix(info.Name, "/") {
			containerNameTag["_container_name_"] = info.Name[1:]
		} else {
			containerNameTag["_container_name_"] = info.Name
		}
	}
	if len(ip) > 0 {
		containerNameTag["_container_ip_"] = ip
	}

	did := &DockerInfoDetail{
		StdoutPath:       info.LogPath,
		ContainerInfo:    info,
		ContainerNameTag: containerNameTag,
		K8SInfo:          &k8sInfo,
		ContainerIP:      ip,
		lastUpdateTime:   time.Now(),
	}

	// Find Env Log Configs
	did.FindAllEnvConfig(envConfigPrefix, selfConfigFlag)

	// Find Container FS Root Path on Host
	// @note for overlayfs only, some driver like nas, you can not see it in upper dir
	if info.GraphDriver.Data != nil {
		if rootPath, ok := did.ContainerInfo.GraphDriver.Data["UpperDir"]; ok {
			did.DefaultRootPath = rootPath
		}
	}
	// for cri-runtime
	if criRuntimeWrapper != nil && info.HostConfig != nil && len(did.DefaultRootPath) == 0 {
		did.DefaultRootPath = criRuntimeWrapper.lookupContainerRootfsAbsDir(info)
	}
	logger.Debugf(context.Background(), "container(id: %s, name: %s) default root path is %s", info.ID, info.Name, did.DefaultRootPath)
	return did
}

func getDockerCenterInstance() *DockerCenter {
	onceDocker.Do(func() {
		logger.Init()
		// load EnvTags first
		LoadEnvTags()
		dockerCenterInstance = &DockerCenter{}
		dockerCenterInstance.imageCache = make(map[string]string)
		dockerCenterInstance.containerMap = make(map[string]*DockerInfoDetail)
		// containerFindingManager works in a producer-consumer model
		// so even manager is not initialized, it will not affect consumers like service_stdout
		go func() {
			retryCount := 0
			containerFindingManager = NewContainerDiscoverManager()
			for {
				if containerFindingManager.Init() {
					break
				}
				if retryCount%10 == 0 {
					logger.Error(context.Background(), "DOCKER_CENTER_ALARM", "docker center init failed", "retry count", retryCount)
				}
				retryCount++
				time.Sleep(time.Second * 1)
			}
			containerFindingManager.TimerFetch()
			containerFindingManager.StartSyncContainers()
		}()
	})
	return dockerCenterInstance
}

func SetEnvConfigPrefix(prefix string) {
	envConfigPrefix = prefix
}

func (dc *DockerCenter) readStaticConfig(forceFlush bool) {
	staticDockerContainerLock.Lock()
	defer staticDockerContainerLock.Unlock()
	containerInfo, removedIDs, changed, err := tryReadStaticContainerInfo()
	if err != nil {
		logger.Warning(context.Background(), "READ_STATIC_CONFIG_ALARM", "read static container info error", err)
	}
	if !dc.initStaticContainerInfoSuccess && len(containerInfo) > 0 {
		dc.initStaticContainerInfoSuccess = true
		forceFlush = true
	}

	// 静态文件读取容器信息的时候，只能全量读取，因此使用updateContainers全量更新
	if forceFlush || changed {
		containerMap := make(map[string]*DockerInfoDetail)
		for _, info := range containerInfo {
			dockerInfoDetail := dockerCenterInstance.CreateInfoDetail(info, envConfigPrefix, false)
			containerMap[info.ID] = dockerInfoDetail
		}
		dockerCenterInstance.updateContainers(containerMap)
	}

	if len(removedIDs) > 0 {
		for _, id := range removedIDs {
			dockerCenterInstance.markRemove(id)
		}
	}
}

func (dc *DockerCenter) flushStaticConfig() {
	for {
		dc.readStaticConfig(false)
		time.Sleep(time.Second)
	}
}

func (dc *DockerCenter) setLastError(err error, msg string) {
	dc.lastErrMu.Lock()
	dc.lastErr = err
	dc.lastErrMu.Unlock()
	if err != nil {
		logger.Warning(context.Background(), "DOCKER_CENTER_ALARM", "message", msg, "error found", err)
	} else {
		logger.Debug(context.Background(), "message", msg)
	}
}

func isMapLabelsMatch(includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	labels map[string]string) bool {
	if len(includeLabel) != 0 || len(includeLabelRegex) != 0 {
		matchedFlag := false
		for key, val := range includeLabel {
			if dockerVal, ok := labels[key]; ok && (len(val) == 0 || dockerVal == val) {
				matchedFlag = true
				break
			}
		}
		// if matched, do not need check regex
		if !matchedFlag {
			for key, reg := range includeLabelRegex {
				if dockerVal, ok := labels[key]; ok && reg.MatchString(dockerVal) {
					matchedFlag = true
					break
				}
			}
		}

		if !matchedFlag {
			return false
		}
	}
	for key, val := range excludeLabel {
		if dockerVal, ok := labels[key]; ok && (len(val) == 0 || dockerVal == val) {
			return false
		}
	}
	for key, reg := range excludeLabelRegex {
		if dockerVal, ok := labels[key]; ok && reg.MatchString(dockerVal) {
			return false
		}
	}
	return true
}

func isContainerLabelMatch(includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	info *DockerInfoDetail) bool {
	return isMapLabelsMatch(includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, info.ContainerInfo.Config.Labels)
}

func isMathEnvItem(env string,
	staticEnv map[string]string,
	regexEnv map[string]*regexp.Regexp) bool {
	var envKey, envValue string
	splitArray := strings.SplitN(env, "=", 2)
	if len(splitArray) < 2 {
		envKey = splitArray[0]
	} else {
		envKey = splitArray[0]
		envValue = splitArray[1]
	}

	if len(staticEnv) > 0 {
		if value, ok := staticEnv[envKey]; ok && (len(value) == 0 || value == envValue) {
			return true
		}
	}

	if len(regexEnv) > 0 {
		if reg, ok := regexEnv[envKey]; ok && reg.MatchString(envValue) {
			return true
		}
	}
	return false
}

func isContainerEnvMatch(includeEnv map[string]string,
	excludeEnv map[string]string,
	includeEnvRegex map[string]*regexp.Regexp,
	excludeEnvRegex map[string]*regexp.Regexp,
	info *DockerInfoDetail) bool {

	if len(includeEnv) != 0 || len(includeEnvRegex) != 0 {
		matchFlag := false
		for _, env := range info.ContainerInfo.Config.Env {
			if isMathEnvItem(env, includeEnv, includeEnvRegex) {
				matchFlag = true
				break
			}
		}
		if !matchFlag {
			return false
		}
	}

	if len(excludeEnv) != 0 || len(excludeEnvRegex) != 0 {
		for _, env := range info.ContainerInfo.Config.Env {
			if isMathEnvItem(env, excludeEnv, excludeEnvRegex) {
				return false
			}
		}
	}

	return true
}

func (dc *DockerCenter) getAllAcceptedInfo(
	includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	includeEnv map[string]string,
	excludeEnv map[string]string,
	includeEnvRegex map[string]*regexp.Regexp,
	excludeEnvRegex map[string]*regexp.Regexp,
	k8sFilter *K8SFilter,
) map[string]*DockerInfoDetail {
	containerMap := make(map[string]*DockerInfoDetail)
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	for id, info := range dc.containerMap {
		if isContainerLabelMatch(includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, info) &&
			isContainerEnvMatch(includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, info) &&
			info.K8SInfo.IsMatch(k8sFilter) {
			containerMap[id] = info
		}
	}
	return containerMap
}

func (dc *DockerCenter) getAllAcceptedInfoV2(
	fullList map[string]bool,
	matchList map[string]*DockerInfoDetail,
	includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	includeEnv map[string]string,
	excludeEnv map[string]string,
	includeEnvRegex map[string]*regexp.Regexp,
	excludeEnvRegex map[string]*regexp.Regexp,
	k8sFilter *K8SFilter,
) (newCount, delCount int, matchAddedList, matchDeletedList []string) {

	dc.lock.RLock()
	defer dc.lock.RUnlock()
	matchDeletedList = make([]string, 0)
	matchAddedList = make([]string, 0)
	// Remove deleted containers from match list and full list.
	delCount = 0

	for id := range fullList {
		if _, exist := dc.containerMap[id]; !exist {
			delete(fullList, id)
			if _, matched := matchList[id]; matched {
				delete(matchList, id)
				matchDeletedList = append(matchDeletedList, id)
				delCount++
			}
		}
	}

	// Update matched container status
	for id := range matchList {
		c, ok := dc.containerMap[id]
		if ok {
			matchList[id] = c
		} else {
			logger.Warningf(context.Background(), "DOCKER_MATCH_ALARM", "matched container not in docker center")
		}
	}

	// Add new containers to full list and matched to match list.
	newCount = 0

	for id, info := range dc.containerMap {
		if _, exist := fullList[id]; !exist {
			fullList[id] = true
			if isContainerLabelMatch(includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, info) &&
				isContainerEnvMatch(includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, info) &&
				info.K8SInfo.IsMatch(k8sFilter) {
				newCount++
				matchList[id] = info
				matchAddedList = append(matchAddedList, id)
			}
		}
	}
	return newCount, delCount, matchAddedList, matchDeletedList
}

func (dc *DockerCenter) getDiffContainers(fullList map[string]struct{}) (fullAddedList, fullDeletedList []string) {
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	fullDeletedList = make([]string, 0)
	fullAddedList = make([]string, 0)
	for id := range fullList {
		if _, exist := dc.containerMap[id]; !exist {
			delete(fullList, id)
			fullDeletedList = append(fullDeletedList, id)
		}
	}
	for id := range dc.containerMap {
		if _, exist := fullList[id]; !exist {
			fullList[id] = struct{}{}
			fullAddedList = append(fullAddedList, id)
		}
	}
	return fullAddedList, fullDeletedList
}

func (dc *DockerCenter) getAllSpecificInfo(filter func(*DockerInfoDetail) bool) (infoList []*DockerInfoDetail) {
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	for _, info := range dc.containerMap {
		if filter(info) {
			infoList = append(infoList, info)
		}
	}
	return infoList
}

func (dc *DockerCenter) processAllContainerInfo(processor func(*DockerInfoDetail)) {
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	for _, info := range dc.containerMap {
		processor(info)
	}
}

func (dc *DockerCenter) getContainerDetail(id string) (containerDetail *DockerInfoDetail, ok bool) {
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	containerDetail, ok = dc.containerMap[id]
	return
}

func (dc *DockerCenter) getLastUpdateMapTime() int64 {
	return atomic.LoadInt64(&dc.lastUpdateMapTime)
}

func (dc *DockerCenter) refreshLastUpdateMapTime() {
	atomic.StoreInt64(&dc.lastUpdateMapTime, time.Now().UnixNano())
}

func (dc *DockerCenter) updateContainers(containerMap map[string]*DockerInfoDetail) {
	dc.lock.Lock()
	defer dc.lock.Unlock()
	for key, container := range dc.containerMap {
		// check removed keys
		if _, ok := containerMap[key]; !ok {
			if !container.IsTimeout() {
				// not timeout, put to new map
				containerMap[key] = container
			}
		}
	}
	// switch to new container map
	if logger.DebugFlag() {
		for i, c := range containerMap {
			logger.Debugf(context.Background(), "Update all containers [%v]: id:%v\tname:%v\tcreated:%v\tstatus:%v detail=%+v",
				i, c.IDPrefix(), c.ContainerInfo.Name, c.ContainerInfo.Created, c.Status(), c.ContainerInfo)
		}
	}
	dc.containerMap = containerMap
	dc.mergeK8sInfo()
	dc.refreshLastUpdateMapTime()
}

func (dc *DockerCenter) mergeK8sInfo() {
	k8sInfoMap := make(map[string][]*K8SInfo)
	for _, container := range dc.containerMap {
		if container.K8SInfo == nil {
			continue
		}
		key := container.K8SInfo.Namespace + "@" + container.K8SInfo.Pod
		k8sInfoMap[key] = append(k8sInfoMap[key], container.K8SInfo)
	}
	for key, k8sInfo := range k8sInfoMap {
		if len(k8sInfo) < 2 {
			logger.Debug(context.Background(), "k8s pod's container count < 2", key)
			continue
		}
		// @note we need test pod with many sidecar containers
		for i := 1; i < len(k8sInfo); i++ {
			k8sInfo[0].Merge(k8sInfo[i])
		}
		for i := 1; i < len(k8sInfo); i++ {
			k8sInfo[i].Merge(k8sInfo[0])
		}
	}
}

func (dc *DockerCenter) updateContainer(id string, container *DockerInfoDetail) {
	dc.lock.Lock()
	defer dc.lock.Unlock()
	if container.K8SInfo != nil {
		if _, ok := dc.containerMap[id]; !ok {
			for _, oldContainer := range dc.containerMap {
				if oldContainer.K8SInfo != nil && oldContainer.K8SInfo.IsSamePod(container.K8SInfo) {
					oldContainer.K8SInfo.Merge(container.K8SInfo)
				}
			}
		}
	}
	if logger.DebugFlag() {
		// bytes, _ := json.Marshal(container)
		// logger.Debug(context.Background(), "update container info", string(bytes))
		logger.Debugf(context.Background(), "Update one container: id:%v\tname:%v\tcreated:%v\tstatus:%v detail=%+v",
			container.IDPrefix(), container.ContainerInfo.Name, container.ContainerInfo.Created, container.Status(), container.ContainerInfo)
	}
	dc.containerMap[id] = container
	dc.refreshLastUpdateMapTime()
}

func (dc *DockerCenter) fetchAll() error {
	dc.containerStateLock.Lock()
	defer dc.containerStateLock.Unlock()
	containers, err := dc.client.ContainerList(context.Background(), types.ContainerListOptions{All: true})
	if err != nil {
		dc.setLastError(err, "list container error")
		return err
	}
	logger.Debug(context.Background(), "fetch all", containers)
	var containerMap = make(map[string]*DockerInfoDetail)

	for _, container := range containers {
		var containerDetail types.ContainerJSON
		for idx := 0; idx < 3; idx++ {
			if containerDetail, err = dc.client.ContainerInspect(context.Background(), container.ID); err == nil {
				break
			}
			time.Sleep(time.Second * 5)
		}
		if err == nil {
			if !ContainerProcessAlive(containerDetail.State.Pid) {
				containerDetail.State.Status = ContainerStatusExited
				finishedAt := containerDetail.State.FinishedAt
				finishedAtTime, _ := time.Parse(time.RFC3339, finishedAt)
				now := time.Now()
				duration := now.Sub(finishedAtTime)
				if duration >= ContainerInfoDeletedTimeout {
					continue
				}
			}
			containerMap[container.ID] = dc.CreateInfoDetail(containerDetail, envConfigPrefix, false)
		} else {
			dc.setLastError(err, "inspect container error "+container.ID)
		}
	}
	dc.updateContainers(containerMap)

	return err
}

func (dc *DockerCenter) fetchOne(containerID string, tryFindSandbox bool) error {
	dc.containerStateLock.Lock()
	defer dc.containerStateLock.Unlock()
	containerDetail, err := dc.client.ContainerInspect(context.Background(), containerID)
	if err != nil {
		dc.setLastError(err, "inspect container error "+containerID)
		return err
	}
	if containerDetail.State.Status == ContainerStatusRunning && !ContainerProcessAlive(containerDetail.State.Pid) {
		containerDetail.State.Status = ContainerStatusExited
	}
	// docker 场景下
	// tryFindSandbox如果是false, 那么fetchOne的地方应该会调用两次，一次是sandbox的id，一次是业务容器的id
	// tryFindSandbox如果是true, 调用的地方只会有一个业务容器的id，然后依赖fetchOne内部把sandbox信息补全
	dc.updateContainer(containerID, dc.CreateInfoDetail(containerDetail, envConfigPrefix, false))
	logger.Debug(context.Background(), "update container", containerID, "detail", containerDetail)
	if tryFindSandbox && containerDetail.Config != nil {
		if id := containerDetail.Config.Labels["io.kubernetes.sandbox.id"]; id != "" {
			containerDetail, err = dc.client.ContainerInspect(context.Background(), id)
			if err != nil {
				dc.setLastError(err, "inspect sandbox container error "+id)
			} else {
				if containerDetail.State.Status == ContainerStatusRunning && !ContainerProcessAlive(containerDetail.State.Pid) {
					containerDetail.State.Status = ContainerStatusExited
				}
				dc.updateContainer(id, dc.CreateInfoDetail(containerDetail, envConfigPrefix, false))
				logger.Debug(context.Background(), "update sandbox container", id, "detail", containerDetail)
			}
		}
	}
	return err
}

// We mark container removed if it is exited or its metadata cannot be accessed
// e.g. cannot docker inspect / crictl inspect it.
func (dc *DockerCenter) markRemove(containerID string) {
	dc.lock.Lock()
	defer dc.lock.Unlock()
	if container, ok := dc.containerMap[containerID]; ok {
		if container.deleteFlag {
			return
		}
		logger.Debugf(context.Background(), "mark remove container: id:%v\tname:%v\tcreated:%v\tstatus:%v detail=%+v",
			container.IDPrefix(), container.ContainerInfo.Name, container.ContainerInfo.Created, container.Status(), container.ContainerInfo)
		container.ContainerInfo.State.Status = ContainerStatusExited
		container.deleteFlag = true
		container.lastUpdateTime = time.Now()
		dc.refreshLastUpdateMapTime()
	}
}

func (dc *DockerCenter) cleanTimeoutContainer() {
	dc.lock.Lock()
	defer dc.lock.Unlock()
	hasDelete := false
	for key, container := range dc.containerMap {
		// Comfirm to delete:
		// 1. The container is marked deleted for a while.
		// 2. The time of last success fetch all is too old.
		if container.IsTimeout() {
			logger.Debugf(context.Background(), "delete container, id:%v\tname:%v\tcreated:%v\tstatus:%v\tdetail:%+v",
				container.IDPrefix(), container.ContainerInfo.Name, container.ContainerInfo.Created, container.Status(), container.ContainerInfo)
			delete(dc.containerMap, key)
			hasDelete = true
		}
	}
	if hasDelete {
		dc.refreshLastUpdateMapTime()
	}
}

func (dc *DockerCenter) sweepCache() {
	// clear unuseful cache
	usedImageIDSet := make(map[string]bool)
	dc.lock.RLock()
	for _, container := range dc.containerMap {
		usedImageIDSet[container.ContainerInfo.Image] = true
	}
	dc.lock.RUnlock()

	dc.imageLock.Lock()

	for key := range dc.imageCache {
		if _, ok := usedImageIDSet[key]; !ok {
			delete(dc.imageCache, key)
		}
	}
	dc.imageLock.Unlock()
}

func dockerCenterRecover() {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "docker center runtime error", err, "stack", string(trace))
	}
}

func (dc *DockerCenter) initClient() error {
	var err error
	// DockerCenterTimeout should only be used by context.WithTimeout() in specific methods
	if dc.client, err = CreateDockerClient(); err != nil {
		dc.setLastError(err, "init docker client from env error")
		return err
	}
	return nil
}

func (dc *DockerCenter) eventListener() {
	errorCount := 0
	defer dockerCenterRecover()
	timer := time.NewTimer(EventListenerTimeout)
	var err error
	for {
		logger.Info(context.Background(), "docker event listener", "start")
		ctx, cancel := context.WithCancel(context.Background())
		events, errors := dc.client.Events(ctx, types.EventsOptions{})
		breakFlag := false
		for !breakFlag {
			timer.Reset(EventListenerTimeout)
			select {
			case event, ok := <-events:
				if !ok {
					logger.Errorf(context.Background(), "DOCKER_EVENT_ALARM", "docker event listener stop")
					errorCount++
					breakFlag = true
					break
				}
				logger.Debug(context.Background(), "docker event captured", event)
				errorCount = 0
				switch event.Status {
				case "start", "restart":
					_ = dc.fetchOne(event.ID, false)
				case "rename":
					_ = dc.fetchOne(event.ID, false)
				case "die":
					dc.markRemove(event.ID)
				default:
				}
				dc.eventChanLock.Lock()
				if dc.eventChan != nil {
					// no block insert
					select {
					case dc.eventChan <- event:
					default:
						logger.Error(context.Background(), "DOCKER_EVENT_ALARM", "event queue is full, miss event", event)
					}
				}
				dc.eventChanLock.Unlock()
			case err = <-errors:
				logger.Error(context.Background(), "DOCKER_EVENT_ALARM", "docker event listener error", err)
				breakFlag = true
			case <-timer.C:
				logger.Errorf(context.Background(), "DOCKER_EVENT_ALARM", "no docker event in 1 hour. Reset event listener")
				breakFlag = true
			}
		}
		cancel()
		if errorCount > 10 && criRuntimeWrapper != nil {
			logger.Info(context.Background(), "docker listener fails and cri runtime wrapper is valid", "stop docker listener")
			break
		}
		// if always error, sleep 300 secs
		if errorCount > 30 {
			time.Sleep(time.Duration(300) * time.Second)
		} else {
			time.Sleep(time.Duration(10) * time.Second)
		}
	}
	dc.setLastError(err, "docker event stream closed")
}
